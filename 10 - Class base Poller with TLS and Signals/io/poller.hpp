#ifndef SQUAWKBUS_IO_POLLER_HPP
#define SQUAWKBUS_IO_POLLER_HPP

#include <poll.h>
#include <signal.h>

#include <csignal>
#include <deque>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <system_error>
#include <utility>
#include <vector>

#include "io/logger.hpp"
#include "io/poll_handler.hpp"

namespace jetblack::io
{
  inline int poll(std::vector<pollfd> &fds, int timeout = -1)
  {
    log.trace("polling");

    int active_fd_count = ::poll(fds.data(), fds.size(), timeout);
    if (active_fd_count < 0)
    {
      if (errno == EINTR)
        return 0; // raising a caught signal causes this behaviour.
      throw std::system_error(errno, std::generic_category(), "poll failed");
    }
    return active_fd_count;
  }

  struct PollClient
  {
    virtual ~PollClient() {}
    virtual void on_startup(Poller& poller) = 0;
    virtual void on_interrupt(Poller& poller) = 0;
    virtual void on_open(Poller& poller, int fd, const std::string& host, std::uint16_t port) = 0;
    virtual void on_close(Poller& poller, int fd) = 0;
    virtual void on_read(Poller& poller, int fd, std::vector<std::vector<char>>&& bufs) = 0;
    virtual void on_error(Poller& poller, int fd, std::exception error) = 0;
  };

  class Poller
  {
  public:
    typedef std::unique_ptr<PollHandler> handler_pointer;
    typedef std::map<int, handler_pointer> handler_map;
    typedef std::shared_ptr<PollClient> client_pointer;

  private:
    handler_map handlers_;

    inline static sig_atomic_t last_signal_ = 0;

  public:
    std::optional<std::function<void()>> on_startup;
    std::optional<std::function<void()>> on_interrupt;
    std::optional<std::function<void(int fd, const std::string& host, std::uint16_t port)>> on_open;
    std::optional<std::function<void(int fd)>> on_close;
    std::optional<std::function<void(int fd, std::vector<std::vector<char>>&& bufs)>> on_read;
    std::optional<std::function<void(int fd, std::exception error)>> on_error;

  public:
    Poller()
    {
    }

    void add_handler(handler_pointer handler, const std::string& host, std::uint16_t port) noexcept
    {
      int fd = handler->fd();
      bool is_listener = handler->is_listener();
      handlers_[fd] = std::move(handler);
      if (!is_listener && on_open)
        (*on_open)(fd, host, port);
    }

    void write(int fd, const std::vector<char>& buf) noexcept
    {
      if (auto i = handlers_.find(fd); i != handlers_.end())
      {
        i->second->enqueue(buf);
      }
    }

    void close(int fd) noexcept
    {
      if (auto i = handlers_.find(fd); i != handlers_.end())
      {
        i->second->close();
      }
    }

    void event_loop(int backlog = 10)
    {
      if (on_startup)
        (*on_startup)();

      while (true) {

        std::vector<pollfd> fds = make_poll_fds();

        int active_fd_count = poll(fds, 1000);

        if (Poller::last_signal_ != 0)
        {
          Poller::last_signal_ = 0;
          try
          {
            if (on_interrupt)
              (*on_interrupt)();
          }
          catch (...)
          {
          }
        }

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

    static void register_signal(int signum)
    {
      struct sigaction action;
      action.sa_handler = &handle_signal;
      sigemptyset(&action.sa_mask);
      action.sa_flags = 0;
      if (sigaction(SIGINT, &action, nullptr) != 0)
        throw std::system_error(errno, std::generic_category(), "failed to set signal");
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
      log.trace(std::format("handling read for {}", handler->fd()));

      try
      {
        auto can_continue = handler->read(*this);

        std::vector<std::vector<char>> bufs;
        auto buf = handler->dequeue();
        while (buf)
        {
          bufs.push_back(*buf);
          buf = handler->dequeue();
        }

        if (!bufs.empty())
        {
          if (on_read)
            (*on_read)(handler->fd(), std::move(bufs));
        }

        return can_continue;
      }
      catch(const std::exception& error)
      {
        if (on_error)
          (*on_error)(handler->fd(), error);
        return false;
      }
    }

    bool handle_write(PollHandler* handler) noexcept
    {
      log.trace(std::format("handling write for {}", handler->fd()));

      try
      {
        return handler->write();
      }
      catch(const std::exception& error)
      {
        if (on_error)
          (*on_error)(handler->fd(), error);
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
      auto closed_fds = find_closed_handler_fds();
      remove_closed_handlers(closed_fds);
    }

    std::vector<int> find_closed_handler_fds()
    {
      std::vector<int> closed_fds;
      for (auto& [fd, handler] : handlers_)
      {
        if (!handler->is_open())
        {
          closed_fds.push_back(fd);
        }
      }
      return closed_fds;
    }

    void remove_closed_handlers(const std::vector<int>& closed_fds)
    {
      for (auto fd : closed_fds)
      {
        auto handler = std::move(handlers_[fd]);
        handlers_.erase(fd);
        if (!handler->is_listener() && on_close)
          (*on_close)(fd);
      }
    }

    static void handle_signal(int signum)
    {
      Poller::last_signal_ = signum;
    }

  };
}

#endif // SQUAWKBUS_IO_POLLER_HPP
