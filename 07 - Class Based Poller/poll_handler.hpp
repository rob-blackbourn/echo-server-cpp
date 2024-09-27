#ifndef JETBLACK_NET_POLL_HANDLER_HPP
#define JETBLACK_NET_POLL_HANDLER_HPP

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

namespace jetblack::net
{
  class Poller;

  class PollHandler
  {
  public:
    virtual ~PollHandler() {};
    virtual bool is_listener() const noexcept = 0;
    virtual int fd() const noexcept = 0;
    virtual bool is_open() const noexcept = 0;
    virtual bool want_read() const noexcept = 0;
    virtual bool want_write() const noexcept = 0;
    virtual bool read(Poller& poller) noexcept = 0;
    virtual bool write() noexcept = 0;
    virtual void enqueue(std::vector<char> buf) noexcept = 0;
    virtual std::optional<std::vector<char>> dequeue() noexcept = 0;
  };

  inline int poll(std::vector<pollfd> &fds)
  {
    int active_fd_count = ::poll(fds.data(), fds.size(), -1);
    if (active_fd_count < 0)
    {
      throw std::system_error(
        errno, std::generic_category(), "poll failed");
    }
    return active_fd_count;
  }
}

#endif // JETBLACK_NET_POLL_HANDLER_HPP
