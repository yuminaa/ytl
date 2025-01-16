#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <curl/curl.h>

namespace ytl
{
    inline constexpr size_t MAX_CONCURRENT_REQUESTS = 512;
    inline constexpr size_t INITIAL_BUFFER_SIZE = 4096;
    inline constexpr uint32_t REQUEST_TIMEOUT_MS = 30000;

    enum class HTTPS_STATE : uint8_t
    {
        FREE,
        PENDING,
        COMPLETE,
        ERROR
    };

    enum class HTTPS_METHOD : uint8_t
    {
        GET,
        POST,
        PUT,
        DELETE,
        HEAD,
        OPTIONS
    };

    enum class ERROR
    {
        NONE,
        INVALID_URL,
        CONNECTION_FAILED,
        TIMEOUT,
        OUT_OF_MEMORY,
        SSL_ERROR,
        NETWORK_ERROR,
        INTERNAL_ERROR
    };

    struct RequestHandle
    {
        uint32_t index;
        uint32_t generation;
    };

    struct alignas(32) RequestData
    {
        HTTPS_STATE state;
        ERROR error;
        uint32_t status_code;
        uint32_t url_offset;
        uint32_t url_length;
        uint32_t data_offset;
        uint32_t data_length;
        CURL *curl;
        char padding[4];
    };

    class RequestPool
    {
    public:
        RequestPool();

        ~RequestPool();

        RequestPool(const RequestPool &) = delete;

        RequestPool &operator=(const RequestPool &) = delete;

        RequestPool(RequestPool &&) noexcept = default;

        RequestPool &operator=(RequestPool &&) noexcept = default;

        RequestHandle create(std::string_view url, HTTPS_METHOD method = HTTPS_METHOD::GET);

        void set_headers(RequestHandle handle, std::span<const char * const> headers);

        void set_post_data(RequestHandle handle, std::span<const std::byte> data);

        void start(RequestHandle handle);

        void update();

        [[nodiscard]] HTTPS_STATE get_state(RequestHandle handle) const;

        [[nodiscard]] ERROR get_error(RequestHandle handle) const;

        [[nodiscard]] uint32_t get_status_code(RequestHandle handle) const;

        [[nodiscard]] std::span<const std::byte> get_response(RequestHandle handle) const;

        void start_batch(std::span<const RequestHandle> handles);

        void update_batch(std::span<RequestHandle> handles);

    private:
        std::array<RequestData, MAX_CONCURRENT_REQUESTS> requests {};
        std::vector<uint32_t> free_indices;
        std::vector<char> urls;
        std::vector<std::byte> responses;
        std::vector<char> headers;
        CURLM *multi_handle;
        uint32_t generation {};
        uint32_t active_count {};

        std::array<CURL *, MAX_CONCURRENT_REQUESTS> curl_handles {};
        uint32_t next_curl_handle {};

        uint32_t alloc_slot();

        void free_slot(uint32_t index);

        [[nodiscard]] bool valid_handle(RequestHandle handle) const;

        size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata);

        void init_curl_handle(CURL *curl, RequestData &req);
    };

    namespace util
    {
        std::string_view error_string(ERROR error) noexcept;

        ERROR curl_to_error(CURLcode code) noexcept;

        bool is_success(uint32_t code) noexcept;
    }
}
