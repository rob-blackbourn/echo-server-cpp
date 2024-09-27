#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <iostream>
#include <cerrno>
#include <cstring>
#include <string>

int main(int argc, char **argv)
{
    const uint16_t port = 22000;


    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        std::cerr
            << "failed to create client socket: "
            << std::strerror(errno)
            << std::endl;
        return 1;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &(servaddr.sin_addr));

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        std::cerr
            << "failed to connect socket to port " << port << ": "
            << std::strerror(errno)
            << std::endl;
        return 1;
    }

    while (1)
    {
        std::cout << "Enter a message: ";
        std::string message;
        std::cin >> message;

        const char* send_buf = message.c_str();
        std::size_t send_buf_len = message.size() + 1;
        std::cout << "Sending " << send_buf_len << " bytes for \"" << send_buf << "\"" << std::endl;
        write(sockfd, send_buf, send_buf_len);

        char buf[100];
        memset(buf, 0, sizeof(buf));
        ssize_t nbytes_read = read(sockfd, buf, sizeof(buf));
        std::cout << "Received " << nbytes_read << " bytes of \"" << buf << "\"" << std::endl;
    }
}
