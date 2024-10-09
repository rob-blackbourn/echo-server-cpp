#ifndef JETBLACK_NET_POLL_HANDLER_HPP
#define JETBLACK_NET_POLL_HANDLER_HPP

#include <optional>
#include <vector>

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
    virtual void close() noexcept = 0;
    virtual void enqueue(const std::vector<char>& buf) noexcept = 0;
    virtual std::optional<std::vector<char>> dequeue() noexcept = 0;
  };
}

#endif // JETBLACK_NET_POLL_HANDLER_HPP
