#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <deque>
#include <format>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "client_state.hpp"

class tcp_listener
{
private:
    int fd_;

public:
    tcp_listener(uint16_t port)
    {
        fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ == -1)
        {
            throw std::runtime_error(
                std::format(
                    "failed to create listener socket: {}",
                    std::strerror(errno)));
        }

        auto is_nblk = jetblack::fcntl::set_flag<O_NONBLOCK>(fd_, true);
        if (!is_nblk)
        {
            throw std::runtime_error(
                std::format(
                    "failed to make listener socket non-blocking: {}",
                    std::strerror(is_nblk.error())));
        }

        sockaddr_in servaddr;
        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htons(INADDR_ANY);
        servaddr.sin_port = htons(port);

        if (bind(fd_, (sockaddr *)&servaddr, sizeof(servaddr)) == -1)
        {
            throw std::runtime_error(
                std::format(
                    "failed to bind listener socket to port {}: {}",
                    port,
                    std::strerror(errno)));
        }

        if (listen(fd_, 10) == -1)
        {
            throw std::runtime_error(
                std::format(
                    "failed to listen to bound socket: {}",
                    std::strerror(errno)));
        }
    }

    int fd() const { return fd_; }

    ClientState accept()
    {
        // New client.
        int client_fd = ::accept(fd_, (sockaddr *)nullptr, nullptr);
        if (client_fd == -1)
        {
            throw std::runtime_error(
                std::format(
                    "failed to accept client socket: {}",
                    std::strerror(errno)));
        }

        auto client_state = ClientState{client_fd};

        auto is_blk = client_state.set_blocking(false);
        if (!is_blk)
        {
            throw std::runtime_error(
                std::format(
                    "failed to make client socket non-blocking: {}",
                    std::strerror(is_blk.error()))
            );
        }

        return client_state;
    }

};

int main()
{
    const uint16_t port = 22000;

    try
    {
        tcp_listener listener = { port };

        std::map<int, ClientState> client_fds;

        while (true)
        {
            std::vector<pollfd> fds;

            fds.push_back(pollfd{listener.fd(), POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL, 0});
            for (auto& [client_fd, client_state] : client_fds)
            {
                int16_t flags = POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL;
                if (client_state.can_write())
                    flags |= POLLOUT;

                fds.push_back(pollfd{client_fd, flags, 0});
            }

            int nactive_fds = poll(fds.data(), fds.size(), -1);
            if (nactive_fds < 0)
            {
                throw std::runtime_error(
                    std::format(
                        "failed to poll: {}",
                        std::strerror(errno)));
            }

            for (auto i = fds.begin(); i != fds.end(); ++i)
            {
                if (i->revents == 0)
                {
                    std::cout << "No events for fd " << i->fd << std::endl;
                    continue;
                }

                if ((i->revents & POLLIN) == POLLIN)
                {
                    std::cout << "Read event for fd " << i->fd << std::endl;

                    --nactive_fds;
                    
                    if (i->fd == listener.fd())
                    {
                        auto client = listener.accept();
                        client_fds[client.fd()] = client;
                    }
                    else
                    {
                        // Client read
                        auto& client_state = client_fds[i->fd];

                        bool is_open = client_state.enqueue_reads();
                        if (!is_open)
                        {
                            std::cout << "Close event for fd " << i->fd << std::endl;
                            client_fds.erase(i->fd);
                            continue;
                        }

                        while (true)
                        {
                            auto buf = client_state.read();
                            if (buf.has_value())
                                client_state.enqueue_write(std::move(buf.value()));
                            else
                                break;
                        }
                    }
                }

                if ((i->revents & POLLOUT) == POLLOUT)
                {
                    std::cout << "Write event for fd " << i->fd << std::endl;

                    // Client write
                    auto& client_state = client_fds[i->fd];

                    bool is_open = client_state.write();
                    if (!is_open)
                    {
                        std::cout << "Close event for fd " << i->fd << std::endl;
                        client_fds.erase(i->fd);
                        continue;
                    }
                }
            }
        }
    }
    catch(const std::runtime_error& e)
    {
        std::cerr << e.what() << '\n';
    }
    
    return 0;
}
