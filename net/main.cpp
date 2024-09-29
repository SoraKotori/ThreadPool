#include <asio.hpp>
#include <iostream>
#include <coroutine>

asio::awaitable<void> handle_socket(asio::ip::tcp::socket socket) {
    char data[1024];
    std::size_t length = co_await socket.async_read_some(asio::buffer(data), asio::use_awaitable);
    co_await asio::async_write(socket, asio::buffer(data, length), asio::use_awaitable);
    socket.close();
}

int main() {
    asio::io_context io_context;
    asio::co_spawn(io_context, [&io_context]() -> asio::awaitable<void> {
        auto acceptor = asio::ip::tcp::acceptor(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 8888));
        while (true) {
            auto socket = co_await acceptor.async_accept(asio::use_awaitable);
            asio::co_spawn(io_context, handle_socket(std::move(socket)), asio::detached);
        }
    }, asio::detached);

    io_context.run();
}
