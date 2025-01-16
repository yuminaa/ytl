#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
#include <netinet/in.h>

namespace ytl
{
    inline constexpr size_t MAX_BUFFERED_PACKETS = 8;
    inline constexpr size_t DEFAULT_PACKET_SIZE = 1024;
    static constexpr size_t MAX_UDP_PACKET = 576;

    enum class UDP_STATE : uint8_t
    {
        FREE,
        PENDING,
        ACTIVE,
        ERROR
    };

    enum class ERROR : uint16_t
    {
        NONE,
        SOCKET_CREATION_FAILED,
        SEND_FAILED,
        BIND_FAILED,
        CONNECT_FAILED,
        ASYNCIFY_FAILED,
        GETADDRINFO_FAILED,
        RECV_FAILED,
        POLL_FAILED,
        FAILED_TO_READ_IO_LEN,
        INVALID_ADDR
    };

    enum class SEND_RESULT : uint8_t
    {
        OK,
        BUFFERED,
        FAILED
    };

    struct alignas(4) IPv4Addr
    {
        union
        {
            uint32_t addr_long;
            std::array<uint8_t, 4> addr;
        };
    };

    struct alignas(16) Address
    {
        union
        {
            char storage[28];
            uint16_t family;
        };

        int32_t addr_len { 0 };

        [[nodiscard]] bool valid() const noexcept
        {
            return addr_len > 0;
        }

        [[nodiscard]] bool operator==(const Address &other) const noexcept;
    };

    struct alignas(32) PacketData
    {
        void *data { nullptr };
        Address *to { nullptr };
        uint16_t len { 0 };
        uint16_t max_len { 0 };
    };

    struct alignas(32) ConnectionData
    {
        int socket { -1 };
        void *recv_buffer { nullptr };
        uint32_t max_packet_size { DEFAULT_PACKET_SIZE };
        uint32_t data_len { 0 };
        Address from {};
        ERROR error { ERROR::NONE };
        UDP_STATE state { UDP_STATE::FREE };
    };

    class UDPPool
    {
    public:
        explicit UDPPool(uint32_t port = 0);

        ~UDPPool();

        UDPPool(const UDPPool &) = delete;

        UDPPool &operator=(const UDPPool &) = delete;

        UDPPool(UDPPool &&) noexcept = default;

        UDPPool &operator=(UDPPool &&) noexcept = default;

        [[nodiscard]] uint32_t create(std::string_view host, std::string_view port, bool connect = false);

        uint32_t serve(uint16_t port);

        SEND_RESULT send(uint32_t conn_id, std::span<const std::byte> data);

        SEND_RESULT send_to(uint32_t conn_id, const Address &addr, std::span<const std::byte> data);

        bool recv(uint32_t conn_id);

        bool recv_from(uint32_t conn_id);

        void update();

        void close(uint32_t conn_id);

        [[nodiscard]] UDP_STATE get_state(uint32_t conn_id) const
        {
            return conns_[conn_id].state;
        }

        [[nodiscard]] ERROR get_error(uint32_t conn_id) const
        {
            return conns_[conn_id].error;
        }

        [[nodiscard]] std::span<const std::byte> get_data(uint32_t conn_id) const;

        [[nodiscard]] const Address &get_from(const uint32_t conn_id) const
        {
            return conns_[conn_id].from;
        }

        void recv_from_all(std::span<const uint32_t> conn_ids);

        void send_to_all(std::span<const uint32_t> conn_ids, std::span<const std::byte> data);

        static Address resolve_host(std::string_view hostname, std::string_view port, bool ipv4 = true);

        ERROR send_oneshot(std::string_view host, std::string_view port, std::span<const std::byte> data);

    private:
        static constexpr size_t MAX_CONNECTIONS = 1024;

        std::array<ConnectionData, MAX_CONNECTIONS> conns_ {};
        std::vector<uint32_t> free_indices_;

        struct PacketQueue
        {
            std::array<PacketData, MAX_BUFFERED_PACKETS> packets {};
            uint32_t length { 0 };
            std::vector<Address> addr_storage;
            std::vector<std::byte> data_storage;
        } packet_queue_;

        uint32_t alloc_slot();

        void free_slot(uint32_t index);

        bool try_flush_queue(uint32_t conn_id);

        SEND_RESULT queue_packet(const Address *addr, std::span<const std::byte> data);

        void init_socket(int sock);
    };

    namespace util
    {
        constexpr std::string_view error_string(ERROR error) noexcept;

        constexpr bool is_error(const SEND_RESULT result) noexcept
        {
            return result == SEND_RESULT::FAILED;
        }
    }
}
