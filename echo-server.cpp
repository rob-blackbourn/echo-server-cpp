#include <iostream>

#include <spdlog/spdlog.h>

#include "tcp.hpp"
#include "utils.hpp"

int main(int argc, char** argv)
{
  const uint16_t port = 22000;

  try
  {
    spdlog::info("starting server on port {}.", port);
    
    auto server = tcp_server(
      port,
      [](int fd, const tcp_server::stream_pointer& stream, const tcp_server::stream_map& streams)
      {
        spdlog::info("on_open: {}", fd);
      },
      [](int fd, const tcp_server::stream_pointer& stream, const tcp_server::stream_map& streams)
      {
        spdlog::info("on_close: {}", fd);
      },
      [](int fd, const tcp_server::stream_pointer& stream, const tcp_server::stream_map& streams, std::optional<std::exception> error)
      {
        spdlog::info("on_read: {}", fd);

        if (error)
        {
          spdlog::error("on_read: failed to read from socket - {}", error->what());
          return;
        }

        while (stream->has_reads())
        {
          auto buf = stream->deque_read();
          spdlog::info("on_read: echoing back {}", to_string(buf));
          stream->enqueue_write(buf);
        }
      },
      [](int fd, const tcp_server::stream_pointer& stream, const tcp_server::stream_map& streams, std::optional<std::exception> error)
      {
        spdlog::info("on_write: {}", fd);

        if (error)
        {
          spdlog::error("on_write: failed to write to socket - {}", error->what());
          return;
        }
      });

    server.event_loop();
  }
  catch (std::exception& error)
  {
    spdlog::error("Server failed: {}", error.what());
  }

  return 0;
}
