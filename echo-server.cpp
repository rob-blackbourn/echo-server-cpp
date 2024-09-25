#include <iostream>

#include <spdlog/spdlog.h>

#include "tcp.hpp"
#include "utils.hpp"

using namespace jetblack::net;

int main(int argc, char** argv)
{
  const uint16_t port = 22000;

  try
  {
    spdlog::info("starting server on port {}.", port);
    
    auto server = TcpServer(
      port,
      [](TcpServer&, const TcpServer::stream_pointer& stream)
      {
        spdlog::info("on_open: {}", stream->socket->fd());
      },
      [](TcpServer&, const TcpServer::stream_pointer& stream)
      {
        spdlog::info("on_close: {}", stream->socket->fd());
      },
      [](TcpServer&, const TcpServer::stream_pointer& stream, std::optional<std::exception> error)
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
          spdlog::info("on_read: echoing back {}", to_string(buf));
          stream->enqueue_write(buf);
        }
      },
      [](TcpServer&, const TcpServer::stream_pointer& stream, std::optional<std::exception> error)
      {
        spdlog::info("on_write: {}", stream->socket->fd());

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
