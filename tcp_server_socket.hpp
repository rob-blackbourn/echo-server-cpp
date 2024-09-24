#ifndef JETBLACK_NET_TCP_SERVER_SOCKET_HPP
#define JETBLACK_NET_TCP_SERVER_SOCKET_HPP

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

namespace jetblack
{
  namespace net
  {

    class tcp_server_socket : public tcp_socket
    {
    private:
      std::string address_;
      uint16_t port_;

    public:
      tcp_server_socket(int fd, const std::string& address, uint16_t port) noexcept
        : tcp_socket(fd)
        , address_(address)
        , port_(port)
      {
      }

      const std::string& address() const noexcept { return address_; }
      uint16_t port() const noexcept { return port_; }
    };

  }
}

#endif // JETBLACK_NET_TCP_SERVER_SOCKET_HPP
