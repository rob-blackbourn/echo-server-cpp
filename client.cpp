#include <iostream>

#include "tcp_client_socket.hpp"

using namespace jetblack::net;

int main()
{
  const std::string host { "localhost" };
  const uint16_t port = 22000;

  try
  {
    auto socket = tcp_client_socket();
    socket.connect(host, port);

    while (1)
    {
        std::cout << "Enter a message: ";
        std::string message;
        std::cin >> message;

        const char* send_buf = message.c_str();
        std::size_t send_buf_len = message.size() + 1;
        std::cout << "Sending " << send_buf_len << " bytes for \"" << send_buf << "\"" << std::endl;
        write(socket.fd(), send_buf, send_buf_len);

        char buf[100];
        memset(buf, 0, sizeof(buf));
        ssize_t nbytes_read = read(socket.fd(), buf, sizeof(buf));
        std::cout << "Received " << nbytes_read << " bytes of \"" << buf << "\"" << std::endl;
    }

  }
  catch(const std::exception& e)
  {
    std::cerr << e.what() << '\n';
  }
  
  return 0;
}