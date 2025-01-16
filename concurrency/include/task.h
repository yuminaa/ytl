#pragma once

#include <functional>
#include <thread>
#include <variant>

namespace ytl
{
    class task
    {
    public:
        enum class type
        {
            FUNCTION,
            THREAD
        };

        enum class state
        {
            CREATED,
            SUSPENDED,
            RUNNING,
            COMPLETED,
            CANCELLED
        };

        using executable = std::variant<std::function<void()>, std::shared_ptr<std::thread> >;

        /**
         * @brief Spawn a new task immediately
         * @tparam Fn Function or lambda type
         * @tparam Args Argument types
         * @param fn Function to execute
         * @param args Arguments to the function
         * @return Task representing the spawned execution unit
         */
        template<typename Fn, typename... Args>
        static task spawn(Fn &&fn, Args &&... args)
        {
            task task;
            task.type = type::FUNCTION;
            task.state.store(state::RUNNING, std::memory_order_relaxed);
            task.execu = [fn = std::forward<Fn>(fn), ... args = std::forward<Args>(args)]() mutable
            {
                fn(std::forward<Args>(args)...);
            };
            task.execute();
            return task;
        }

        /**
         * @brief Spawn a new thread task
         * @param thread Thread to manage
         * @return Task representing the thread
         */
        static task spawn(const std::shared_ptr<std::thread> &thread);

        template<typename Fn, typename... Args>
        static task defer(Fn &&fn, Args &&... args)
        {
            return delay(0.0f, std::forward<Fn>(fn), std::forward<Args>(args)...);
        }

        /**
        * @brief Defer a thread task
        * @param thread Thread to defer
        * @return Deferred Task
        */
        static task defer(std::variant<std::function<void()>, std::shared_ptr<std::thread> > thread);

        template<typename Fn, typename... Args>
        static task delay(float duration, Fn &&fn, Args &&... args)
        {
            task task;
            task.type = type::FUNCTION;
            task.state.store(state::SUSPENDED, std::memory_order_relaxed);
            task.execu = [duration, fn = std::forward<Fn>(fn), ...args = std::forward<Args>(args)]() mutable
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(duration * 1000)));
                fn(std::forward<Args>(args)...);
            };
            task.execute();
            return task;
        }

        /**
         * @brief Delay a thread task
         * @param duration Delay in seconds
         * @param thread Thread to delay
         * @return Delayed Task
         */
        static task delay(float duration, const std::shared_ptr<std::thread> &thread);

        static void desynchronize();

        static void synchronize();

        static float wait(float duration);

        static void cancel(task &task);

        task();

        task(const task &) = delete;

        task &operator=(const task &) = delete;

        task(task &&other) noexcept;

        task &operator=(task &&other) noexcept;

    private:
        static inline std::atomic<uint64_t> next_id = 0;

        std::atomic<state> state;
        uint64_t id;
        type type;
        executable execu;

        static std::mutex mutex;
        static bool serial;

        void execute() const;
    };
}
