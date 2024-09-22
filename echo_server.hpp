#ifndef __echo_server_hpp
#define __echo_server_hpp

#include "tcp_buffered_client.hpp"
#include "tcp_server.hpp"

class echo_server : public tcp_server<tcp_buffered_client>
{
public:
  echo_server(uint16_t port)
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
        client->enqueue_write(std::move(client->read_queue.front()));
        client->read_queue.pop_front();
      }
  }
};

#endif // __echo_server_hpp
