#include "../include/task.h"

namespace ytl
{
    std::mutex task::mutex;
    bool task::serial = true;

    task::task() : state(state::CREATED), id(next_id.fetch_add(1, std::memory_order_relaxed)), type(type::FUNCTION) {}

    task::task(task &&other) noexcept : id(other.id),
                                        type(other.type),
                                        execu(std::move(other.execu))
    {
        state.store(other.state.load(std::memory_order_relaxed), std::memory_order_relaxed);
        other.id = 0;
        other.state.store(state::CREATED, std::memory_order_relaxed);
    }

    task &task::operator=(task &&other) noexcept
    {
        if (this != &other)
        {
            id = other.id;
            type = other.type;
            execu = std::move(other.execu);
            state.store(other.state.load(std::memory_order_relaxed), std::memory_order_relaxed);
            other.id = 0;
            other.state.store(state::CREATED, std::memory_order_relaxed);
        }
        return *this;
    }

    task task::spawn(const std::shared_ptr<std::thread> &thread)
    {
        task task;
        task.type = type::THREAD;
        task.state.store(state::RUNNING, std::memory_order_relaxed);
        task.execu = thread;
        task.execute();
        return task;
    }

    task task::defer(std::variant<std::function<void()>, std::shared_ptr<std::thread>> thread)
    {
        task task;
        task.type = type::THREAD;
        task.state.store(state::SUSPENDED, std::memory_order_relaxed);
        task.execu = std::move(thread);
        return task;
    }

    task task::delay(const float duration, const std::shared_ptr<std::thread> &thread)
    {
        task task;
        task.type = type::THREAD;
        task.state.store(state::SUSPENDED, std::memory_order_relaxed);
        task.execu = thread;
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(duration * 1000)));
        task.execute();
        return task;
    }

    void task::desynchronize()
    {
        serial = false;
    }

    void task::synchronize()
    {
        serial = true;
    }

    float task::wait(float duration)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(duration * 1000)));
        return duration;
    }

    void task::cancel(task &task)
    {
        if (task.state.load(std::memory_order_relaxed) != state::CANCELLED)
        {
            task.state.store(state::CANCELLED, std::memory_order_relaxed);
            if (std::holds_alternative<std::shared_ptr<std::thread>>(task.execu))
            {
                if (const auto &thread = std::get<std::shared_ptr<std::thread>>(task.execu);
                    thread->joinable())
                {
                    thread->detach(); // TODO: Proper cancellation & cleanup
                }
            }
        }
    }

    void task::execute() const
    {
        if (state.load(std::memory_order_relaxed) == state::CANCELLED)
            return;

        if (serial)
        {
            std::lock_guard lock(mutex);
            if (std::holds_alternative<std::function<void()>>(execu))
            {
                std::get<std::function<void()>>(execu)();
            }
            else if (std::holds_alternative<std::shared_ptr<std::thread>>(execu))
            {
                if (auto &thread = std::get<std::shared_ptr<std::thread>>(execu);
                    thread->joinable())
                {
                    thread->join();
                }
            }
        }
        else
        {
            if (std::holds_alternative<std::function<void()>>(execu))
            {
                std::get<std::function<void()>>(execu)();
            }
            else if (std::holds_alternative<std::shared_ptr<std::thread>>(execu))
            {
                if (auto &thread = std::get<std::shared_ptr<std::thread>>(execu);
                    thread->joinable())
                {
                    thread->join();
                }
            }
        }
    }
}

