#include <string_view>
#include <memory>
#include <iostream>

// #include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int main(int argc, char* argv[])
{
    std::string_view hostname = "localhost";
    std::string_view port = "8080";

    addrinfo hints =
    {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };

    std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> result(nullptr, ::freeaddrinfo);
    if (int status = ::getaddrinfo(std::data(hostname), std::data(port), &hints, std::out_ptr(result)))
    {
        std::println(std::cerr, "getaddrinfo: {}.", ::gai_strerror(status));
        return EXIT_FAILURE;
    }

    int file_descriptor = 0;
    for (auto result_ptr = result.get(); true; result_ptr = result_ptr->ai_next)
    {
        if (!result_ptr)
        {
            std::println(std::cerr, "Could not bind.");
            return EXIT_FAILURE;
        }

        if ((file_descriptor = ::socket(result_ptr->ai_family,
                                        result_ptr->ai_socktype,
                                        result_ptr->ai_protocol)) == -1)
            continue;

        if (int status = ::bind(file_descriptor, result_ptr->ai_addr,
                                                 result_ptr->ai_addrlen); !status)
            break;

        close(file_descriptor);
    }

    // struct io_uring_params params;

    int queue_size = 16;
    if (int status = ::listen(file_descriptor, queue_size))
    {
        std::println(std::cerr, "getaddrinfo: {}.", ::gai_strerror(status));
        return EXIT_FAILURE;
    }


    while (true)
    {
        std::array<char, 1024 * 16> buffer;
        sockaddr_storage addr_storage;

        sockaddr& addr = reinterpret_cast<sockaddr&>(addr_storage);
        socklen_t addr_len = sizeof(sockaddr_storage);

        auto receive_size = ::recvfrom(file_descriptor,
                                       std::data(buffer), std::size(buffer),
                                       0,
                                       &addr, &addr_len);
        if (receive_size == -1)
            continue;

        std::array<char, 1024> receive_host;
        std::array<char, 1024> receive_port;
        if (int status = ::getnameinfo(&addr, addr_len,
                                       std::data(receive_host), std::size(receive_host),
                                       std::data(receive_port), std::size(receive_port), 0))
            continue;

        std::println(std::cerr, "Received {} bytes from {}:{}.", receive_size, std::data(receive_host), std::data(receive_port));

        if (receive_size != ::sendto(file_descriptor,
                                     std::data(buffer), receive_size,
                                     0,
                                     &addr, addr_len))
            std::println(std::cerr, "Error sending response");
               
        // std::string_view receive{std::data(buffer), receive_size};
        // std::println(std::cerr, "{}", receive);
    }

    close(file_descriptor);

    return 0;
}