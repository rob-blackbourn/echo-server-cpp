#ifndef __tcp_server_hpp
#define __tcp_server_hpp

#include <poll.h>

#include <functional>
#include <map>
#include <memory>
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
  const client_callback& _on_accept;
  const client_callback& _on_read;

public:
  tcp_server(
    uint16_t port,
    const client_callback& on_accept,
    const client_callback& on_read)
    : _on_accept(on_accept),
      _on_read(on_read)
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
          if (handle_event(poll_state))
            --active_fd_count;

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
      if (!client->is_write_queue_empty())
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
    _on_accept(client->fd(), client, _clients);
  }

  bool handle_read(int fd)
  {
    auto& client = _clients[fd];
    bool is_open = client->enqueue_reads();
    _on_read(fd, client, _clients);
    return is_open;
  }

  bool handle_event(const pollfd& poll_state)
  {
    if (poll_state.revents == 0)
    {
      // no events for file descriptor.
      return false;
    }

    if ((poll_state.revents & POLLIN) == POLLIN)
    {
      // Read event.
      if (poll_state.fd == _listener.fd())
      {
        handle_accept();
        return true;
      }

      try
      {
        // Client read
        bool is_open = handle_read(poll_state.fd);
        if (!is_open)
        {
          _clients.erase(poll_state.fd);
          return true;
        }
      }
      catch(const std::exception& e)
      {
        std::cerr << "failed to handle client read - " << e.what() << '\n';
      }
    }

    if ((poll_state.revents & POLLOUT) == POLLOUT)
    {
      try
      {
        // Client write
        bool is_open = _clients[poll_state.fd]->write();
        if (!is_open)
        {
          _clients.erase(poll_state.fd);
          return true;
        }
      }
      catch(const std::exception& e)
      {
        std::cerr << "failed to handle client write - " << e.what() << '\n';
      }
    }

    return true;
  }

};

#endif // __tcp_server_hpp
