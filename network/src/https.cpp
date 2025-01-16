#include "../include/https.h"

namespace ytl
{
    RequestPool::RequestPool()
    {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        multi_handle = curl_multi_init();

        free_indices.reserve(MAX_CONCURRENT_REQUESTS);
        for (uint32_t i = 0; i < MAX_CONCURRENT_REQUESTS; ++i)
        {
            free_indices.push_back(MAX_CONCURRENT_REQUESTS - i - 1);
            curl_handles[i] = curl_easy_init();
        }

        urls.reserve(INITIAL_BUFFER_SIZE);
        responses.reserve(INITIAL_BUFFER_SIZE);
        headers.reserve(INITIAL_BUFFER_SIZE);
    }

    RequestPool::~RequestPool()
    {
        for (auto *handle: curl_handles)
        {
            if (handle)
                curl_easy_cleanup(handle);
        }
        if (multi_handle)
            curl_multi_cleanup(multi_handle);
        curl_global_cleanup();
    }

    RequestHandle RequestPool::create(std::string_view url, HTTPS_METHOD method)
    {
        uint32_t index = alloc_slot();
        auto &req = requests[index];

        req.url_offset = urls.size();
        req.url_length = url.length();
        urls.insert(urls.end(), url.begin(), url.end());

        req.state = HTTPS_STATE::PENDING;
        req.error = ERROR::NONE;
        req.status_code = 0;
        req.data_offset = 0;
        req.data_length = 0;

        req.curl = curl_handles[next_curl_handle++];
        if (next_curl_handle >= MAX_CONCURRENT_REQUESTS)
            next_curl_handle = 0;

        init_curl_handle(req.curl, req);

        return RequestHandle { index, generation };
    }

    void RequestPool::set_headers(RequestHandle handle, std::span<const char * const> headers)
    {
        if (!valid_handle(handle))
            return;
        auto &req = requests[handle.index];

        curl_slist *header_list = nullptr;
        for (const char *header: headers)
            header_list = curl_slist_append(header_list, header);

        curl_easy_setopt(req.curl, CURLOPT_HTTPHEADER, header_list);
    }

    void RequestPool::set_post_data(RequestHandle handle, std::span<const std::byte> data)
    {
        if (!valid_handle(handle))
            return;
        auto &req = requests[handle.index];

        curl_easy_setopt(req.curl, CURLOPT_POSTFIELDS, data.data());
        curl_easy_setopt(req.curl, CURLOPT_POSTFIELDSIZE, data.size());
    }

    void RequestPool::start(RequestHandle handle)
    {
        if (!valid_handle(handle))
            return;
        auto &req = requests[handle.index];

        curl_multi_add_handle(multi_handle, req.curl);
        ++active_count;
    }

    void RequestPool::update()
    {
        int still_running = 0;
        curl_multi_perform(multi_handle, &still_running);

        int msgs_left = 0;
        while (auto *msg = curl_multi_info_read(multi_handle, &msgs_left))
        {
            if (msg->msg == CURLMSG_DONE)
            {
                for (auto &req: requests)
                {
                    if (req.curl == msg->easy_handle)
                    {
                        req.state = msg->data.result == CURLE_OK ? HTTPS_STATE::COMPLETE : HTTPS_STATE::ERROR;

                        if (req.state == HTTPS_STATE::ERROR)
                        {
                            req.error = util::curl_to_error(msg->data.result);
                        }

                        curl_multi_remove_handle(multi_handle, req.curl);
                        --active_count;
                        break;
                    }
                }
            }
        }
    }

    HTTPS_STATE RequestPool::get_state(RequestHandle handle) const
    {
        return valid_handle(handle) ? requests[handle.index].state : HTTPS_STATE::ERROR;
    }

    ERROR RequestPool::get_error(RequestHandle handle) const
    {
        return valid_handle(handle) ? requests[handle.index].error : ERROR::INTERNAL_ERROR;
    }

    uint32_t RequestPool::get_status_code(RequestHandle handle) const
    {
        return valid_handle(handle) ? requests[handle.index].status_code : 0;
    }

    std::span<const std::byte> RequestPool::get_response(RequestHandle handle) const
    {
        if (!valid_handle(handle))
            return {};
        const auto &[state, error, status_code, url_offset, url_length, data_offset, data_length, curl, padding] =
                requests[handle.index];
        return std::span(
            responses.data() + data_offset,
            data_length
        );
    }

    void RequestPool::start_batch(std::span<const RequestHandle> handles)
    {
        for (auto handle: handles)
            start(handle);
    }

    void RequestPool::update_batch(std::span<RequestHandle> handles)
    {
        for (auto handle: handles)
        {
            if (get_state(handle) == HTTPS_STATE::PENDING)
            {
                update();
                break;
            }
        }
    }

    uint32_t RequestPool::alloc_slot()
    {
        if (free_indices.empty())
        {
            generation++;
            for (uint32_t i = 0; i < MAX_CONCURRENT_REQUESTS; ++i)
            {
                if (requests[i].state == HTTPS_STATE::FREE)
                {
                    free_indices.push_back(i);
                }
            }
        }

        uint32_t index = free_indices.back();
        free_indices.pop_back();
        return index;
    }

    void RequestPool::free_slot(uint32_t index)
    {
        requests[index].state = HTTPS_STATE::FREE;
        free_indices.push_back(index);
    }

    bool RequestPool::valid_handle(RequestHandle handle) const
    {
        return handle.index < MAX_CONCURRENT_REQUESTS &&
               handle.generation == generation &&
               requests[handle.index].state != HTTPS_STATE::FREE;
    }

    size_t RequestPool::write_cb(char *ptr, const size_t size, const size_t nmemb, void *userdata)
    {
        auto *req = static_cast<RequestData *>(userdata);
        size_t real_size = size * nmemb;

        auto *response_ptr = reinterpret_cast<std::byte *>(ptr);
        responses.insert(
            responses.begin() + req->data_offset + req->data_length,
            response_ptr,
            response_ptr + real_size
        );
        req->data_length += real_size;

        return real_size;
    }

    void RequestPool::init_curl_handle(CURL *curl, RequestData &req)
    {
        curl_easy_reset(curl);

        curl_easy_setopt(curl, CURLOPT_URL, urls.data() + req.url_offset);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &req);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, REQUEST_TIMEOUT_MS);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    }

    namespace util
    {
        std::string_view error_string(ERROR error) noexcept
        {
            switch (error)
            {
                case ERROR::NONE:
                    return "No error";
                case ERROR::INVALID_URL:
                    return "Invalid URL";
                case ERROR::CONNECTION_FAILED:
                    return "Connection failed";
                case ERROR::TIMEOUT:
                    return "Request timed out";
                case ERROR::OUT_OF_MEMORY:
                    return "Out of memory";
                case ERROR::SSL_ERROR:
                    return "SSL error";
                case ERROR::NETWORK_ERROR:
                    return "Network error";
                case ERROR::INTERNAL_ERROR:
                    return "Internal error";
                default:
                    return "Unknown error";
            }
        }

        ERROR curl_to_error(const CURLcode code) noexcept
        {
            switch (code)
            {
                case CURLE_OK:
                    return ERROR::NONE;
                case CURLE_URL_MALFORMAT:
                    return ERROR::INVALID_URL;
                case CURLE_COULDNT_CONNECT:
                    return ERROR::CONNECTION_FAILED;
                case CURLE_OPERATION_TIMEDOUT:
                    return ERROR::TIMEOUT;
                case CURLE_OUT_OF_MEMORY:
                    return ERROR::OUT_OF_MEMORY;
                case CURLE_SSL_CONNECT_ERROR:
                    return ERROR::SSL_ERROR;
                case CURLE_COULDNT_RESOLVE_HOST:
                    return ERROR::NETWORK_ERROR;
                default:
                    return ERROR::INTERNAL_ERROR;
            }
        }

        bool is_success(const uint32_t code) noexcept
        {
            return code >= 200 && code < 300;
        }
    }
}
