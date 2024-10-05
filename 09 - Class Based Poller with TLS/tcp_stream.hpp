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
#include "openssl_error.hpp"

namespace jetblack::net
{

  struct blocked {};
  struct eof {};


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
    SSL* ssl_;
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
        ssl_ = nullptr;
        state_ = State::DATA;
      }
      else
      {
        state_ = State::HANDSHAKE;
        int is_client = server_name.has_value() ? 1 : 0;
        BIO* ssl_bio = BIO_new_ssl(ssl_ctx.value()->ptr(), is_client);
        BIO_push(ssl_bio, bio_);
        bio_ = ssl_bio;
        BIO_get_ssl(bio_, &ssl_);

        if (server_name.has_value())
        {
          // Set hostname for SNI.
          SSL_set_tlsext_host_name(ssl_, server_name->c_str());

          // Configure server hostname check.
          if (!SSL_set1_host(ssl_, server_name->c_str()))
          {
            throw std::runtime_error("failed to configure hostname check");
          }
        }
      }
    }

    ~TcpStream()
    {
      BIO_free_all(bio_); // This should free bio and ssl.
      bio_ = nullptr;
      ssl_ = nullptr;
    }

    bool want_read() const noexcept { return BIO_should_read(bio_); }
    bool want_write() const noexcept{ return BIO_should_write(bio_); }

    bool do_handshake()
    {
      if (ssl_ == nullptr || state_ != State::HANDSHAKE)
      {
        return true; // continue processing reads.
      }

      int ret = SSL_do_handshake(ssl_);
      if (ret == 1)
      {
        state_ = State::DATA;
        return true;  // continue processing data.
      }

      int error = SSL_get_error(ssl_, ret);
      if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE)
      {
        return false; // Wait for next io event.
      }

      std::string errstr = openssl_strerror();
      std::string ssl_errstr = ssl_strerror(error);
      throw std::runtime_error("ssl handshake failed: " + errstr + " - " + ssl_errstr);
    }

    void verify()
    {
      int err = SSL_get_verify_result(ssl_);
      if (err != X509_V_OK)
      {
        std::string message = X509_verify_cert_error_string(err);
        throw std::runtime_error(message);
      }

      X509 *cert = SSL_get_peer_certificate(ssl_);
      if (cert == nullptr) {
          throw std::runtime_error("No certificate was presented by the server");
      }      
    }

    bool shutdown()
    {
      if (ssl_ != nullptr)
      {
        int retcode = SSL_shutdown(ssl_);
        if (retcode < 0)
        {
          auto error = SSL_get_error(ssl_, retcode);
          auto str = ssl_strerror(error);
          throw std::runtime_error("failed to shutdown: " + str);
        }
        return retcode == 1; // true if shutdown is complete.
      }
      else
      {
        return true;
      }

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

      if (ssl_ == nullptr)
      {
        state_ = State::STOP;
        return true;
      }

      if (ssl_ == nullptr || state_ != State::SHUTDOWN)
      {
        return true;
      }

      int retcode = SSL_shutdown(ssl_);

      if (retcode < 0)
      {
        // The shutdown failed.
        socket->is_open(false);
        return true; // nothing more to do.
      }

      if (retcode == 1)
      {
        // The shutdown succeeded.
        socket->is_open(false);
        return true; // nothing more to do.
      }

      return false; // more work to be done.
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
        if (!do_shutdown())
          return blocked {};
        else
          return eof {};
      }

      std::vector<char> buf(len);

      std::size_t nbytes_read;
      int result = BIO_read_ex(bio_, buf.data(), len, &nbytes_read);
      if (result == 0) {
        int ssl_err = SSL_get_error(ssl_, result);
        std::string errstr = ssl_strerror(ssl_err);;
        std::cout << "eof: " << errstr << std::endl;
        // Check if we can retry.
        if (!(BIO_should_retry(bio_))) {
          if (ssl_ != nullptr && SSL_get_error(ssl_, result) == SSL_ERROR_ZERO_RETURN)
          {
            // Try to do an SSL shutdown.
            state_ = State::SHUTDOWN;
            if (!do_shutdown())
              return blocked {};
            else
              return eof {};
          }
          else
          {
            // The socket has faulted.
            handle_client_faulted();
            socket->is_open(false);
            return eof {};
          }
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
      if (state_ == State::HANDSHAKE)
      {
        if (!do_handshake())
          return blocked {};
      }

      if (state_ == State::SHUTDOWN)
      {
        if (!do_shutdown())
          return blocked {};
        else
          return eof {};
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
      if (ssl_ != nullptr)
      {
        // This stops BIO_free_all (via SSL_SHUTDOWN) from raising SIGPIPE.
        SSL_set_shutdown(ssl_, SSL_SENT_SHUTDOWN);
        SSL_set_quiet_shutdown(ssl_, 1);
      }
    }
  };

}

#endif // JETBLACK_NET_TCP_STREAM_HPP
