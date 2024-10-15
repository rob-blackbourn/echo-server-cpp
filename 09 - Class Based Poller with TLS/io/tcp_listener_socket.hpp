#ifndef JETBLACK_IO_TCP_LISTENER_SOCKET_HPP
#define JETBLACK_IO_TCP_LISTENER_SOCKET_HPP

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <system_error>

#include "io/tcp_socket.hpp"
#include "io/tcp_server_socket.hpp"

namespace jetblack::io
{

  class TcpListenerSocket : public TcpSocket
  {
  public:
    typedef std::shared_ptr<TcpServerSocket> client_pointer;

  public:
    TcpListenerSocket()
      : TcpSocket()
    {
    }

    TcpListenerSocket(int) = delete;

    void bind(uint16_t port) {
      bind(htonl(INADDR_ANY), port);
    }

    void bind(const std::string& address, uint16_t port)
    {
      in_addr addr;
      int result = inet_pton(AF_INET, address.c_str(), &addr);
      if (result == 0) {
        throw std::runtime_error("invalid network address");
      } else {
        throw std::system_error(errno, std::generic_category(), "failed to parse network address");
      }

      bind(addr.s_addr, port);
    }

    void bind(uint32_t addr, uint16_t port)
    {
      sockaddr_in servaddr;
      std::memset(&servaddr, 0, sizeof(servaddr));
      servaddr.sin_family = AF_INET;
      servaddr.sin_addr.s_addr = addr;
      servaddr.sin_port = htons(port);

      if (::bind(fd_, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr)) == -1) {
        throw std::system_error(
          errno, std::generic_category(), "failed to bind listener socket");
      }
    }

    void listen(int backlog = 10)
    {
      if (::listen(fd_, backlog) == -1) {
        throw std::system_error(errno, std::generic_category(), "failed to listen on bound socket");
      }
    }

    client_pointer accept()
    {
      sockaddr_in clientaddr;
      socklen_t clientlen = sizeof(clientaddr);

      int client_fd = ::accept(fd_, reinterpret_cast<sockaddr*>(&clientaddr), &clientlen);
      if (client_fd == -1) {
        throw std::system_error(
          errno, std::generic_category(), "failed to accept socket");
      }

      char address[INET_ADDRSTRLEN];
      if (inet_ntop(AF_INET, &clientaddr.sin_addr, address, sizeof(address)) == nullptr)
      {
        throw std::system_error(
          errno, std::generic_category(), "failed to find client address");
      }
      uint16_t port = ntohs(clientaddr.sin_port);

      return std::make_shared<TcpServerSocket>(client_fd, address, port);
    }
  };

}

#endif
