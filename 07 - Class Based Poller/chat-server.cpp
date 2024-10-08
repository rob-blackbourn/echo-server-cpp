#include <set>

#include <spdlog/spdlog.h>

#include "poller.hpp"
#include "tcp_listener_poll_handler.hpp"
#include "utils.hpp"

using namespace jetblack::net;

int main(int argc, char** argv)
{
  const uint16_t port = 22000;

  try
  {
    spdlog::info("starting chat server on port {}.", port);

    std::set<int> clients;

    auto poller = Poller(
      [&clients](Poller&, int fd)
      {
        spdlog::info("on_open: {}", fd);
        clients.insert(fd);
      },
      [&clients](Poller&, int fd)
      {
        spdlog::info("on_close: {}", fd);
        clients.erase(fd);
      },
      [&clients](Poller& poller, int fd, std::vector<std::vector<char>> bufs)
      {
        spdlog::info("on_read: {}", fd);

        for (auto& buf : bufs)
        {
          spdlog::info("on_read: received {}", to_string(buf));
          for (auto client_fd : clients)
          {
            if (client_fd != fd)
            {
              spdlog::info("on_read: sending to {}", client_fd);
              poller.write(client_fd, buf);
            }
          }
        }
      },
      [](Poller&, int fd, std::exception error)
      {
        spdlog::info("on_error: {}, {}", fd, error.what());
      }
    );
    poller.add_handler(std::make_unique<TcpListenerPollHandler>(port));
    poller.event_loop();
  }
  catch(const std::exception& error)
  {
    spdlog::error("Server failed: {}", error.what());
  }

  return 0;
}
