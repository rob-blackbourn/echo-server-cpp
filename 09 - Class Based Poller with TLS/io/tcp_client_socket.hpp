#ifndef JETBLACK_IO_TCP_CLIENT_SOCKET_HPP
#define JETBLACK_IO_TCP_CLIENT_SOCKET_HPP

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <array>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <system_error>

#include "io/tcp_socket.hpp"
#include "io/tcp_address.hpp"

namespace jetblack::io
{
  class TcpClientSocket :public TcpSocket
  {
  public:
    TcpClientSocket()
      : TcpSocket()
    {
    }

    TcpClientSocket(int) = delete;

    void connect(const sockaddr_in& address)
    {
      if (::connect(fd_, (struct sockaddr *)&address, sizeof(address)) == -1)
      {
        throw std::system_error(
          errno, std::generic_category(), "failed to connect");
      }
    }

    void connect(const in_addr& host, uint16_t port)
    {
      sockaddr_in addr;
      std::memset(&addr, 0, sizeof(addr));
      addr.sin_family = AF_INET;
      std::memcpy(&addr.sin_addr, &host, sizeof(host));
      addr.sin_port = htons(port);

      connect(addr);
    }

    void connect(const std::string& host, std::uint16_t port)
    {
      auto addresses = getaddrinfo_inet4(host, port);
      if (addresses.empty())
      {
        throw std::runtime_error("failed to resolve address");
      }
      connect(addresses.front());
    }

  };
  
}

#endif // JETBLACK_IO_TCP_CLIENT_SOCKET_HPP
