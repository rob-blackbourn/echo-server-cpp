#ifndef JETBLACK_NET_TCP_STREAM_HPP
#define JETBLACK_NET_TCP_STREAM_HPP

#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

#include <cerrno>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <system_error>
#include <variant>
#include <vector>
#include <utility>

#include "tcp_socket.hpp"
#include "ssl_ctx.hpp"

namespace jetblack::net
{

  struct eof {};
  struct blocked {};

  class TcpStream
  {
  public:
    typedef std::shared_ptr<TcpSocket> socket_pointer;

  private:
    BIO* bio_;
    std::optional<std::shared_ptr<SslContext>> ssl_ctx_;


  public:
    socket_pointer socket;

  public:
    TcpStream(socket_pointer socket, std::optional<std::shared_ptr<SslContext>> ssl_ctx) noexcept
      : bio_(BIO_new_socket(socket->fd(), BIO_NOCLOSE)),
        ssl_ctx_(ssl_ctx),
        socket(std::move(socket))
    {
    }
    ~TcpStream()
    {
      BIO_free_all(bio_);
    }

    std::variant<std::vector<char>, eof, blocked> read(std::size_t len)
    {
      std::vector<char> buf(len);

      std::size_t nbytes_read;
      int result = BIO_read_ex(bio_, buf.data(), len, &nbytes_read);
      if (result == 0) {
        // Check if we can retry.
        if (!(BIO_should_retry(bio_))) {
          // The socket has faulted.
          socket->is_open(false);
          return eof {};
        }

        // The socket is ok, but nothing has been read due to blocking.
        return blocked {};
      }

      if (nbytes_read == 0) {
        // A read of zero bytes indicates socket has closed.
        socket->is_open(false);
        return eof {};
      }

      // Data has been read successfully. Resize the buffer and return.
      buf.resize(nbytes_read);
      return buf;
    }

    std::variant<std::size_t, eof, blocked> write(const std::span<char>& buf)
    {
      std::size_t nbytes_written;
      int result = BIO_write_ex(bio_, buf.data(), buf.size(), &nbytes_written);
      if (result == 0)
      {
        // Check if it's flow control.
        if (!BIO_should_retry(bio_)) {
          // Not flow control; the socket has faulted.
          socket->is_open(false);
          throw std::runtime_error("failed to write");
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
      return nbytes_written;
    }
  };

}

#endif // JETBLACK_NET_TCP_STREAM_HPP
