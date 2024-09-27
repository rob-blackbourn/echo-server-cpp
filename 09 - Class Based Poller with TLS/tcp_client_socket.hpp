#ifndef JETBLACK_NET_TCP_CLIENT_SOCKET_HPP
#define JETBLACK_NET_TCP_CLIENT_SOCKET_HPP

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

#include "tcp_socket.hpp"

namespace jetblack::net
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
      char port_str[6];
      std::memset(port_str, 0, sizeof(port_str));
      auto res = std::to_chars(port_str, port_str + sizeof(port_str) - 1, port);
      if (res.ec != std::errc{})
      {
          throw std::system_error(std::make_error_code(res.ec));
      }

      addrinfo hints {
          .ai_flags = 0,
          .ai_family = AF_INET,
          .ai_socktype = SOCK_STREAM,
          .ai_protocol = IPPROTO_TCP,
          .ai_addrlen = 0,
          .ai_addr = nullptr,
          .ai_canonname = nullptr,
          .ai_next = nullptr
      };
      addrinfo* info;
      int result = getaddrinfo(host.c_str(), port_str, &hints, &info);
      if (result != 0)
      {
          throw std::runtime_error(gai_strerror(result));
      }

      sockaddr_in addr;
      std::memcpy(&addr, info->ai_addr, info->ai_addrlen);

      freeaddrinfo(info);

      connect(addr);
    }

  };
  
}

#endif // JETBLACK_NET_TCP_CLIENT_SOCKET_HPP
