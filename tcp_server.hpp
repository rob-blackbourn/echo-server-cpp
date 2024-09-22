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

#include "tcp_buffered_client.hpp"

class tcp_server
{
public:
  typedef tcp_buffered_client client_type;
  typedef std::shared_ptr<client_type> client_pointer;
  typedef std::map<int, client_pointer> client_map;
  typedef std::function<void(int, const client_pointer&, const client_map&)> client_callback;
  
private:
  std::map<int, client_pointer> _clients;
  tcp_listener<tcp_buffered_client> _listener;
  std::optional<client_callback> _on_open;
  std::optional<client_callback> _on_read;
  std::optional<client_callback> _on_close;

public:
  tcp_server(
    uint16_t port,
    const std::optional<client_callback> on_open = std::nullopt,
    const std::optional<client_callback> on_read = std::nullopt,
    const std::optional<client_callback> on_close = std::nullopt)
    : _on_open(on_open),
      _on_read(on_read),
      _on_close(on_close)
  {
    _listener.bind(port);
    _listener.blocking(false);
    _listener.reuseaddr(true);
  }

  void event_loop(int backlog = 10)
  {
    try
    {
      _listener.listen(backlog);

      bool is_ok = true;

      while (is_ok) {

        std::vector<pollfd> fds = make_poll_fds();

        int active_fd_count = poll(fds);

        for (const auto& poll_state : fds)
        {
          if (poll_state.revents == 0)
          {
            // no events for file descriptor.
            continue;
          }

          --active_fd_count;

          handle_event(poll_state);

          if (active_fd_count == 0)
            break;
        }
      }
    }
    catch(const std::exception& e)
    {
      std::cerr << "Event loop failed - " << e.what() << '\n';
    }
  }

private:

  std::vector<pollfd> make_poll_fds()
  {
    std::vector<pollfd> fds;

    fds.push_back(pollfd{_listener.fd(), POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL, 0});
    for (auto& [client_fd, client] : _clients)
    {
      int16_t flags = POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL;
      if (!client->can_write())
          flags |= POLLOUT;

      fds.push_back(pollfd{client_fd, flags, 0});
    }

    return fds;
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

  void handle_accept()
  {
    auto client = _listener.accept(
      [](int fd, const std::string& addr, uint16_t port)
      {
        return std::make_shared<tcp_buffered_client>(fd, addr, port, 8096, 8096);
      }
    );
    client->blocking(false);
    _clients[client->fd()] = client;
    if (_on_open.has_value())
    {
      _on_open.value()(client->fd(), client, _clients);
    }
  }

  bool handle_read(int fd)
  {
    auto& client = _clients[fd];
    try
    {
      bool is_open = client->enqueue_reads();
      if (_on_read.has_value())
      {
        _on_read.value()(fd, client, _clients);
      }
      return is_open;
    }
    catch(const std::exception& e)
    {
      std::cerr << "read failed - " << e.what() << '\n';
      handle_close(fd);
      return false;
    }    
  }

  bool handle_write(int fd)
  {
    try
    {
      return _clients[fd]->write_enqueued();
    }
    catch(const std::exception& e)
    {
      std::cerr << "write failed - " << e.what() << '\n';
      return false;
    }
  }

  void handle_close(int fd)
  {
    auto client = _clients[fd];

    _clients.erase(fd);
    
    if (_on_close.has_value())
    {
      _on_close.value()(fd, client, _clients);
    }
  }

  void handle_event(const pollfd& poll_state)
  {
    if ((poll_state.revents & POLLIN) == POLLIN)
    {
      // Read events.

      // Listener read.
      if (poll_state.fd == _listener.fd())
      {
        // A read on a listening socket indicates a client can be accepted.
        handle_accept();
        // No further processing is necessary.
        return;
      }

      // Client read.
      bool is_open = handle_read(poll_state.fd);
      if (!is_open)
      {
        // The read detected a close. 
        handle_close(poll_state.fd);
        // No further events can be handled.
        return;
      }
    }

    if ((poll_state.revents & POLLOUT) == POLLOUT)
    {
      bool is_open = handle_write(poll_state.fd);
      if (!is_open)
      {
        // The write detected a close.
        handle_close(poll_state.fd);
        // No further events can be handled.
        return;
      }
    }
  }

};

#endif // __tcp_server_hpp
