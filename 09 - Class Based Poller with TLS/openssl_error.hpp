#ifndef JETBLACK_NET_OPENSSL_ERROR_HPP
#define JETBLACK_NET_OPENSSL_ERROR_HPP

#include <string>

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

}

#endif // JETBLACK_NET_OPENSSL_ERROR_HPP
