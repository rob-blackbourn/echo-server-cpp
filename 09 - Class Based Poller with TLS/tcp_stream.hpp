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
    SSL* ssl_;
    bool handshake_done {false};

  public:
    socket_pointer socket;

  public:
    TcpStream(
      socket_pointer socket,
      std::optional<std::shared_ptr<SslContext>> ssl_ctx,
      bool is_client) noexcept
      : bio_(BIO_new_socket(socket->fd(), BIO_NOCLOSE)),
        socket(std::move(socket))
    {
      if (!ssl_ctx.has_value())
      {
        ssl_ = nullptr;
      }
      else
      {
        BIO* ssl_bio = BIO_new_ssl(ssl_ctx.value()->ptr(), is_client ? 1 : 0);
        BIO_push(ssl_bio, bio_);
        bio_ = ssl_bio;
        BIO_get_ssl(bio_, &ssl_);
      }
    }
    ~TcpStream()
    {
      BIO_free_all(bio_); // This should free bio and ssl.
      bio_ = nullptr;
      ssl_ = nullptr;
    }

    bool want_read() const noexcept { return BIO_should_read(bio_); }
    bool want_write() const noexcept { return BIO_should_write(bio_); }

    bool do_handshake()
    {
      std::cerr << "do_handshake" << std::endl;
      if (ssl_ == nullptr || handshake_done)
      {
        std::cerr << "not required" << std::endl;
        return true;
      }

      int ret = SSL_do_handshake(ssl_);
      if (ret == 1)
      {
        std::cerr << "ssl handshake done" << std::endl;
        handshake_done = true;
        return true;
      }

      int error = SSL_get_error(ssl_, ret);
      switch (error)
      {
      case SSL_ERROR_NONE:
        std::cerr << "ssl error NONE" << std::endl;
        return false;
      case SSL_ERROR_ZERO_RETURN:
        std::cerr << "ssl error ZERO RETURN" << std::endl;
        return false;
      case SSL_ERROR_WANT_READ:
        std::cerr << "ssl error WANT READ" << std::endl;
        return false;
      case SSL_ERROR_WANT_WRITE:
        std::cerr << "ssl error WANT WRITE" << std::endl;
        return false;
      case SSL_ERROR_WANT_CONNECT:
        std::cerr << "ssl error WANT CONNECT" << std::endl;
        throw std::runtime_error("ssl want connect");
      case SSL_ERROR_WANT_ACCEPT:
        std::cerr << "ssl error WANT ACCEPT" << std::endl;
        throw std::runtime_error("ssl want connect");
      case SSL_ERROR_WANT_X509_LOOKUP:
        std::cerr << "ssl error WANT X509 lookup" << std::endl;
        throw std::runtime_error("ssl want X509 lookup");
      case SSL_ERROR_WANT_ASYNC:
        std::cerr << "ssl error WANT ASYNC" << std::endl;
        throw std::runtime_error("ssl want async");
      case SSL_ERROR_WANT_ASYNC_JOB:
        std::cerr << "ssl error WANT ASYNC JOB" << std::endl;
        throw std::runtime_error("ssl want async job");
      case SSL_ERROR_WANT_CLIENT_HELLO_CB:
        std::cerr << "ssl error WANT CLIENT HELLO CB" << std::endl;
        throw std::runtime_error("ssl want client hello cb");
      case SSL_ERROR_SYSCALL:
        std::cerr << "ssl error SYSCALL" << std::endl;
        throw std::runtime_error("ssl syscall");
      case SSL_ERROR_SSL:
        std::cerr << "ssl error SSL" << std::endl;
        throw std::runtime_error("ssl error");
      default:
        std::cerr << "ssl error unknown" << std::endl;
        throw std::runtime_error("ssl error unknown");
      }
    }

    std::variant<std::vector<char>, eof, blocked> read(std::size_t len)
    {
      if (!do_handshake())
        return blocked {};

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
      if (!do_handshake())
        return blocked {};

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
