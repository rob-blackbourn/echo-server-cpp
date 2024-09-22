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

class chat_server : public tcp_server<tcp_buffered_client>
{
public:
  chat_server(uint16_t port)
    : tcp_server(port)
  {
  }

  std::shared_ptr<tcp_buffered_client> on_accept(int fd, const std::string& addr, uint16_t port) override
  {
    return std::make_shared<tcp_buffered_client>(fd, addr, port, 8096, 8096);
  }

  void on_read(int fd, const std::shared_ptr<tcp_buffered_client>& client) override
  {
      while (!client->read_queue.empty())
      {
        for (auto& [other_fd, other_client] : clients())
        {
          if (other_fd != fd)
            other_client->enqueue_write(client->read_queue.front());
        }
        client->read_queue.pop_front();
      }
  }
};

int
main(int argc, char** argv)
{
  const uint16_t port = 22000;

  auto server = chat_server(port);
  server.event_loop();

  return 0;
}
