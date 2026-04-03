#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <iostream>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <format>
#include <string>

#include "io.hpp"

int main()
{
    const uint16_t port = 22000;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        std::cerr << std::format(
            "failed to create client socket: {}\n",
            std::strerror(errno));
        return 1;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &(servaddr.sin_addr));

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        std::cerr << std::format(
            "failed to connect socket to port {}: {}\n",
            port,
            std::strerror(errno));
        return 1;
    }

    while (true)
    {
        std::cout << "Enter a message: ";
        std::string message;
        std::cin >> message;

        const char* send_buf = message.c_str();
        std::size_t send_buf_len = message.size() + 1;
        std::cout << std::format("Sending {} bytes for \"{}\"\n", send_buf_len, send_buf);
        auto success = jetblack::write_all(sockfd, send_buf, send_buf_len);
        if (!success)
        {
            std::cout << "Failed to write\n";
            break;
        }

        char buf[100];
        memset(buf, 0, sizeof(buf));
        auto nbytes_read = jetblack::read(sockfd, buf, sizeof(buf));
        if (!nbytes_read)
        {
            std::cout << "Failed to write\n";
            break;
        }

        std::cout << std::format("Received {} bytes of \"{}\"\n", *nbytes_read, buf);
    }
}
