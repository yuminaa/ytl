#include "../include/udp.h"
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

namespace ytl
{
    bool Address::operator==(const Address &other) const noexcept
    {
        return addr_len == other.addr_len &&
               std::memcmp(storage, other.storage, addr_len) == 0;
    }

    UDPPool::UDPPool(const uint32_t port)
    {
        free_indices_.reserve(MAX_CONNECTIONS);
        for (uint32_t i = 0; i < MAX_CONNECTIONS; ++i)
        {
            free_indices_.push_back(MAX_CONNECTIONS - i - 1);
        }

        packet_queue_.addr_storage.reserve(MAX_BUFFERED_PACKETS);
        packet_queue_.data_storage.reserve(DEFAULT_PACKET_SIZE * MAX_BUFFERED_PACKETS);

        if (port > 0)
            serve(port);
    }

    UDPPool::~UDPPool()
    {
        for (auto &[socket, recv_buffer, max_packet_size, data_len, from, error, state]: conns_)
        {
            if (state != UDP_STATE::FREE)
            {
                close(socket);
                std::free(recv_buffer);
            }
        }
    }

    uint32_t UDPPool::create(const std::string_view host, const std::string_view port, const bool connect)
    {
        const uint32_t index = alloc_slot();
        if (index == MAX_CONNECTIONS)
        {
            return MAX_CONNECTIONS;
        }

        auto &conn = conns_[index];
        conn.recv_buffer = std::malloc(DEFAULT_PACKET_SIZE);
        conn.max_packet_size = DEFAULT_PACKET_SIZE;

        const auto addr = resolve_host(host, port);
        if (!addr.valid())
        {
            conn.error = ERROR::INVALID_ADDR;
            return index;
        }

        conn.socket = socket(addr.family, SOCK_DGRAM, IPPROTO_UDP);
        if (conn.socket == -1)
        {
            conn.error = ERROR::SOCKET_CREATION_FAILED;
            return index;
        }

        if (connect)
        {
            if (::connect(conn.socket, reinterpret_cast<const sockaddr *>(addr.storage),
                          addr.addr_len) < 0)
            {
                conn.error = ERROR::CONNECT_FAILED;
                return index;
            }
        }
        else
        {
            if (bind(conn.socket, reinterpret_cast<const sockaddr *>(addr.storage),
                     addr.addr_len) < 0)
            {
                conn.error = ERROR::BIND_FAILED;
                return index;
            }
        }

        init_socket(conn.socket);
        conn.from = addr;
        conn.state = UDP_STATE::ACTIVE;
        return index;
    }

    uint32_t UDPPool::serve(uint16_t port)
    {
        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        Address bind_addr {};
        std::memcpy(bind_addr.storage, &addr, sizeof(addr));
        bind_addr.addr_len = sizeof(addr);

        const uint32_t index = alloc_slot();
        if (index == MAX_CONNECTIONS)
        {
            return MAX_CONNECTIONS;
        }

        auto &conn = conns_[index];
        conn.recv_buffer = std::malloc(DEFAULT_PACKET_SIZE);
        conn.max_packet_size = DEFAULT_PACKET_SIZE;

        conn.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (conn.socket == -1)
        {
            conn.error = ERROR::SOCKET_CREATION_FAILED;
            return index;
        }

        if (bind(conn.socket, reinterpret_cast<const sockaddr *>(bind_addr.storage),
                 bind_addr.addr_len) < 0)
        {
            conn.error = ERROR::BIND_FAILED;
            return index;
        }

        init_socket(conn.socket);
        conn.state = UDP_STATE::ACTIVE;
        return index;
    }

    bool UDPPool::recv(const uint32_t conn_id)
    {
        auto &conn = conns_[conn_id];
        if (conn.error != ERROR::NONE || conn.state != UDP_STATE::ACTIVE)
        {
            return false;
        }

        if (ioctl(conn.socket, FIONREAD, &conn.data_len) < 0)
        {
            conn.error = ERROR::FAILED_TO_READ_IO_LEN;
            return false;
        }

        if (!conn.data_len)
        {
            return false;
        }

        if (conn.data_len > conn.max_packet_size)
        {
            const auto new_buf = std::realloc(conn.recv_buffer, conn.data_len);
            if (!new_buf)
            {
                conn.error = ERROR::FAILED_TO_READ_IO_LEN;
                return false;
            }
            conn.recv_buffer = new_buf;
            conn.max_packet_size = conn.data_len;
        }

        const ssize_t received = ::recv(conn.socket, conn.recv_buffer, conn.data_len, 0);
        if (received < 0)
        {
            conn.error = ERROR::RECV_FAILED;
            conn.data_len = 0;
            return false;
        }

        conn.data_len = received;
        return true;
    }

    bool UDPPool::recv_from(const uint32_t conn_id)
    {
        auto &conn = conns_[conn_id];
        if (conn.error != ERROR::NONE || conn.state != UDP_STATE::ACTIVE)
        {
            return false;
        }

        if (ioctl(conn.socket, FIONREAD, &conn.data_len) < 0)
        {
            conn.error = ERROR::FAILED_TO_READ_IO_LEN;
            return false;
        }

        if (!conn.data_len)
        {
            return false;
        }

        if (conn.data_len > conn.max_packet_size)
        {
            const auto new_buf = std::realloc(conn.recv_buffer, conn.data_len);
            if (!new_buf)
            {
                conn.error = ERROR::FAILED_TO_READ_IO_LEN;
                return false;
            }
            conn.recv_buffer = new_buf;
            conn.max_packet_size = conn.data_len;
        }

        conn.from.addr_len = sizeof(conn.from.storage);
        const ssize_t received = recvfrom(conn.socket, conn.recv_buffer, conn.data_len, 0,
                                          reinterpret_cast<sockaddr *>(&conn.from.storage),
                                          reinterpret_cast<socklen_t *>(&conn.from.addr_len));

        if (received < 0)
        {
            conn.error = ERROR::RECV_FAILED;
            conn.data_len = 0;
            return false;
        }

        conn.data_len = received;
        return true;
    }

    void UDPPool::update()
    {
        for (uint32_t i = 0; i < MAX_CONNECTIONS; ++i)
        {
            if (auto&[socket, recv_buffer, max_packet_size, data_len, from, error, state] = conns_[i];
                state != UDP_STATE::ACTIVE)
                continue;

            try_flush_queue(i);
            // TODO: Check dead connections & disconnect it
        }
    }

    SEND_RESULT UDPPool::send(const uint32_t conn_id, const std::span<const std::byte> data)
    {
        auto &[socket, recv_buffer, max_packet_size, data_len, from, error, state] = conns_[conn_id];
        if (error != ERROR::NONE || state != UDP_STATE::ACTIVE)
        {
            return SEND_RESULT::FAILED;
        }

        if (try_flush_queue(conn_id))
        {
            return queue_packet(nullptr, data);
        }

        if (ssize_t sent = ::send(socket, data.data(), data.size(), 0);
            sent < 0)
        {
            if (errno == EWOULDBLOCK)
            {
                return queue_packet(nullptr, data);
            }
            error = ERROR::SEND_FAILED;
            return SEND_RESULT::FAILED;
        }

        return SEND_RESULT::OK;
    }

    SEND_RESULT UDPPool::send_to(const uint32_t conn_id, const Address &addr,
                                 const std::span<const std::byte> data)
    {
        auto &[socket, recv_buffer, max_packet_size, data_len, from, error, state] = conns_[conn_id];
        if (error != ERROR::NONE || state != UDP_STATE::ACTIVE)
        {
            return SEND_RESULT::FAILED;
        }

        if (try_flush_queue(conn_id))
        {
            return queue_packet(&addr, data);
        }

        const ssize_t sent = sendto(socket, data.data(), data.size(), 0,
                                    reinterpret_cast<const sockaddr *>(addr.storage),
                                    addr.addr_len);

        if (sent < 0)
        {
            if (errno == EWOULDBLOCK)
            {
                return queue_packet(&addr, data);
            }
            error = ERROR::SEND_FAILED;
            return SEND_RESULT::FAILED;
        }

        return SEND_RESULT::OK;
    }

    void UDPPool::recv_from_all(const std::span<const uint32_t> conn_ids)
    {
        std::vector<pollfd> fds;
        fds.reserve(conn_ids.size());

        for (const uint32_t id: conn_ids)
        {
            auto &[socket, recv_buffer, max_packet_size, data_len, from, error, state] = conns_[id];
            if (error != ERROR::NONE || state != UDP_STATE::ACTIVE)
                continue;
            
            fds.push_back(pollfd {
                .fd = socket,
                .events = POLLIN,
                .revents = 0
            });
        }

        if (poll(fds.data(), fds.size(), 0) < 0)
        {
            for (uint32_t id: conn_ids)
            {
                conns_[id].error = ERROR::POLL_FAILED;
            }
            return;
        }

        for (size_t i = 0; i < fds.size(); ++i)
        {
            if (fds[i].revents & POLLIN)
            {
                recv_from(conn_ids[i]);
            }
        }
    }

    void UDPPool::send_to_all(std::span<const uint32_t> conn_ids,
                              std::span<const std::byte> data)
    {
        for (const uint32_t id: conn_ids)
        {
            send(id, data);
        }
    }

    uint32_t UDPPool::alloc_slot()
    {
        if (free_indices_.empty())
        {
            return MAX_CONNECTIONS;
        }
        uint32_t index = free_indices_.back();
        free_indices_.pop_back();
        return index;
    }

    void UDPPool::free_slot(const uint32_t index) // NOLINT(*-no-recursion)
    {
        if (auto &conn = conns_[index];
            conn.state != UDP_STATE::FREE)
        {
            close(conn.socket);
            std::free(conn.recv_buffer);
            conn = ConnectionData {};
            free_indices_.push_back(index);
        }
    }

    bool UDPPool::try_flush_queue(const uint32_t conn_id)
    {
        auto &[packets, length, addr_storage, data_storage] = packet_queue_;
        if (length == 0 || conns_[conn_id].error != ERROR::NONE)
        {
            return false;
        }

        const size_t start = length > MAX_BUFFERED_PACKETS ? length - MAX_BUFFERED_PACKETS : 0;

        for (size_t i = start; i < length; ++i)
        {
            auto &[data, to, len, max_len] = packets[i & (MAX_BUFFERED_PACKETS - 1)];
            if (len == 0)
                continue;

            ssize_t ret;
            if (to && to->addr_len > 0)
            {
                ret = sendto(conns_[conn_id].socket,
                             data,
                             len, 0,
                             reinterpret_cast<const sockaddr *>(to->storage),
                             to->addr_len);
            }
            else
            {
                ret = ::send(conns_[conn_id].socket,
                           data,
                           len, 0);
            }

            if (ret < 0)
            {
                if (errno == EWOULDBLOCK)
                {
                    return true;
                }
                conns_[conn_id].error = ERROR::SEND_FAILED;
                return false;
            }
            len = 0;
        }

        length = 0;
        return false;
    }

    SEND_RESULT UDPPool::queue_packet(const Address *addr,
                                      const std::span<const std::byte> data)
    {
        auto &[packets, length, addr_storage, data_storage] = packet_queue_;
        auto &packet = packets[length++ & (MAX_BUFFERED_PACKETS - 1)];

        if (packet.max_len < data.size())
        {
            packet.data = std::realloc(packet.data, data.size()); // NOLINT(*-suspicious-realloc-usage)
            if (!packet.data)
            {
                return SEND_RESULT::FAILED;
            }
            packet.max_len = data.size();
        }

        std::memcpy(packet.data, data.data(), data.size());
        packet.len = data.size();

        if (addr)
        {
            if (!packet.to)
            {
                packet.to = &addr_storage.emplace_back();
            }
            *packet.to = *addr;
        }
        else if (packet.to)
        {
            packet.to->addr_len = 0;
        }

        return SEND_RESULT::BUFFERED;
    }

    void UDPPool::init_socket(int sock)
    {
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags >= 0)
        {
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        }
        else
        {
            conns_[sock].error = ERROR::ASYNCIFY_FAILED;
        }
    }

    void UDPPool::close(const uint32_t conn_id) // NOLINT(*-no-recursion)
    {
        free_slot(conn_id);
    }

    std::span<const std::byte> UDPPool::get_data(const uint32_t conn_id) const
    {
        const auto &conn = conns_[conn_id];
        return {
            static_cast<const std::byte *>(conn.recv_buffer),
            conn.data_len
        };
    }

    Address UDPPool::resolve_host(const std::string_view hostname, const std::string_view port, const bool ipv4)
    {
        const addrinfo hints {
            .ai_family = ipv4 ? AF_INET : AF_INET6,
            .ai_socktype = SOCK_DGRAM,
            .ai_protocol = IPPROTO_UDP,
            .ai_flags = AI_PASSIVE
        };

        addrinfo *result = nullptr;
        if (getaddrinfo(hostname.data(), port.data(), &hints, &result) != 0)
        {
            return Address {};
        }

        Address addr {};
        std::memcpy(&addr.storage, result->ai_addr, result->ai_addrlen);
        addr.addr_len = static_cast<int32_t>(result->ai_addrlen);

        freeaddrinfo(result);
        return addr;
    }

    ERROR UDPPool::send_oneshot(const std::string_view host, const std::string_view port,
                                const std::span<const std::byte> data)
    {
        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == -1)
        {
            return ERROR::SOCKET_CREATION_FAILED;
        }

        auto addr = resolve_host(host, port, true);
        if (!addr.valid())
        {
            close(sock);
            return ERROR::INVALID_ADDR;
        }

        ssize_t sent = sendto(sock, data.data(), data.size(), 0,
                              reinterpret_cast<const sockaddr *>(addr.storage),
                              addr.addr_len);

        close(sock);
        return sent < 0 ? ERROR::SEND_FAILED : ERROR::NONE;
    }

    namespace util
    {
        constexpr std::string_view error_string(ERROR error) noexcept
        {
            switch (error)
            {
                case ERROR::NONE:
                    return "No error";
                case ERROR::SOCKET_CREATION_FAILED:
                    return "Failed to create socket";
                case ERROR::SEND_FAILED:
                    return "Failed to send packet";
                case ERROR::BIND_FAILED:
                    return "Failed to bind socket";
                case ERROR::CONNECT_FAILED:
                    return "Failed to connect";
                case ERROR::ASYNCIFY_FAILED:
                    return "Failed to make socket non-blocking";
                case ERROR::GETADDRINFO_FAILED:
                    return "Failed to resolve address";
                case ERROR::RECV_FAILED:
                    return "Failed to receive packet";
                case ERROR::POLL_FAILED:
                    return "Failed to poll sockets";
                case ERROR::FAILED_TO_READ_IO_LEN:
                    return "Failed to read IO length";
                case ERROR::INVALID_ADDR:
                    return "Invalid address";
                default:
                    return "Unknown error";
            }
        }
    }
}
