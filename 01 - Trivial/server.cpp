#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <cerrno>
#include <cstring>

int main(int argc, char** argv)
{
    const uint16_t port = 22000;

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1)
    {
        std::cerr
            << "failed to create listener socket: "
            << std::strerror(errno)
            << std::endl;
        return 1;
    }

    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(listen_fd, (sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        std::cerr
            << "failed to bind listener socket to port " << port << ": "
            << std::strerror(errno)
            << std::endl;
        return 1;
    }

    if (listen(listen_fd, 10) == -1)
    {
        std::cerr
            << "failed to listen to bound socket: "
            << std::strerror(errno)
            << std::endl;
        return 1;
    }

    int client_fd = accept(listen_fd, (sockaddr *)nullptr, nullptr);
    if (client_fd == -1)
    {
        std::cerr
            << "failed to accept client socket: "
            << std::strerror(errno)
            << std::endl;
        return 1;
    }

    bool is_ok = true;
    while (is_ok)
    {
        char buf[100];
        memset(buf, 0, sizeof(buf));

        ssize_t nbytes_read = read(client_fd, buf, sizeof(buf));
        std::cout << "Received " << nbytes_read << " bytes of \"" << buf << "\"" << std::endl;
        if (nbytes_read <= 0)
        {
            std::cerr << "Exiting read" << std::endl;
            is_ok = false;
            continue;
        }

        std::cout << "Echoing back - " << buf << std::endl;
        std::cout << "Writing " << strlen(buf) + 1 << std::endl;
        ssize_t nbytes_written = write(client_fd, buf, strlen(buf) + 1);
        std::cout << "Wrote " << nbytes_written <<" bytes" << std::endl;
    }

    return 0;
}
