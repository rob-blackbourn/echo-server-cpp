#include <iostream>

#include "tcp.hpp"

int main(int argc, char** argv)
{
  const uint16_t port = 22000;

  auto server = tcp_server(
    port,
    [](int fd, const tcp_server::stream_pointer& stream, const tcp_server::stream_map& streams)
    {
      std::cout << "Client accept: " << fd << std::endl;
    },
    [](int fd, const tcp_server::stream_pointer& stream, const tcp_server::stream_map& streams)
    {
      std::cout << "Client read: " << fd << std::endl;

      while (stream->has_reads())
      {
        auto buf = stream->deque_read();
        for (auto& [other_fd, other_stream] : streams)
        {
          if (other_fd != fd)
            other_stream->enqueue_write(buf);
        }
      }

    });
  server.event_loop();

  return 0;
}
