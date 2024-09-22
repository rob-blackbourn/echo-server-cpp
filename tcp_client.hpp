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
  std::string _address;
  uint16_t _port;

public:
  tcp_client(int fd, const std::string& address, uint16_t port) noexcept
    : tcp_socket(fd)
    , _address(address)
    , _port(port)
  {
  }

  const std::string& address() const noexcept { return _address; }
  uint16_t port() const noexcept { return _port; }
};

#endif // __tcp_client_hpp
