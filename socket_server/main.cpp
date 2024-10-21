#include <cerrno>
#include <cstring>

#include <string_view>
#include <memory>
#include <iostream>
#include <vector>
#include <expected>
#include <map>
#include <optional>
#include <cassert>

#include <sys/socket.h>
#include <netdb.h>

#include <sys/epoll.h>

class file_descriptor : public std::optional<int>
{
public:
    file_descriptor(int fd) :
        std::optional<int>(std::in_place, fd) {}

    ~file_descriptor()
    {
        if (std::optional<int>::operator bool())
            if (::close(std::optional<int>::operator*()) == -1)
                std::cerr << std::error_code(errno, std::system_category());
    }

    file_descriptor(const file_descriptor&) = delete;
    file_descriptor& operator=(const file_descriptor&) = delete;

    file_descriptor(file_descriptor&& other)
    {
        // a moved-from std::optional still contains a value, but the value itself is moved from.
        std::optional<int>::swap(other);
    }

    operator int&()
    {
        return std::optional<int>::operator*();
    }
};

class epoll
{
public:
    static std::expected<epoll, std::error_code> create()
    {
        int epfd = ::epoll_create1(0);
        if (epfd == -1) {
            return std::unexpected(std::error_code(errno, std::system_category()));
        }

        return epoll(epfd);
    }

    epoll(epoll&& other) noexcept :
        m_epfd(other.m_epfd),
        m_fds(std::move(other.m_fds))
    {
        other.m_epfd = -1; // 將原對象的文件描述符設置為無效
    }

    auto insert_or_close(file_descriptor fd, struct epoll_event& event)
    {
        if (::epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &event) == -1)
        {
            std::cerr << std::error_code(errno, std::system_category());
            // std::println(std::cerr, "epoll_ctl: {}.", std::strerror(errno));

            return false;
        }

        auto [iterator, is_inserted] = m_fds.try_emplace(int(fd), nullptr);
        if (!is_inserted)
        {
            std::cerr << "m_fds.try_emplace Number of fd inserted: 0.\n";

            return false;
        }

        fd.reset();
        return is_inserted;
    }

    auto erase_and_close(int fd)
    {
        if (::epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr) == -1)
            std::cerr << std::error_code(errno, std::system_category());

        if (::close(fd) == -1)
            std::cerr << std::error_code(errno, std::system_category());

        if (auto is_removed = m_fds.erase(fd); !is_removed)
            std::cerr << "m_fds.erase Number of fd removed: 0.\n";
    }

    std::expected<bool, std::error_code> listen(const char *__restrict name,
			                                     const char *__restrict service,
			                                     const struct addrinfo *__restrict req,
                                                 int backlog)
    {
        std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> result(nullptr, ::freeaddrinfo);

        // getaddrinfo() returns 0 if it succeeds, or the nonzero error codes
        if (int status = ::getaddrinfo(name, service, req, std::out_ptr(result)))
        {
            std::println(std::cerr, "getaddrinfo: {}.", ::gai_strerror(status)); // !
            return std::unexpected(std::error_code(status, std::system_category())); // !
        }

        for (auto result_ptr = result.get(); result_ptr; result_ptr = result_ptr->ai_next)
        {
            // AF_INET is 2, AF_INET6 is 10
            file_descriptor listen_fd = ::socket(result_ptr->ai_family,
                                                 result_ptr->ai_socktype,
                                                 result_ptr->ai_protocol);
            if (listen_fd == -1)
            {
                std::cerr << std::error_code(errno, std::system_category());
                continue;
            }

            if (::bind(listen_fd, result_ptr->ai_addr,
                                  result_ptr->ai_addrlen) == -1)
            {
                std::cerr << std::error_code(errno, std::system_category());
                // std::println(std::cerr, "bind: {}.", std::strerror(errno));

                continue;
            }

            if (::listen(listen_fd, backlog) == -1)
            {
                std::cerr << std::error_code(errno, std::system_category());
                // std::println(std::cerr, "listen: {}.", std::strerror(errno));

                continue;
            }

            struct epoll_event event = {
                .events = EPOLLIN,
                .data = { .fd = listen_fd }
            };

            // Transfer ownership of listen_fd to the insert_or_close function.
            insert_or_close(std::move(listen_fd), event);

            // listen_fd is now empty after the ownership has been moved
            assert(!listen_fd.has_value());
        }

        return true;
    }

    std::expected<bool, std::error_code> wait(std::vector<struct epoll_event> events, int timeout)
    {
        int events_size = ::epoll_wait(m_epfd, std::data(events), std::size(events), timeout);
        if (events_size == -1)
            return std::unexpected(std::error_code(errno, std::system_category()));

        for (int events_index = 0; events_index < events_size; events_index++)
        {
            auto event_flag = events[events_index].events;
            auto socket_fd  = events[events_index].data.fd;

            if (event_flag & EPOLLERR)
            {
                std::println(std::cerr, "EPOLLERR, fd: {}.", socket_fd);
                continue;
            }

            if (event_flag != EPOLLIN)
            {
                std::println(std::clog, "event_flag != EPOLLIN: {}.", event_flag);
                continue;
            }

            int is_listen;
            socklen_t len = sizeof(is_listen);
            if (::getsockopt(socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &is_listen, &len) == -1)
            {
                std::cerr << std::error_code(errno, std::system_category());
                // std::println(std::cout, "getsockopt: {}.", std::strerror(errno));
                continue;
            }

            if (is_listen) // listen socket
            {
                sockaddr_storage addr_storage;
                sockaddr& addr = reinterpret_cast<sockaddr&>(addr_storage);
                socklen_t addr_len = sizeof(sockaddr_storage);

                int accept_fd = ::accept(socket_fd, &addr, &addr_len);
                if (accept_fd == -1)
                {
                    std::cerr << std::error_code(errno, std::system_category());
                    // std::println(std::cerr, "accept: {}.", std::strerror(errno));
                    continue;
                }

                std::array<char, 1024> receive_host;
                std::array<char, 1024> receive_port;
                if (int status = ::getnameinfo(&addr, addr_len,
                                               std::data(receive_host), std::size(receive_host),
                                               std::data(receive_port), std::size(receive_port), 0))
                {
                    std::println(std::cerr, "getnameinfo: {}.", gai_strerror(status));
                    continue;
                }

                // print host and port
                std::println(std::clog, "accept from {}:{}.", std::data(receive_host), std::data(receive_port));

                struct epoll_event event = {
                    .events = EPOLLIN | EPOLLRDHUP,
                    .data = { .fd = accept_fd }
                };

                insert_or_close(accept_fd, event);
            }
            else // accept socket
            {
                std::array<char, 1024 * 16> buffer;

                auto receive_size = ::recv(socket_fd, std::data(buffer), std::size(buffer), 0);
                if (receive_size == -1)
                {
                    std::cerr << std::error_code(errno, std::system_category());
                    // std::println(std::cerr, "recv error: {}.", errno, std::strerror(errno));
                    continue;
                }

                if (receive_size == 0)
                {
                    erase_and_close(socket_fd);
                    continue;
                }

                if (receive_size != ::send(socket_fd, std::data(buffer), receive_size, 0))
                    std::println(std::cerr, "Error sending response");
                    
                std::string_view receive{std::data(buffer), static_cast<std::size_t>(receive_size)};
                std::println(std::clog, "{}", receive);
            }

            if (event_flag == EPOLLHUP ||
                event_flag == EPOLLRDHUP)
                erase_and_close(socket_fd);
        }

        return true;
    }

    ~epoll()
    {
        if (m_epfd != -1)
            if (::close(m_epfd) == -1)
                std::cerr << std::error_code(errno, std::system_category());

        for (auto [fd, ptr] : m_fds)
            if (::close(fd) == -1)
                std::cerr << std::error_code(errno, std::system_category());
    }

private:
    epoll(int epfd) :
        m_epfd(epfd) {}

    int m_epfd;
    std::map<int, void*> m_fds;
};

int main(int argc, char* argv[])
{
    auto the_epoll = epoll::create();

    std::string_view hostname = "localhost";
    std::string_view port = "8080";
    addrinfo hints =
    {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };
    int backlog = true ? 16 : SOMAXCONN;
    the_epoll->listen(std::data(hostname), std::data(port), &hints, backlog);

    int max_events = 16;
    int timeout = 1000;
    std::vector<struct epoll_event> events(max_events);

    while (true)
    {
        auto expected = the_epoll->wait(events, timeout);
        if (expected)
            continue;

        std::cerr << expected.error();
        if (expected.error().value() == EINTR)
            // 系統調用被信號中斷，需要重新執行 epoll_wait
            continue;

        break;
    }

    return 0;
}