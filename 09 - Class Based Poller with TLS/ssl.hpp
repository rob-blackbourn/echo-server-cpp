#ifndef JETBLACK_NET_SSL_HPP
#define JETBLACK_NET_SSL_HPP

#include <format>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

namespace jetblack::net
{

  class Ssl
  {
  public:

    enum class Error
    {
      NONE = SSL_ERROR_NONE,
      SSL = SSL_ERROR_SSL,
      WANT_READ = SSL_ERROR_WANT_READ,
      WANT_WRITE = SSL_ERROR_WANT_WRITE,
      WANT_X509_LOOKUP = SSL_ERROR_WANT_X509_LOOKUP,
      SYSCALL = SSL_ERROR_SYSCALL,
      ZERO_RETURN = SSL_ERROR_ZERO_RETURN,
      WANT_CONNECT = SSL_ERROR_WANT_CONNECT,
      WANT_ACCEPT = SSL_ERROR_WANT_ACCEPT,
      WANT_ASYNC = SSL_ERROR_WANT_ASYNC,
      WANT_ASYNC_JOB = SSL_ERROR_WANT_ASYNC_JOB,
      WANT_CLIENT_HELLO_CB = SSL_ERROR_WANT_CLIENT_HELLO_CB,
      WANT_RETRY_VERIFY = SSL_ERROR_WANT_RETRY_VERIFY
    };

  private:
    SSL* ssl_;
    bool is_owner_;

  public:
    Ssl(SSL* ssl, bool is_owner)
      : ssl_(ssl),
        is_owner_(is_owner)
    {
    }
    ~Ssl()
    {
      if (is_owner_)
      {
        SSL_free(ssl_);
      }
      ssl_ = nullptr;
    }
    Ssl(const Ssl&) = delete;
    Ssl& operator=(const Ssl&) = delete;
    Ssl(Ssl&& other)
    {
      *this = std::move(other);
    }
    Ssl& operator=(Ssl&& other)
    {
      ssl_ = other.ssl_;
      is_owner_ = other.is_owner_;
      other.ssl_ = nullptr;
      return *this;
    }

    void tlsex_host_name(const std::string& host_name)
    {
      // Set hostname for SNI.
      if (SSL_set_tlsext_host_name(ssl_, host_name.c_str()) != 1)
      {
        throw std::runtime_error("failed to set host name for SNI");
      }
    }

    void host(const std::string& host_name)
    {
      if (!SSL_set1_host(ssl_, host_name.c_str()))
      {
        throw std::runtime_error("failed to configure hostname check");
      }
    }

    Error error(int ret) const noexcept
    {
      return static_cast<Error>(SSL_get_error(ssl_, ret));
    }

    static const char* error_code(Error error)
    {
      switch (error)
      {
      case Error::NONE:
        return "NONE";
      case Error::ZERO_RETURN:
        return "ZERO_RETURN";
      case Error::WANT_READ:
        return "WANT_READ";
      case Error::WANT_WRITE:
        return "WANT_WRITE";
      case Error::WANT_ACCEPT:
        return "WANT_ACCEPT";
      case Error::WANT_CONNECT:
        return "WANT_CONNECT";
      case Error::WANT_X509_LOOKUP:
        return "WANT_X509_LOOKUP";
      case Error::WANT_ASYNC:
        return "WANT_ASYNC";
      case Error::WANT_ASYNC_JOB:
        return "WANT_ASYNC_JOB";
      case Error::WANT_CLIENT_HELLO_CB:
        return "WANT_CLIENT_HELLO_CB";
      case Error::SYSCALL:
        return "SYSCALL";
      case Error::SSL:
        return "SSL";
      default:
        return "UNKNOWN";
      }
    }

    static const char* error_description(Error error)
    {
      switch (error)
      {
      case Error::NONE:
        return "operation completed";
      case Error::ZERO_RETURN:
        return "peer closed connection";
      case Error::WANT_READ:
        return "a read is required";    
      case Error::WANT_WRITE:
        return "a write is required";    
      case Error::WANT_ACCEPT:
        return "an accept would block and should be retried";    
      case Error::WANT_CONNECT:
        return "a connect would block and should be retried";    
      case Error::WANT_X509_LOOKUP:
        return "the callback asked to be called again";    
      case Error::WANT_ASYNC:
        return "the async engine is still processing data";    
      case Error::WANT_ASYNC_JOB:
        return "an async job could not be created";    
      case Error::WANT_CLIENT_HELLO_CB:
        return "the callback asked to be called again";    
      case Error::SYSCALL:
        return "unrecoverable";    
      case Error::SSL:
        return "unrecoverable";    
      default:
        return "unknown";
      }
    }

    Error do_handshake()
    {
      int ret = SSL_do_handshake(ssl_);
      if (ret == 1)
      {
        return Error::NONE;
      }

      auto err = error(ret);

      if (ret == 0)
      {
        return err;
      }

      std::string message = std::format(
        "handshake failed: {} - {}",
        error_code(err),
        error_description(err));
      throw std::runtime_error(message);
    }

    const X509* peer_certificate() const
    {
      return SSL_get_peer_certificate(ssl_);
    }

    void verify()
    {
      int err = SSL_get_verify_result(ssl_);
      if (err != X509_V_OK)
      {
        std::string message = X509_verify_cert_error_string(err);
        throw std::runtime_error(message);
      }

      if (peer_certificate() == nullptr) {
          throw std::runtime_error("no certificate was presented");
      }
    }
  };

}

#endif JETBLACK_NET_SSL_HPP
