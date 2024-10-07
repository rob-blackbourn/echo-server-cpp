#ifndef JETBLACK_NET_FILE_HPP
#define JETBLACK_NET_FILE_HPP

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

  class File
  {
  protected:
    int fd_;
    bool is_open_ { true };

  public:
    explicit File(int fd) noexcept
      : fd_(fd)
    {
    }

    File(const File&) = delete;
    File& operator = (const File&) = delete;

    File(File&& other)
    {
      *this = std::move(other);
    }

    File& operator = (File&& other)
    {
      fd_ = other.fd_;
      is_open_ = other.is_open_;

      other.fd_ = -1;
      other.is_open_ = false;

      return *this;
    }

    ~File()
    {
      if (!is_open_)
      {
        try
        {
          close();
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
        throw std::system_error(errno, std::generic_category(), "failed to close socket");
      }
      is_open_ = false;
    }

    int fcntl_flags() const
    {
      int flags = ::fcntl(fd_, F_GETFL, 0);
      if (flags == -1) {
        throw std::system_error(errno, std::generic_category(), "fcntl failed to get flags");
      }
      return flags;
    }

    void fcntl_flags(int flags)
    {
      if (::fcntl(fd_, F_SETFL, flags) == -1) {
        throw std::system_error(errno, std::generic_category(), "fcntl failed to set flags");
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
  };

}

#endif // JETBLACK_NET_FILE_HPP
