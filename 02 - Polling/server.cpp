#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <cerrno>
#include <cstring>
#include <set>
#include <vector>

int main(int argc, char **argv)
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
    if (fcntl(listen_fd, F_SETFL, O_NONBLOCK) == -1)
    {
        std::cerr
            << "failed to make listener socket non-blocking: "
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

    std::set<int> client_fds;
    bool is_listening = true;
    while (is_listening)
    {
        std::vector<pollfd> fds;

        fds.push_back(pollfd{listen_fd, POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL, 0});
        for (auto client_fd : client_fds)
            fds.push_back(pollfd{client_fd, POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL, 0});

        int nactive_fds = poll(fds.data(), fds.size(), -1);
        if (nactive_fds < 0)
        {
            std::cerr << "failed to poll: " << std::strerror(errno) << std::endl;
            is_listening = false;
            continue;
        }
        for (auto i = fds.begin(); nactive_fds > 0 && i != fds.end(); ++i)
        {
            if (i->revents == 0)
            {
                std::cout << "No events for fd " << i->fd << std::endl;
            }
            else if ((i->revents & POLLIN) == 0)
            {
                std::cout << "Unhandled events for fd " << i->fd << std::endl;
                is_listening = false;
                break;
            }
            else
            {
                --nactive_fds;
                
                if (i->fd == listen_fd)
                {
                    int client_fd = accept(listen_fd, (sockaddr *)nullptr, nullptr);
                    if (client_fd == -1)
                    {
                        std::cerr
                            << "failed to accept client socket: "
                            << std::strerror(errno)
                            << std::endl;
                        is_listening = false;
                        break;
                    }

                    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) == -1)
                    {
                        std::cerr
                            << "failed to make client socket non-blocking: "
                            << std::strerror(errno)
                            << std::endl;
                        is_listening = false;
                        break;
                    }

                    client_fds.insert(client_fd);
                }
                else
                {
                    char buf[100];
                    memset(buf, 0, sizeof(buf));

                    ssize_t nbytes_read = read(i->fd, buf, sizeof(buf));
                    std::cout << "Received " << nbytes_read << " bytes from " << i->fd << " of \"" << buf << "\"" << std::endl;
                    if (nbytes_read <= 0)
                    {
                        std::cout << "Removing client " << i->fd << std::endl;
                        client_fds.erase(i->fd);
                    }
                    else
                    {
                        std::cout << "Echoing back - " << buf << std::endl;
                        std::cout << "Writing " << strlen(buf) + 1 << std::endl;
                        ssize_t nbytes_written = write(i->fd, buf, strlen(buf) + 1);
                        std::cout << "Wrote " << nbytes_written << " bytes" << std::endl;
                    }
                }
            }
        }
    }

    return 0;
}
