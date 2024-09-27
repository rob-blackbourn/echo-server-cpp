#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <iostream>

int main()
{
    const char* host = "yahoo.com";
    const std::uint16_t port = 22000;
    char port_str[6];
    std::memset(port_str, 0, sizeof(port_str));
    auto res = std::to_chars(port_str, port_str + sizeof(port_str) - 1, port);
    if (res.ec != std::errc{})
    {
        throw std::system_error(std::make_error_code(res.ec));
    }

    addrinfo hints {
        .ai_flags = 0,
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
        .ai_addrlen = 0,
        .ai_addr = nullptr,
        .ai_canonname = nullptr,
        .ai_next = nullptr
    };
    addrinfo* addresses;
    int result = getaddrinfo(host, port_str, &hints, &addresses);
    if (result != 0)
    {
        throw std::runtime_error(gai_strerror(result));
    }

    for (addrinfo* i = addresses; i != nullptr; i = i->ai_next)
    {
        auto in_addr = reinterpret_cast<sockaddr_in*>(i->ai_addr);

        char ip_address[INET_ADDRSTRLEN];
        if (inet_ntop(i->ai_family, &in_addr->sin_addr, ip_address, sizeof(ip_address)) == nullptr)
        {
            throw std::system_error(errno, std::generic_category(), "failed to convert ip address");
        }
        std::cout << "found " << ip_address << ":" << ntohs(in_addr->sin_port) << std::endl;
    }

    freeaddrinfo(addresses);

    return 0;
}
