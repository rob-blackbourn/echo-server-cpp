#include <iostream>

#include "tcp.hpp"

int main(int argc, char** argv)
{
  const uint16_t port = 22000;

  auto server = tcp_server(
    port,
    [](int fd, const tcp_server::stream_pointer& stream, const tcp_server::stream_map& streams)
    {
      std::cout << "on_open: " << fd << std::endl;
    },
    [](int fd, const tcp_server::stream_pointer& stream, const tcp_server::stream_map& streams)
    {
      std::cout << "on_read: " << fd << std::endl;

      while (stream->has_reads())
      {
        stream->enqueue_write(stream->deque_read());
      }
    },
    [](int fd, const tcp_server::stream_pointer& stream, const tcp_server::stream_map& streams)
    {
      std::cout << "on_close: " << fd << std::endl;
    });
  server.event_loop();

  return 0;
}
