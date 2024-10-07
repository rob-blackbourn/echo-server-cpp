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
#include <utility>
#include <variant>
#include <vector>

#include "match.hpp"

#include "tcp_socket.hpp"
#include "tcp_types.hpp"
#include "ssl_ctx.hpp"
#include "ssl.hpp"
#include "openssl_error.hpp"

namespace jetblack::net
{
  class TcpStream
  {
  public:
    typedef std::shared_ptr<TcpSocket> socket_pointer;
    enum class State
    {
      START,
      HANDSHAKE,
      DATA,
      SHUTDOWN,
      STOP
    };

  private:
    BIO* bio_;
    std::optional<Ssl> ssl_;
    State state_ { State::START };

  public:
    socket_pointer socket;

  public:
    TcpStream(
      socket_pointer socket,
      std::optional<std::shared_ptr<SslContext>> ssl_ctx,
      std::optional<std::string> server_name)
      : bio_(BIO_new_socket(socket->fd(), BIO_NOCLOSE)),
        socket(std::move(socket))
    {
      if (!ssl_ctx.has_value())
      {
        ssl_ = std::nullopt;
        state_ = State::DATA;
      }
      else
      {
        state_ = State::HANDSHAKE;
        int is_client = server_name.has_value() ? 1 : 0;
        BIO* ssl_bio = BIO_new_ssl(ssl_ctx.value()->ptr(), is_client);
        BIO_push(ssl_bio, bio_);
        bio_ = ssl_bio;
        SSL* ssl = nullptr;
        BIO_get_ssl(bio_, &ssl);
        ssl_ = Ssl(ssl, false);

        if (server_name.has_value())
        {
          // Set hostname for SNI.
          ssl_->tlsext_host_name(server_name.value());
          ssl_->host(server_name.value());
        }
      }
    }

    ~TcpStream()
    {
      BIO_free_all(bio_); // This should free bio and ssl.
      bio_ = nullptr;
    }

    bool want_read() const noexcept { return BIO_should_read(bio_); }
    bool want_write() const noexcept{ return BIO_should_write(bio_); }

    bool do_handshake()
    {
      if (!ssl_.has_value() || state_ != State::HANDSHAKE)
      {
        return true; // continue processing reads.
      }

      bool is_done = std::visit(
        match {
          
          [](blocked&&)
          {
            return false;
          },
          
          [](bool is_complete)
          {
            return is_complete;
          }
        },
        ssl_->do_handshake()
      );

      if (is_done)
      {
        state_ = State::DATA;
      }

      return is_done;
    }

    void verify()
    {
      ssl_->verify();
    }

    bool do_shutdown()
    {
      if (state_ == State::DATA)
      {
        // Transition to shutdown state.
        state_ = State::SHUTDOWN;
      }

      if (state_ == State::STOP)
      {
        return true;
      }

      if (state_ != State::SHUTDOWN)
      {
        throw std::runtime_error("shutdown in invalid state");
      }

      if (!ssl_.has_value())
      {
        state_ = State::STOP;
        return true;
      }

      if (!ssl_.has_value() || state_ != State::SHUTDOWN)
      {
        return true;
      }

      return handle_shutdown();
    }

    std::variant<std::vector<char>, eof, blocked> read(std::size_t len)
    {
      if (state_ == State::HANDSHAKE)
      {
        if (!do_handshake())
          return blocked {};
      }

      if (state_ == State::SHUTDOWN)
      {
        if (handle_shutdown())
          return eof {};
        else
          return blocked {};
      }

      std::vector<char> buf(len);

      std::size_t nbytes_read;
      int result = BIO_read_ex(bio_, buf.data(), buf.size(), &nbytes_read);
      if (result == 0) {
        if (BIO_should_retry(bio_))
        {
          // The socket is ok, but nothing has been read due to blocking.
          return blocked {};
        }

        if (ssl_.has_value() && ssl_->error(result) == SSL_ERROR_ZERO_RETURN)
        {
          // The client has initiated an SSL shutdown.
          state_ = State::SHUTDOWN;
          if (handle_shutdown())
            return eof {};
          else
            return blocked {};
        }

        // The socket has faulted.
        handle_client_faulted();
        socket->is_open(false);
        return eof {};
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
      if (state_ == State::HANDSHAKE)
      {
        if (!do_handshake())
          return blocked {};
      }

      if (state_ == State::SHUTDOWN)
      {
        if (handle_shutdown())
          return eof {};
        else
          return blocked {};
      }

      std::size_t nbytes_written;
      int result = BIO_write_ex(bio_, buf.data(), buf.size(), &nbytes_written);
      if (result == 0)
      {
        // Check if it's flow control.
        if (!BIO_should_retry(bio_)) {
          // Not flow control; the socket has faulted.
          socket->is_open(false);
          handle_client_faulted();
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

  private:
    void handle_client_faulted()
    {
      if (ssl_.has_value())
      {
        // This stops BIO_free_all (via SSL_SHUTDOWN) from raising SIGPIPE.
        ssl_->quiet_shutdown(true);
      }
    }

    bool handle_shutdown()
    {
      if (!ssl_.has_value())
      {
        state_ = State::STOP;
        return true;
      }

      bool is_done = std::visit(
        match {

          [](blocked&&)
          {
            return false;
          },

          [](bool is_completed)
          {
            return is_completed;
          }

        },
        ssl_->shutdown()
      );

      if (is_done)
      {
        socket->is_open(false);
      }

      return is_done;
    }
  };
}

#endif // JETBLACK_NET_TCP_STREAM_HPP
