#ifndef JETBLACK_NET_LISTENER_SOCKET_POLL_HANDLER_HPP
#define JETBLACK_NET_LISTENER_SOCKET_POLL_HANDLER_HPP

#include <poll.h>

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>

#include "tcp_socket.hpp"
#include "tcp_listener_socket.hpp"
#include "tcp_stream.hpp"

#include "match.hpp"

#include "poll_handler.hpp"
#include "poller.hpp"
#include "tcp_server_socket_poll_handler.hpp"

namespace jetblack::net
{
  class TcpListenerSocketPollHandler : public PollHandler
  {
  private:
    TcpListenerSocket listener_;

  public:
    TcpListenerSocketPollHandler(uint16_t port, int backlog = 10)
    {
      listener_.bind(port);
      listener_.blocking(false);
      listener_.reuseaddr(true);
      listener_.listen(backlog);
    }
    ~TcpListenerSocketPollHandler() override
    {
    }

    bool is_listener() const noexcept override { return true; }

    int fd() const noexcept override { return listener_.fd(); }
    bool is_open() const noexcept override { return listener_.is_open(); }

    bool want_read() const noexcept override { return true; }
    bool want_write() const noexcept override { return false; }

    bool read(Poller& poller) noexcept override
    {
      // Accept the client. This might throw an exception which will not be caught, as subsequent
      // connections will also fail.
      auto client = listener_.accept();
      client->blocking(false);

      poller.add_handler(std::make_unique<TcpServerSocketPollHandler>(std::move(client), 8096, 8096));

      return true;
    }

    bool write() noexcept override { return false; }

    std::optional<std::vector<char>> dequeue() noexcept override { return std::nullopt; }
    void enqueue(std::vector<char> buf) noexcept override {}
  };

}

#endif // JETBLACK_NET_LISTENER_SOCKET_POLL_HANDLER_HPP
