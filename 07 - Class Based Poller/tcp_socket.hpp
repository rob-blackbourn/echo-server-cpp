#ifndef JETBLACK_NET_TCP_SOCKET_HPP
#define JETBLACK_NET_TCP_SOCKET_HPP

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <iostream>
#include <system_error>

namespace jetblack::net
{

  class TcpSocket
  {
  protected:
    int fd_;
    bool is_open_ { true };

  public:
    explicit TcpSocket()
      : fd_(socket(AF_INET, SOCK_STREAM, 0))
    {
      if (fd_ == -1) {
        throw std::system_error(
          errno, std::generic_category(), "failed to create socket");
      }
    }

    explicit TcpSocket(int fd) noexcept
      : fd_(fd)
    {
    }

    TcpSocket(const TcpSocket&) = delete;
    TcpSocket(TcpSocket&&) = delete;
    TcpSocket& operator = (const TcpSocket&) = delete;
    TcpSocket& operator = (TcpSocket&&) = delete;

    ~TcpSocket()
    {
      if (!is_open_)
      {
        try
        {
          close();
          is_open_ = false;
        }
        catch(const std::exception& e)
        {
        }
      }
    }

    int fd() const noexcept { return fd_; }

    bool is_open() const noexcept { return is_open_; }
    void is_open(bool value) { is_open_ = value; }
    
    void close()
    {
      int result = ::close(fd_);
      if (result == -1) {
        throw std::system_error(
          errno, std::generic_category(), "failed to close socket");
      }
    }

    int fcntl_flags() const
    {
      int flags = ::fcntl(fd_, F_GETFL, 0);
      if (flags == -1) {
        throw std::system_error(
          errno, std::generic_category(), "fcntl failed to get flags");
      }
      return flags;
    }

    void fcntl_flags(int flags)
    {
      if (::fcntl(fd_, F_SETFL, flags) == -1) {
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
      if (::setsockopt(fd_, level, name, (void*)&value, sizeof(value)) == -1) {
        throw std::system_error(
          errno, std::generic_category(), "fcntl failed to set flags");
      }
    }

    void reuseaddr(bool is_reusable)
    {
      set_option(SOL_SOCKET, SO_REUSEADDR, is_reusable);
    }
  };

}

#endif // JETBLACK_NET_TCP_SOCKET_HPP
