#ifndef JETBLACK_NET_POLLER_HPP
#define JETBLACK_NET_POLLER_HPP

#include <poll.h>

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <system_error>
#include <utility>

#include "poll_handler.hpp"

namespace jetblack::net
{
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

  class Poller
  {
  public:
    typedef std::unique_ptr<PollHandler> handler_pointer;
    typedef std::map<int, handler_pointer> handler_map;
    typedef std::function<void(Poller&, int fd)> connection_callback;
    typedef std::function<void(Poller&, int fd, std::vector<std::vector<char>> bufs)> read_callback;
    typedef std::function<void(Poller&, int fd, std::exception)> error_callback;

  private:
    handler_map handlers_;
    connection_callback on_open_;
    connection_callback on_close_;
    read_callback on_read_;
    error_callback on_error_;

  public:
    Poller(
      connection_callback on_open,
      connection_callback on_close,
      read_callback on_read,
      error_callback on_error)
      : on_open_(on_open),
        on_close_(on_close),
        on_read_(on_read),
        on_error_(on_error)
    {
    }

    void add_handler(handler_pointer handler) noexcept
    {
      auto fd = handler->fd();
      auto is_listener = handler->is_listener();
      handlers_[fd] = std::move(handler);
      if (!is_listener)
        on_open_(*this, fd);
    }

    void write(int fd, std::vector<char> buf) noexcept
    {
      if (auto i = handlers_.find(fd); i != handlers_.end())
      {
        i->second->enqueue(buf);
      }
    }

    void event_loop(int backlog = 10)
    {
      bool is_ok = true;

      while (is_ok) {

        std::vector<pollfd> fds = make_poll_fds();

        int active_fd_count = poll(fds);

        for (const auto& poll_state : fds)
        {
          if (poll_state.revents == 0)
          {
            continue; // no events for file descriptor.
          }

          handle_event(poll_state);

          if (--active_fd_count == 0)
            break;
        }

        remove_closed_handlers();
      }
    }

  private:

    void handle_event(const pollfd& poll_state)
    {
      auto handler = handlers_[poll_state.fd].get();

      if ((poll_state.revents & POLLIN) == POLLIN)
      {
        if (handler->is_listener())
        {
          handler->read(*this);
          return;
        }
        
        if (!handle_read(handler))
          return;
      }

      if ((poll_state.revents & POLLOUT) == POLLOUT)
      {
        if (!handle_write(handler))
          return;
      }
    }

    bool handle_read(PollHandler* handler) noexcept
    {
      try
      {
        auto can_continue = handler->read(*this);

        std::vector<std::vector<char>> bufs;
        auto buf = handler->dequeue();
        while (buf.has_value())
        {
          bufs.push_back(buf.value());
          buf = handler->dequeue();
        }

        if (!bufs.empty())
        {
          on_read_(*this, handler->fd(), bufs);
        }

        return can_continue;
      }
      catch(const std::exception& error)
      {
        on_error_(*this, handler->fd(), error);
        return false;
      }
    }

    bool handle_write(PollHandler* handler) noexcept
    {
      try
      {
        return handler->write();
      }
      catch(const std::exception& error)
      {
        on_error_(*this, handler->fd(), error);
        return false;
      }
    }

    std::vector<pollfd> make_poll_fds()
    {
      std::vector<pollfd> fds;

      for (auto& [fd, handler] : handlers_)
      {
        int16_t flags = POLLPRI | POLLERR | POLLHUP | POLLNVAL;

        if (handler->want_read())
        {
            flags |= POLLIN;
        }

        // We are interested in writes when we have data to write.
        if (handler->want_write())
        {
            flags |= POLLOUT;
        }

        fds.push_back(pollfd{fd, flags, 0});
      }

      return fds;
    }

    void remove_closed_handlers()
    {
      std::vector<int> closed_fds;
      
      for (auto& [fd, handler] : handlers_)
      {
        if (!handler->is_open())
        {
          closed_fds.push_back(fd);
        }
      }

      for (auto fd : closed_fds)
      {
        auto handler = std::move(handlers_[fd]);
        handlers_.erase(fd);
        if (!handler->is_listener())
          on_close_(*this, handler->fd());
      }
    }
  };
}

#endif // JETBLACK_NET_POLLER_HPP
