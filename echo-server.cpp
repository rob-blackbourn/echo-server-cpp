#include <iostream>
#include <set>

#include <spdlog/spdlog.h>

#include "tcp.hpp"
#include "poller.hpp"
#include "tcp_listener_socket_poll_handler.hpp"
#include "utils.hpp"

using namespace jetblack::net;

int main(int argc, char** argv)
{
  const uint16_t port = 22000;

  try
  {
    spdlog::info("starting echo server on port {}.", port);

    auto poller = Poller(
      [](Poller&, int fd)
      {
        spdlog::info("on_open: {}", fd);
      },
      [](Poller&, int fd)
      {
        spdlog::info("on_close: {}", fd);
      },
      [](Poller& poller, int fd, std::vector<std::vector<char>> bufs)
      {
        spdlog::info("on_read: {}", fd);

        for (auto& buf : bufs)
        {
          spdlog::info("on_read: received {}", to_string(buf));
          poller.write(fd, buf);
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
