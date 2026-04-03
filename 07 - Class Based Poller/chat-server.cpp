#include <iostream>
#include <set>

#include "poller.hpp"
#include "tcp_listener_poll_handler.hpp"
#include "utils.hpp"

using namespace jetblack::net;

int main()
{
  const uint16_t port = 22000;

  try
  {
    std::cout << std::format("starting chat server on port {}.\n", port);

    std::set<int> clients;

    auto poller = Poller(
      [&clients](Poller&, int fd)
      {
        std::cout << std::format("on_open: {}\n", fd);
        clients.insert(fd);
      },
      [&clients](Poller&, int fd)
      {
        std::cout << std::format("on_close: {}\n", fd);
        clients.erase(fd);
      },
      [&clients](Poller& poller, int fd, std::vector<std::vector<char>> bufs)
      {
        std::cout << std::format("on_read: {}\n", fd);

        for (auto& buf : bufs)
        {
          std::cout << std::format("on_read: received {}\n", to_string(buf));
          for (auto client_fd : clients)
          {
            if (client_fd != fd)
            {
              std::cout << std::format("on_read: sending to {}\n", client_fd);
              poller.write(client_fd, buf);
            }
          }
        }
      },
      [](Poller&, int fd, std::exception error)
      {
        std::cout << std::format("on_error: {}, {}\n", fd, error.what());
      }
    );
    poller.add_handler(std::make_unique<TcpListenerPollHandler>(port));
    poller.event_loop();
  }
  catch(const std::exception& error)
  {
    std::cerr << std::format("Server failed: {}\n", error.what());
  }

  return 0;
}
