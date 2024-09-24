#ifndef __tcp_stream_hpp
#define __tcp_stream_hpp

#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <system_error>
#include <variant>
#include <vector>

#include "tcp_socket.hpp"

struct eof {};
struct blocked {};

class tcp_stream
{
public:
  typedef std::shared_ptr<tcp_socket> socket_pointer;

public:
  tcp_stream(socket_pointer socket) noexcept
    : socket(socket)
  {
  }

  socket_pointer socket;

  std::variant<std::vector<char>, eof, blocked> read(std::size_t len)
  {
    std::vector<char> buf(len);
    int result = ::read(socket->fd(), buf.data(), len);
    if (result == -1) {
      // Check if it's flow control.
      if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
        // Not a flow control error; the socket has faulted.
        socket->is_open(false);
        throw std::system_error(
          errno, std::generic_category(), "client socket failed to read");
      }

      // The socket is ok, but nothing has been read due to blocking.
      return blocked {};
    }

    if (result == 0) {
      // A read of zero bytes indicates socket has closed.
      socket->is_open(false);
      return eof {};
    }

    // Data has been read successfully. Resize the buffer and return.
    buf.resize(result);
    return buf;
  }

  std::variant<ssize_t, eof, blocked> write(const std::span<char>& buf)
  {
    int result = ::write(socket->fd(), buf.data(), buf.size());
    if (result == -1)
    {
      // Check if it's flow control.
      if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
        // Not flow control; the socket has faulted.
        socket->is_open(false);
        throw std::system_error(
          errno, std::generic_category(), "client socket failed to write");
      }

      // The socket is ok, but nothing has been written due to blocking.
      return blocked {};
    }

    if (result == 0)
    {
      // A write of zero bytes indicates socket has closed.
      socket->is_open(false);
      return eof {};
    }

    // return the number of bytes that were written.
    return result;
  }
};

#endif // __tcp_stream_hpp
