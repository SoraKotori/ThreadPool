#include <coroutine>
#include <iostream>

#include <coroutine>
#include <iostream>
#include <stdexcept>
#include <thread>
 
auto switch_to_new_thread(std::jthread& out)
{
    struct awaitable
    {
        std::jthread* p_out;
        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<> h)
        {
            std::jthread& out = *p_out;
            if (out.joinable())
                throw std::runtime_error("Output jthread parameter not empty");
            out = std::jthread([h] { h.resume(); });
            // Potential undefined behavior: accessing potentially destroyed *this
            // std::cout << "New thread ID: " << p_out->get_id() << '\n';
            std::cout << "New thread ID: " << out.get_id() << '\n'; // this is OK
        }
        void await_resume() {}
    };
    return awaitable{&out};
}
 
struct task
{
    struct promise_type
    {
        task get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
};
 
task resuming_on_new_thread(std::jthread& out)
{
    std::cout << "Coroutine started on thread: " << std::this_thread::get_id() << '\n';
    co_await switch_to_new_thread(out);
    // awaiter destroyed here
    std::cout << "Coroutine resumed on thread: " << std::this_thread::get_id() << '\n';
}



struct Task_promise_type;

// 自定義 coroutine 類別
class Task : public std::coroutine_handle<Task_promise_type> {
public:
    using promise_type = Task_promise_type;
};

struct Task_promise_type {
    // 取得 coroutine handle 的返回對象
    Task get_return_object() {
        return { Task::from_promise(*this) };
    }
    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
};

Task coroutine_example()
{
    std::cout << "Coroutine started\n";
    co_await std::suspend_always{};  // 讓 coroutine 掛起
    std::cout << "Coroutine resumed\n";
    co_return;
}

int main() {


    // 啟動 coroutine 並取得 handle
    auto handle = coroutine_example();
    
    std::cout << "Coroutine is now suspended\n";

    // 在 main 中控制 coroutine 的恢復
    std::cout << "Resuming coroutine...\n";
    handle.resume();  // 恢復 coroutine

    // 檢查 coroutine 是否完成
    if (handle.done()) {
        std::cout << "Coroutine has finished\n";
    }

    // 清理 handle
    handle.destroy();

    std::jthread out;
    auto thread_task = resuming_on_new_thread(out);

    return 0;
}
