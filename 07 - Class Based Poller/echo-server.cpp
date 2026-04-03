#include <iostream>
#include <set>

#include "tcp.hpp"
#include "event_loop.hpp"
#include "tcp_listener_event_handler.hpp"
#include "utils.hpp"

using namespace jetblack::net;

int main()
{
  const uint16_t port = 22000;

  try
  {
    std::cout << std::format("starting echo server on port {}.\n", port);

    auto event_loop = EventLoop(
      [](EventLoop&, int fd)
      {
        std::cout << std::format("on_open: {}\n", fd);
      },
      [](EventLoop&, int fd)
      {
        std::cout << std::format("on_close: {}\n", fd);
      },
      [](EventLoop& event_loop, int fd, std::vector<std::vector<char>> bufs)
      {
        std::cout << std::format("on_read: {}\n", fd);

        for (auto& buf : bufs)
        {
          std::cout << std::format("on_read: received {}\n", to_string(buf));
          event_loop.write(fd, buf);
        }
      },
      [](EventLoop&, int fd, std::exception error)
      {
        std::cout << std::format("on_error: {}, {}\n", fd, error.what());
      }
    );
    event_loop.add_handler(std::make_unique<TcpListenerEventHandler>(port));
    event_loop.event_loop();
  }
  catch(const std::exception& error)
  {
    std::cerr << std::format("Server failed: {}\n", error.what());
  }

  return 0;
}
