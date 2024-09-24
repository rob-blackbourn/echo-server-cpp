#include <iostream>

#include <spdlog/spdlog.h>

#include "tcp.hpp"
#include "utils.hpp"

using namespace jetblack::net;

int main(int argc, char** argv)
{
  const uint16_t port = 22000;

  auto server = tcp_server(
    port,
    [](tcp_server&, const tcp_server::stream_pointer& stream)
    {
      spdlog::info("on_open: {}", stream->socket->fd());
    },
    [](tcp_server&, const tcp_server::stream_pointer& stream)
    {
      spdlog::info("on_close: {}", stream->socket->fd());
    },
    [](tcp_server& server, const tcp_server::stream_pointer& stream, std::optional<std::exception> error)
    {
      spdlog::info("on_read: {}", stream->socket->fd());

      if (error)
      {
        spdlog::error("on_read: failed to read from socket - {}", error->what());
        return;
      }

      while (stream->has_reads())
      {
        auto buf = stream->deque_read();
        spdlog::info("on_read: received {}", to_string(buf));
        for (auto& [other_fd, other_stream] : server.streams())
        {
          if (other_fd != stream->socket->fd())
          {
            spdlog::info("on_read: sending to {}", other_fd);
            other_stream->enqueue_write(buf);
          }
        }
      }
    },
    [](tcp_server&, const tcp_server::stream_pointer& stream, std::optional<std::exception> error)
    {
      spdlog::info("on_write: {}", stream->socket->fd());

      if (error)
      {
        spdlog::error("on_write: failed to write to socket - {}", error->what());
        return;
      }
    });
  server.event_loop();

  return 0;
}
