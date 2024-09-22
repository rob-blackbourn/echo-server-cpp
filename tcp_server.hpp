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

template<class TClient = tcp_buffered_client>
class tcp_server
{
private:
  std::map<int, std::shared_ptr<TClient>> _clients;
  tcp_listener<TClient> _listener;

public:
  tcp_server(uint16_t port)
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

  virtual std::shared_ptr<TClient> on_accept(int fd, const std::string& addr, uint16_t port) = 0;

  virtual void on_read(int fd, const std::shared_ptr<TClient>& client) = 0;

  const std::map<int, std::shared_ptr<TClient>>& clients() const noexcept { return _clients; }

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
      [this](int fd, const std::string& addr, uint16_t port)
      {
        return this->on_accept(fd, addr, port);
      }
    );
    client->blocking(false);
    _clients[client->fd()] = client;
  }

  bool handle_read(int fd)
  {
    auto& client = _clients[fd];
    bool is_open = client->enqueue_reads();
    on_read(fd, client);
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
