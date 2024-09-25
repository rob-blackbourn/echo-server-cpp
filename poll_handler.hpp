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

  class Poller
  {
  public:
    typedef std::shared_ptr<PollHandler> handler_pointer;
    typedef std::map<int, handler_pointer> poll_handler_map;
    typedef std::function<void(Poller&, int fd)> connection_callback;
    typedef std::function<void(Poller&, int fd, std::vector<std::vector<char>> bufs)> read_callback;
    typedef std::function<void(Poller&, int fd, std::exception)> error_callback;

  private:
    std::map<int, handler_pointer> handlers_;
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

    void add_handler(std::shared_ptr<PollHandler> handler) noexcept
    {
      handlers_[handler->fd()] = handler;
      if (!handler->is_listener())
        on_open_(*this, handler->fd());
    }

    void enqueue(int fd, std::vector<char> buf) noexcept
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
      auto handler = handlers_[poll_state.fd];

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

    bool handle_read(const handler_pointer& handler) noexcept
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

        on_read_(*this, handler->fd(), bufs);

        return can_continue;
      }
      catch(const std::exception& error)
      {
        on_error_(*this, handler->fd(), error);
        return false;
      }
    }

    bool handle_write(const handler_pointer& handler) noexcept
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
        auto handler = handlers_[fd];
        handlers_.erase(fd);
        if (!handler->is_listener())
          on_close_(*this, handler->fd());
      }
    }
  };

  class TcpServerSocketPollHandler : public PollHandler
  {
  private:
    TcpStream stream_;
    typedef TcpStream::buffer_type buffer_type;
    std::deque<buffer_type> read_queue_;
    std::deque<std::pair<buffer_type, std::size_t>> write_queue_;

  public:
    const std::size_t read_bufsiz;
    const std::size_t write_bufsiz;

    TcpServerSocketPollHandler(
      std::shared_ptr<TcpSocket> socket,
      std::size_t read_bufsiz,
      std::size_t write_bufsiz)
      : stream_(socket),
        read_bufsiz(read_bufsiz),
        write_bufsiz(write_bufsiz)
    {
    }
    ~TcpServerSocketPollHandler() override
    {
    }

    bool is_listener() const noexcept override { return false; }
    int fd() const noexcept override { return stream_.socket->fd(); }
    bool is_open() const noexcept override { return stream_.socket->is_open(); }
    bool want_read() const noexcept override { return stream_.socket->is_open(); }
    bool want_write() const noexcept override { return stream_.socket->is_open() && !write_queue_.empty(); }

    bool read(Poller& poller) noexcept override
    {
      bool ok = stream_.socket->is_open();
      while (ok) {
        ok = std::visit(match {
          
          [](blocked&&)
          {
            return false;
          },

          [](eof&&)
          {
            return false;
          },

          [&](buffer_type&& buf) mutable
          {
            read_queue_.push_back(std::move(buf));
            return stream_.socket->is_open();
          }

        },
        stream_.read(read_bufsiz));
      }
      return stream_.socket->is_open();
    }

    bool write() noexcept override
    {
      bool can_write = stream_.socket->is_open() && !write_queue_.empty();
      while (can_write) {

        auto& [orig_buf, offset] = write_queue_.front();
        std::size_t count = std::min(orig_buf.size() - offset, write_bufsiz);
        const auto& buf = std::span<char>(orig_buf).subspan(offset, count);

        can_write = std::visit(match {
          
          [](eof&&)
          {
            return false;
          },

          [](blocked&&)
          {
            return false;
          },

          [&](ssize_t&& bytes_written) mutable
          {
            // Update the offset reference by the number of bytes written.
            offset += bytes_written;
            // Are we there yet?
            if (offset == orig_buf.size()) {
              // The buffer has been completely used. Remove it from the
              // queue.
              write_queue_.pop_front();
            }
            return stream_.socket->is_open() && !write_queue_.empty();
          }
          
        },
        stream_.write(buf));
      }

      return stream_.socket->is_open();
    }

    bool has_reads() const noexcept { return !read_queue_.empty(); }

    std::optional<std::vector<char>> dequeue() noexcept override
    {
      if (read_queue_.empty())
        return std::nullopt;

      auto buf { std::move(read_queue_.front()) };
      read_queue_.pop_front();
      return buf;
    }

    void enqueue(std::vector<char> buf) noexcept override
    {
      write_queue_.push_back(std::make_pair(std::move(buf), 0));
    }
  };

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

      poller.add_handler(std::static_pointer_cast<PollHandler>(std::make_shared<TcpServerSocketPollHandler>(client, 8096, 8096)));

      return true;
    }

    bool write() noexcept override { return false; }

    std::optional<std::vector<char>> dequeue() noexcept override { return std::nullopt; }
    void enqueue(std::vector<char> buf) noexcept override {}
  };

}

#endif // JETBLACK_NET_POLL_HANDLER_HPP
