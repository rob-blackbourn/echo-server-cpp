#ifndef JETBLACK_NET_TCP_SERVER_SOCKET_HPP
#define JETBLACK_NET_TCP_SERVER_SOCKET_HPP

#include <cstdint>
#include <string>

#include "tcp_socket.hpp"

namespace jetblack::net
{

  class tcp_server_socket : public tcp_socket
  {
  private:
    std::string address_;
    std::uint16_t port_;

  public:
    tcp_server_socket(int fd, const std::string& address, std::uint16_t port) noexcept
      : tcp_socket(fd)
      , address_(address)
      , port_(port)
    {
    }

    const std::string& address() const noexcept { return address_; }
    uint16_t port() const noexcept { return port_; }
  };

}

#endif // JETBLACK_NET_TCP_SERVER_SOCKET_HPP
