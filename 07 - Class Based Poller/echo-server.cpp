#include <iostream>
#include <set>

#include "tcp.hpp"
#include "poller.hpp"
#include "tcp_listener_poll_handler.hpp"
#include "utils.hpp"

using namespace jetblack::net;

int main()
{
  const uint16_t port = 22000;

  try
  {
    std::cout << std::format("starting echo server on port {}.\n", port);

    auto poller = Poller(
      [](Poller&, int fd)
      {
        std::cout << std::format("on_open: {}\n", fd);
      },
      [](Poller&, int fd)
      {
        std::cout << std::format("on_close: {}\n", fd);
      },
      [](Poller& poller, int fd, std::vector<std::vector<char>> bufs)
      {
        std::cout << std::format("on_read: {}\n", fd);

        for (auto& buf : bufs)
        {
          std::cout << std::format("on_read: received {}\n", to_string(buf));
          poller.write(fd, buf);
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
