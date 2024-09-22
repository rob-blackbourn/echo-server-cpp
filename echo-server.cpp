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
#include "tcp_server.hpp"
#include "stream_out.hpp"

int
main(int argc, char** argv)
{
  const uint16_t port = 22000;

  auto server = tcp_server(
    port,
    [](int fd, const std::shared_ptr<tcp_buffered_client>& client, const std::map<int, std::shared_ptr<tcp_buffered_client>>& clients)
    {
      std::cout << "Client accept: " << fd << std::endl;
    },
    [](int fd, const std::shared_ptr<tcp_buffered_client>& client, const std::map<int, std::shared_ptr<tcp_buffered_client>>& clients)
    {
      std::cout << "Client read: " << fd << std::endl;

      while (!client->is_read_queue_empty())
      {
        client->enqueue_write(std::move(client->dequeue_read()));
      }

    });
  server.event_loop();

  return 0;
}
