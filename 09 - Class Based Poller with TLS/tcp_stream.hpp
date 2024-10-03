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
      std::optional<std::string> server_name)
      : bio_(BIO_new_socket(socket->fd(), BIO_NOCLOSE)),
        socket(std::move(socket))
    {
      if (!ssl_ctx.has_value())
      {
        ssl_ = nullptr;
      }
      else
      {
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
    bool want_write() const noexcept { return BIO_should_write(bio_); }

    bool do_handshake()
    {
      if (ssl_ == nullptr || handshake_done)
      {
        return true; // continue processing reads.
      }

      int ret = SSL_do_handshake(ssl_);
      if (ret == 1)
      {
        handshake_done = true;
        return true;  // continue processing reads.
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
          handle_client_faulted();
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
      }
    }
  };

}

#endif // JETBLACK_NET_TCP_STREAM_HPP
