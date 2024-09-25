#include <iostream>
#include <set>

#include <spdlog/spdlog.h>

#include "tcp.hpp"
#include "poll_handler.hpp"
#include "utils.hpp"

using namespace jetblack::net;

int main(int argc, char** argv)
{
  const uint16_t port = 22000;

  try
  {
    spdlog::info("starting server on port {}.", port);

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
      [clients](Poller& poller, int fd, std::vector<std::vector<char>> bufs)
      {
        spdlog::info("on_read: {}", fd);

        for (auto& buf : bufs)
        {
          poller.enqueue(fd, buf);
        }
      },
      [](Poller&, int fd, std::exception error)
      {
        spdlog::info("on_error: {}, {}", fd, error.what());
      }
    );
    poller.add_handler(std::make_shared<TcpListenerSocketPollHandler>(port));
    poller.event_loop();
  }
  catch(const std::exception& error)
  {
    spdlog::error("Server failed: {}", error.what());
  }

  return 0;
}
