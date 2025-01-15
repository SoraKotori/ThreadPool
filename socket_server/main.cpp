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
#include <sys/epoll.h>
#include <netdb.h>

template<typename CharT, typename Traits>
auto print_error_message(std::basic_ostream<CharT, Traits>& os, const std::error_code& error)
{
    os << error << ',' << error.message() << '\n';
}

class file_descriptor : public std::optional<int>
{
public:
    using Base = std::optional<int>;

    explicit file_descriptor(int fd) :
        Base(std::in_place, fd) {}

    ~file_descriptor()
    {
        if (Base::has_value())
            if (::close(Base::value()) == -1)
                print_error_message(std::cerr, std::error_code(errno, std::system_category()));
    }

    file_descriptor(const file_descriptor&) = delete;
    file_descriptor& operator=(const file_descriptor&) = delete;

    file_descriptor(file_descriptor&& other)
    {
        // a moved-from std::optional still contains a value, but the value itself is moved from.
        Base::swap(other);
    }
};

class epoll
{
private:
    epoll(int epfd) :
        m_epfd(epfd) {}

    file_descriptor m_epfd;
    std::map<file_descriptor, void*, std::less<file_descriptor::Base>> m_fds;

public:
    using iterator = typename decltype(m_fds)::iterator;
    using key_type = typename decltype(m_fds)::key_type;
    using mapped_type = typename decltype(m_fds)::mapped_type;

    static std::expected<epoll, std::error_code> create()
    {
        int epfd = ::epoll_create1(0);
        if (epfd == -1) {
            return std::unexpected(std::error_code(errno, std::system_category()));
        }

        return epoll(epfd);
    }

    auto erase(iterator it)
    {
        if (::epoll_ctl(*m_epfd, EPOLL_CTL_DEL, *it->first, nullptr) == -1)
            print_error_message(std::cerr, std::error_code(errno, std::system_category()));

        // The iterator pos must be valid and dereferenceable.
        // return iterator following the last removed element.
        return m_fds.erase(it);
    }

    auto try_emplace(uint32_t event_flag, file_descriptor fd, mapped_type mapped)
    {
        auto it_inserted = m_fds.try_emplace(std::move(fd), std::move(mapped));
        if (!it_inserted.second)
        {
            std::cerr << "m_fds.try_emplace Number of fd inserted: 0\n";

            return it_inserted;
        }
        
        static_assert(sizeof(it_inserted.first) == sizeof(epoll_data_t));
        struct epoll_event event = {
            .events = event_flag,
            .data = { .ptr = *reinterpret_cast<void**>(&it_inserted.first) }
        };

        if (::epoll_ctl(*m_epfd, EPOLL_CTL_ADD, *fd, &event) == -1)
        {
            print_error_message(std::cerr, std::error_code(errno, std::system_category()));
            // std::println(std::cerr, "epoll_ctl: {}.", std::strerror(errno));

            it_inserted.first = erase(it_inserted.first);
            it_inserted.second = false;
        }

        return it_inserted;
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
            std::println(std::cerr, "getaddrinfo: {}", ::gai_strerror(status)); // !
            return std::unexpected(std::error_code(status, std::system_category())); // !
        }

        for (auto result_ptr = result.get(); result_ptr; result_ptr = result_ptr->ai_next)
        {
            // ai_family: AF_INET is 2, AF_INET6 is 10
            file_descriptor listen_fd(::socket(result_ptr->ai_family,
                                               result_ptr->ai_socktype,
                                               result_ptr->ai_protocol));
            if (*listen_fd == -1)
            {
                print_error_message(std::cerr, std::error_code(errno, std::system_category()));
                continue;
            }

            if (::bind(*listen_fd, result_ptr->ai_addr,
                                   result_ptr->ai_addrlen) == -1)
            {
                print_error_message(std::cerr, std::error_code(errno, std::system_category()));
                continue;
            }

            if (::listen(*listen_fd, backlog) == -1)
            {
                print_error_message(std::cerr, std::error_code(errno, std::system_category()));
                continue;
            }

            // Transfer ownership of listen_fd to the try_emplace function.
            try_emplace(EPOLLIN, std::move(listen_fd), {});

            // listen_fd is now empty after the ownership has been moved
            assert(!listen_fd.has_value());
        }

        return true;
    }

    std::expected<bool, std::error_code> wait(std::vector<struct epoll_event>& events, int timeout)
    {
        int events_size = ::epoll_wait(*m_epfd, std::data(events), std::size(events), timeout);
        if (events_size == -1)
            return std::unexpected(std::error_code(errno, std::system_category()));

        for (int events_index = 0; events_index < events_size; events_index++)
        {
            // warning: taking address of packed member of 'epoll_event' may result in an unaligned pointer value
            iterator it = *reinterpret_cast<iterator*>(&events[events_index].data.ptr);

            auto event_flag = events[events_index].events;
            auto socket_fd = *it->first;

            std::println(std::clog, "event_flag: {}", event_flag);

            if (event_flag & EPOLLERR)
            {
                std::println(std::cerr, "EPOLLERR, fd: {}", socket_fd);
                continue;
            }

            if (event_flag & EPOLLHUP)
            {
                erase(it);
                continue;
            }

            if (event_flag & EPOLLRDHUP)
            {
                if (::shutdown(socket_fd, SHUT_WR) == -1)
                    print_error_message(std::cerr, std::error_code(errno, std::system_category()));
            }

            int is_listen;
            socklen_t len = sizeof(is_listen);
            if (::getsockopt(socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &is_listen, &len) == -1)
            {
                print_error_message(std::cerr, std::error_code(errno, std::system_category()));
                continue;
            }

            if (is_listen) // listen socket
            {
                sockaddr_storage addr_storage;
                sockaddr& addr = reinterpret_cast<sockaddr&>(addr_storage);
                socklen_t addr_len = sizeof(sockaddr_storage);

                file_descriptor accept_fd(::accept(socket_fd, &addr, &addr_len));
                if (accept_fd == -1)
                {
                    std::cerr << std::error_code(errno, std::system_category());
                    // std::println(std::cerr, "accept: {}", std::strerror(errno));
                    continue;
                }

                std::array<char, 1024> receive_host;
                std::array<char, 1024> receive_port;
                if (int status = ::getnameinfo(&addr, addr_len,
                                               std::data(receive_host), std::size(receive_host),
                                               std::data(receive_port), std::size(receive_port), 0))
                {
                    std::println(std::cerr, "getnameinfo: {}", gai_strerror(status));
                    continue;
                }

                // print host and port
                std::println(std::clog, "accept from {}:{}", std::data(receive_host), std::data(receive_port));

                try_emplace(EPOLLIN | EPOLLRDHUP, std::move(accept_fd), {});
            }
            else // accept socket
            {
                std::array<char, 1024 * 16> buffer;

                auto receive_size = ::recv(socket_fd, std::data(buffer), std::size(buffer), 0);
                if (receive_size == -1)
                {
                    print_error_message(std::cerr, std::error_code(errno, std::system_category()));
                    continue;
                }

                if (receive_size == 0)
                {
                    std::println(std::clog, "receive_size: 0");
                    continue;
                }

                std::string_view receive{std::data(buffer), static_cast<std::size_t>(receive_size)};
                std::print(std::clog, "receive_size: {}, {}", receive_size, receive);

                if (receive_size != ::send(socket_fd, std::data(buffer), receive_size, 0))
                    std::println(std::cerr, "Error sending response");
            }
        }

        return true;
    }
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