#ifndef __chat_server_hpp
#define __chat_server_hpp

#include "tcp_buffered_client.hpp"
#include "tcp_server.hpp"

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

#endif // __chat_server_hpp
