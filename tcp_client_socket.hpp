#ifndef JETBLACK_NET_TCP_CLIENT_SOCKET_HPP
#define JETBLACK_NET_TCP_CLIENT_SOCKET_HPP

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <exception>
#include <string>
#include <system_error>

#include "tcp_socket.hpp"

namespace jetblack::net
{
  class tcp_client_socket :public tcp_socket
  {
  public:
    tcp_client_socket()
      : tcp_socket()
    {
    }

    tcp_client_socket(int) = delete;

    void connect(const sockaddr_in& address)
    {
      if (::connect(fd_, (struct sockaddr *)&address, sizeof(address)) == -1)
      {
        throw std::system_error(
          errno, std::generic_category(), "failed to connect");
      }
    }

    void connect(const in_addr& address, uint16_t port)
    {
      sockaddr_in servaddr;
      std::memset(&servaddr, 0, sizeof(servaddr));
      servaddr.sin_family = AF_INET;
      std::memcpy(&servaddr.sin_addr, &address, sizeof(address));
      servaddr.sin_port = htons(port);

      connect(servaddr);
    }

    void connect(const std::string& address, std::uint16_t port)
    {
      in_addr addr;
      if (inet_pton(AF_INET, address.c_str(), &addr) == 0)
      {
        throw std::runtime_error("Failed to connect");
      }

      connect(addr, port);
    }
  };
  
}

#endif // JETBLACK_NET_TCP_CLIENT_SOCKET_HPP
