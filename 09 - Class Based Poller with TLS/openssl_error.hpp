#ifndef JETBLACK_NET_OPENSSL_ERROR_HPP
#define JETBLACK_NET_OPENSSL_ERROR_HPP

#include <string>

#include "openssl/ssl.h"
#include "openssl/err.h"

namespace jetblack::net
{
  std::string openssl_strerror()
  {
    std::string str;
    auto error = ERR_get_error();
    while (error != 0)
    {
      error = ERR_get_error();
      char buf[2048];
      ERR_error_string_n(error, buf, sizeof(buf));
      str.append((char*)buf);
    }
    return str;
  }

  std::string ssl_strerror(int error)
  {
    switch (error)
    {
    case SSL_ERROR_NONE:
      return "NONE - operation completed";
    case SSL_ERROR_ZERO_RETURN:
      return "ZERO_RETURN - peer closed connection";
    case SSL_ERROR_WANT_READ:
      return "WANT_READ - a read is required";    
    case SSL_ERROR_WANT_WRITE:
      return "WANT_WRITE - a write is required";    
    case SSL_ERROR_WANT_ACCEPT:
      return "WANT_ACCEPT - an accept would block and should be retried";    
    case SSL_ERROR_WANT_CONNECT:
      return "WANT_CONNECT - a connect would block and should be retried";    
    case SSL_ERROR_WANT_X509_LOOKUP:
      return "WANT_X509_LOOKUP - the callback asked to be called again";    
    case SSL_ERROR_WANT_ASYNC:
      return "WANT_ASYNC - the async engine is still processing data";    
    case SSL_ERROR_WANT_ASYNC_JOB:
      return "WANT_ASYNC_JOB - an async job could not be created";    
    case SSL_ERROR_WANT_CLIENT_HELLO_CB:
      return "WANT_CLIENT_HELLO_CB - the callback asked to be called again";    
    case SSL_ERROR_SYSCALL:
      return "SYSCALL - unrecoverable";    
    case SSL_ERROR_SSL:
      return "SSL - unrecoverable";    
    default:
      return "SSL - unknown";
    }
  }
}

#endif // JETBLACK_NET_OPENSSL_ERROR_HPP
