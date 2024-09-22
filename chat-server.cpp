#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <utility>

#include "match.hpp"
#include "tcp.hpp"
#include "utils.hpp"

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

      while (stream->can_read())
      {
        auto buf = stream->read();
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
