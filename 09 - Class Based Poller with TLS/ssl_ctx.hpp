#ifndef JETBLACK_NET_SSL_CTX_HPP
#define JETBLACK_NET_SSL_CTX_HPP

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <stdexcept> 
#include <string>
#include <utility>

namespace jetblack::net
{
  class SslContext
  {
  protected:
    SSL_CTX* ctx_;
  public:
    SslContext() = delete;
    SslContext(const SSL_METHOD* method)
      : ctx_(SSL_CTX_new(method))
    {
    }
    ~SslContext()
    {
      SSL_CTX_free(ctx_);
    }
    SslContext(const SslContext&) = delete;
    SslContext(SslContext&& other)
    {
      ctx_ = other.ctx_;
      other.ctx_ = nullptr;
    }
    SslContext& operator=(const SslContext&) = delete;
    SslContext& operator=(SslContext&& other)
    {
      ctx_ = other.ctx_;
      other.ctx_ = nullptr;
      return *this;
    }
  };

  class SslClientContext : public SslContext
  {
  public:
    SslClientContext()
      : SslContext(TLS_client_method())
    {
    }
    SslClientContext(SslClientContext&& other)
      : SslContext(std::move(other))
    {
    }
    SslClientContext& operator = (SslClientContext&& other)
    {
      SslContext::operator=(std::move(other));
      return *this;
    }

    void verify(int mode = SSL_VERIFY_PEER)
    {
      SSL_CTX_set_verify(ctx_, mode, nullptr);
    }

    void load_verify_locations(const std::string& path)
    {
      if (SSL_CTX_load_verify_locations(ctx_, path.c_str(), nullptr) == 0)
      {
        auto error = ERR_get_error();
        char buf[1024];
        ERR_error_string_n(error, buf, sizeof(buf));
        throw std::runtime_error(std::string(static_cast<const char*>(buf)));
      }
    }

    void set_default_verify_paths()
    {
      if (SSL_CTX_set_default_verify_paths(ctx_) == 0)
      {
        auto error = ERR_get_error();
        char buf[1024];
        ERR_error_string_n(error, buf, sizeof(buf));
        throw std::runtime_error(std::string(static_cast<const char*>(buf)));
      }
    }
  };

  class SslServerContext : public SslContext
  {
  public:
    SslServerContext()
      : SslContext(TLS_server_method())
    {
    }
    SslServerContext(SslServerContext&& other)
      : SslContext(std::move(other))
    {
    }
    SslServerContext& operator = (SslServerContext&& other)
    {
      SslContext::operator=(std::move(other));
      return *this;
    }
    
    void use_certificate_file(const std::string& path, int type = SSL_FILETYPE_PEM)
    {
      if (SSL_CTX_use_certificate_file(ctx_, path.c_str(), type) <= 0)
      {
        auto error = ERR_get_error();
        char buf[1024];
        ERR_error_string_n(error, buf, sizeof(buf));
        throw std::runtime_error(std::string(static_cast<const char*>(buf)));
      }
    }
    
    void use_certificate_chain_file(const std::string& path)
    {
      if (SSL_CTX_use_certificate_chain_file(ctx_, path.c_str()) <= 0)
      {
        auto error = ERR_get_error();
        char buf[1024];
        ERR_error_string_n(error, buf, sizeof(buf));
        throw std::runtime_error(std::string(static_cast<const char*>(buf)));
      }
    }
    
    void use_private_key_file(const std::string& path, int type = SSL_FILETYPE_PEM)
    {
      if (SSL_CTX_use_PrivateKey_file(ctx_, path.c_str(), type) <= 0)
      {
        auto error = ERR_get_error();
        char buf[1024];
        ERR_error_string_n(error, buf, sizeof(buf));
        throw std::runtime_error(std::string(static_cast<const char*>(buf)));
      }
    }
  };
}

#endif // JETBLACK_NET_SSL_CTX_HPP
