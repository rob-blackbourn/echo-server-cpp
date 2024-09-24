#ifndef __tcp_client_hpp
#define __tcp_client_hpp

#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <optional>
#include <span>
#include <stdexcept>
#include <system_error>
#include <variant>
#include <vector>

#include "tcp_socket.hpp"

class tcp_client : public tcp_socket
{
private:
  std::string address_;
  uint16_t port_;

public:
  tcp_client(int fd, const std::string& address, uint16_t port) noexcept
    : tcp_socket(fd)
    , address_(address)
    , port_(port)
  {
  }

  const std::string& address() const noexcept { return address_; }
  uint16_t port() const noexcept { return port_; }
};

#endif // __tcp_client_hpp
