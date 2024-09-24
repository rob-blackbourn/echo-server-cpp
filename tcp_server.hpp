#ifndef __tcp_server_hpp
#define __tcp_server_hpp

#include <poll.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "tcp_socket.hpp"
#include "tcp_client.hpp"
#include "tcp_listener.hpp"

#include "tcp_buffered_stream.hpp"

class tcp_server
{
public:
  typedef tcp_buffered_stream<tcp_client> stream_type;
  typedef std::shared_ptr<stream_type> stream_pointer;
  typedef std::map<int, stream_pointer> stream_map;
  typedef std::function<void(int, const stream_pointer&, const stream_map&)> stream_connection;
  typedef std::function<void(int, const stream_pointer&, const stream_map&, std::optional<std::exception>)> stream_io;
  
private:
  std::map<int, stream_pointer> streams_;
  tcp_listener<tcp_client> listener_;
  std::optional<stream_connection> on_open_;
  std::optional<stream_connection> on_close_;
  std::optional<stream_io> on_read_;
  std::optional<stream_io> on_write_;

public:
  tcp_server(
    uint16_t port,
    const std::optional<stream_connection> on_open = std::nullopt,
    const std::optional<stream_connection> on_close = std::nullopt,
    const std::optional<stream_io> on_read = std::nullopt,
    const std::optional<stream_io> on_write = std::nullopt)
    : on_open_(on_open),
      on_close_(on_close),
      on_read_(on_read),
      on_write_(on_write)
  {
    listener_.bind(port);
    listener_.blocking(false);
    listener_.reuseaddr(true);
  }

  void event_loop(int backlog = 10)
  {
    listener_.listen(backlog);

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

      remove_closed_streams();
    }
  }

private:

  std::vector<pollfd> make_poll_fds()
  {
    std::vector<pollfd> fds;

    // Add read for the listener in order to detect new connections.
    fds.push_back(pollfd{listener_.fd(), POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL, 0});

    for (auto& [client_fd, stream] : streams_)
    {
      // We are always interested in incoming data.
      int16_t flags = POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL;

      // We are interested in writes when we have data to write.
      if (stream->has_writes())
          flags |= POLLOUT;

      fds.push_back(pollfd{client_fd, flags, 0});
    }

    return fds;
  }

  void remove_closed_streams()
  {
    std::vector<int> closed_fds;
    
    for (auto& [fd, stream] : streams_)
    {
      if (!stream->socket->is_open())
      {
        closed_fds.push_back(fd);
      }
    }

    for (auto fd : closed_fds)
    {
      auto stream = streams_[fd];
      streams_.erase(fd);
      raise(on_close_, fd, stream, streams_);
    }
  }

  int poll(std::vector<pollfd> &fds)
  {
    int active_fd_count = ::poll(fds.data(), fds.size(), -1);
    if (active_fd_count < 0)
    {
      throw std::system_error(
        errno, std::generic_category(), "poll failed");
    }
    return active_fd_count;
  }

  void raise(const std::optional<stream_connection>& callback, int fd, const stream_pointer& stream, const stream_map& streams)
  {
    if (callback.has_value())
    {
      callback.value()(fd, stream, streams_);
    }
  }

  void handle_accept()
  {
    // Accept the client. This might throw an exception which will not be cached, as subsequent
    // connections will also fail.
    auto client = listener_.accept(
      [](int fd, const std::string& addr, uint16_t port)
      {
        return std::make_shared<tcp_client>(fd, addr, port);
      }
    );
    client->blocking(false);

    auto stream = std::make_shared<tcp_buffered_stream<tcp_client>>(client, 8096, 8096);
    streams_[client->fd()] = stream;
    raise(on_open_, client->fd(), stream, streams_);
  }

  void raise(const std::optional<stream_io>& callback, int fd, const stream_pointer& stream, const stream_map& streams, std::optional<std::exception> error)
  {
    if (callback.has_value())
    {
      callback.value()(fd, stream, streams_, error);
    }
  }

  bool handle_read(int fd)
  {
    auto& stream = streams_[fd];
    try
    {
      bool is_open = stream->enqueue_reads();
      raise(on_read_, fd, stream, streams_, std::nullopt);
      return is_open;
    }
    catch(const std::exception& e)
    {
      raise(on_read_, fd, stream, streams_, std::nullopt);
      return false;
    }    
  }

  bool handle_write(int fd)
  {
    auto& stream = streams_[fd];

    try
    {
      bool is_open = stream->write_enqueued();
      raise(on_write_, fd, stream, streams_, std::nullopt);
      return is_open;
    }
    catch(const std::exception& e)
    {
      raise(on_write_, fd, stream, streams_, std::nullopt);
      return false;
    }
  }

  void handle_event(const pollfd& poll_state)
  {
    if ((poll_state.revents & POLLIN) == POLLIN)
    {
      if (poll_state.fd == listener_.fd())
      {
        // A read on a listening socket indicates a client can be accepted.
        handle_accept();
        // No further processing is necessary.
        return;
      }

      if (!handle_read(poll_state.fd))
      {
        // No further events can be handled for this stream.
        return;
      }
    }

    if ((poll_state.revents & POLLOUT) == POLLOUT)
    {
      if (!handle_write(poll_state.fd))
      {
        // No further events can be handled for this stream.
        return;
      }
    }
  }

};

#endif // __tcp_server_hpp
