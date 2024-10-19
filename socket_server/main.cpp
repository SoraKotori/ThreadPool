#include <cerrno>
#include <cstring>

#include <string_view>
#include <memory>
#include <iostream>
#include <vector>

// #include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <sys/epoll.h>

int main(int argc, char* argv[])
{
    int max_events = 16;
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        std::println(std::cerr, "epoll_create1: {}.", std::strerror(errno));
        return EXIT_FAILURE;
    }

    std::string_view hostname = "localhost";
    std::string_view port = "8080";
    int backlog = true ? 16 : SOMAXCONN;

    addrinfo hints =
    {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };

    // getaddrinfo() returns 0 if it succeeds, or the nonzero error codes
    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> result(nullptr, freeaddrinfo);
    if (int status = getaddrinfo(std::data(hostname), std::data(port), &hints, std::out_ptr(result)))
    {
        std::println(std::cerr, "getaddrinfo: {}.", gai_strerror(status));
        return EXIT_FAILURE;
    }

    for (auto result_ptr = result.get(); result_ptr; result_ptr = result_ptr->ai_next)
    {
        int socket_fd = socket(result_ptr->ai_family,
                               result_ptr->ai_socktype,
                               result_ptr->ai_protocol);
        if (socket_fd == -1)
            continue;

        if (bind(socket_fd, result_ptr->ai_addr,
                            result_ptr->ai_addrlen) == -1)
        {
            std::println(std::cerr, "bind: {}.", std::strerror(errno));

            if (close(socket_fd) == -1)
                std::println(std::cerr, "close: {}.", std::strerror(errno));

            continue;
        }

        if (listen(socket_fd, backlog))
        {
            std::println(std::cerr, "listen: {}.", std::strerror(errno));

            if (close(socket_fd) == -1)
                std::println(std::cerr, "close: {}.", std::strerror(errno));

            continue;
        }

        struct epoll_event event = {
            .events = EPOLLIN,
            .data = { .fd = socket_fd }
        };

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &event) == -1)
        {
            std::println(std::cerr, "epoll_ctl: {}.", std::strerror(errno));

            if (close(socket_fd) == -1)
                std::println(std::cerr, "close: {}.", std::strerror(errno));

            continue;
        }
    }

    while (true)
    {
        std::vector<struct epoll_event> events(max_events);

        int events_size = epoll_wait(epfd, std::data(events), std::size(events), 1000);
        if (events_size == -1)
        {
            std::println(std::cerr, "epoll_wait: {}.", std::strerror(errno));
            if (errno == EINTR)
                // 系統調用被信號中斷，需要重新執行 epoll_wait
                continue;
            else
                break;
        }

        for (int fd_index = 0; fd_index < events_size; fd_index++)
        {
            auto event_flag = events[fd_index].events;
            auto socket_fd  = events[fd_index].data.fd;

            if (event_flag & EPOLLERR)
            {
                std::println(std::cout, "EPOLLERR, fd: {}.", socket_fd);
                continue;
            }
            else if (event_flag & EPOLLHUP)
            {
                if (epoll_ctl(epfd, EPOLL_CTL_DEL, socket_fd, nullptr) == -1)
                    std::println(std::cerr, "epoll_ctl EPOLL_CTL_DEL: {}.", std::strerror(errno));

                if (close(socket_fd) == -1)
                    std::println(std::cerr, "close: {}.", std::strerror(errno));

                continue;
            }
            else if (event_flag != EPOLLIN)
            {
                std::println(std::cerr, "event_flag != EPOLLIN: {}.", event_flag);
                continue;
            }

            int is_listen;
            socklen_t len = sizeof(is_listen);
            if (getsockopt(socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &is_listen, &len) == -1)
            {
                std::println(std::cout, "getsockopt: {}.", std::strerror(errno));
                continue;
            }

            if (is_listen) // is a listening socket
            {
                sockaddr_storage addr_storage;
                sockaddr& addr = reinterpret_cast<sockaddr&>(addr_storage);
                socklen_t addr_len = sizeof(sockaddr_storage);

                int accept_fd = 0;
                if ((accept_fd = accept(socket_fd, &addr, &addr_len)) == -1)
                {
                    std::println(std::cerr, "accept: {}.", std::strerror(errno));
                    break;
                }

                std::array<char, 1024> receive_host;
                std::array<char, 1024> receive_port;
                if (int status = getnameinfo(&addr, addr_len,
                                                std::data(receive_host), std::size(receive_host),
                                                std::data(receive_port), std::size(receive_port), 0))
                {
                    std::println(std::cerr, "getnameinfo: {}.", gai_strerror(status));
                    break;
                }

                // print host and port
                std::println(std::clog, "accept from {}:{}.", std::data(receive_host), std::data(receive_port));

                struct epoll_event socket_event = {
                    .events = EPOLLIN,
                    .data = { .fd = accept_fd }
                };

                if (epoll_ctl(epfd, EPOLL_CTL_ADD, accept_fd, &socket_event) == -1)
                {
                    std::println(std::cerr, "epoll_ctl: {}.", std::strerror(errno));

                    if (close(socket_fd) == -1)
                        std::println(std::cerr, "close: {}.", std::strerror(errno));

                    break;
                }
            }
            else // not a listening socket
            {
                std::array<char, 1024 * 16> buffer;

                auto receive_size = recv(socket_fd, std::data(buffer), std::size(buffer), 0);
                if (receive_size == -1)
                {
                    std::println(std::cerr, "recvfrom error: {} {}.", errno, std::strerror(errno));
                    continue;
                }

                if (receive_size != send(socket_fd, std::data(buffer), receive_size, 0))
                    std::println(std::cerr, "Error sending response");
                    
                std::string_view receive{std::data(buffer), static_cast<std::size_t>(receive_size)};
                std::println(std::cerr, "{}", receive);
            }
        }
    }

    return 0;
}