#ifndef __tcp_socket_hpp
#define __tcp_socket_hpp

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <system_error>

class tcp_socket
{
protected:
  int _fd;
  bool _is_open = true;

public:
  explicit tcp_socket()
    : _fd(socket(AF_INET, SOCK_STREAM, 0))
  {
    if (_fd == -1) {
      throw std::system_error(
        errno, std::generic_category(), "failed to create socket");
    }
  }

  explicit tcp_socket(int fd) noexcept
    : _fd(fd)
  {
  }

  tcp_socket(const tcp_socket&) = delete;

  ~tcp_socket() noexcept
  {
    if (!_is_open)
    {
      return;
    }

    try
    {
      close();
    }
    catch (...)
    {
      // Ignore all errors.
    }
  }

  void close()
  {
    int result = ::close(_fd);
    if (result == -1) {
      throw std::system_error(
        errno, std::generic_category(), "failed to close socket");
    }
    _is_open = false;
  }

  int fd() const noexcept { return _fd; }
  bool is_open() const noexcept { return _is_open; }

  int fcntl_flags() const
  {
    int flags = ::fcntl(_fd, F_GETFL, 0);
    if (flags == -1) {
      throw std::system_error(
        errno, std::generic_category(), "fcntl failed to get flags");
    }
    return flags;
  }

  void fcntl_flags(int flags)
  {
    if (::fcntl(_fd, F_SETFL, flags) == -1) {
      throw std::system_error(
        errno, std::generic_category(), "fcntl failed to set flags");
    }
  }

  void fcntl_flag(int flag, bool is_add)
  {
    int flags = fcntl_flags();
    flags = is_add ? (flags & ~flag) : (flags | flag);
    fcntl_flags(flags);
  }

  bool blocking() const { return (fcntl_flags() & O_NONBLOCK) == O_NONBLOCK; }
  void blocking(bool is_blocking) { fcntl_flag(O_NONBLOCK, is_blocking); }

  void set_option(int level, int name, bool is_set) const
  {
    int value = is_set ? 1 : 0;
    if (::setsockopt(_fd, level, name, (void*)&value, sizeof(value)) == -1) {
      throw std::system_error(
        errno, std::generic_category(), "fcntl failed to set flags");
    }
  }

  void reuseaddr(bool is_reusable)
  {
    set_option(SOL_SOCKET, SO_REUSEADDR, is_reusable);
  }
};

#endif // __tcp_socket_hpp
