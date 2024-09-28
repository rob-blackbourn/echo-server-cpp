#include <iostream>
#include <memory>
#include <span>
#include <utility>
#include <variant>

#include "tcp_client_socket.hpp"
#include "tcp_stream.hpp"

#include "external/popl.hpp"

#include "match.hpp"
#include "utils.hpp"

using namespace jetblack::net;

int main(int argc, char** argv)
{
  popl::OptionParser op("options");
  bool use_tls = false;
  op.add<popl::Switch>("s", "ssl", "Connect with TLS", &use_tls);
  auto help_option = op.add<popl::Switch>("", "help", "produce help message");
  std::uint16_t port = 22000;
  op.add<popl::Value<decltype(port)>>("p", "port", "port number", port, &port);
  std::string host = "localhost";
  op.add<popl::Value<decltype(host)>>("h", "host", "host name or ip address (use fqdn for tls)", host, &host);

  try
  {
    op.parse(argc, argv);

    if (help_option->is_set())
    {
      if (help_option->count() == 1)
    		std::cout << op << "\n";
	    else if (help_option->count() == 2)
		    std::cout << op.help(popl::Attribute::advanced) << "\n";
	    else
		    std::cout << op.help(popl::Attribute::expert) << "\n";
      exit(1);
    }

    auto socket = std::make_unique<TcpClientSocket>();
    socket->connect(host, port);

    auto stream = TcpStream(std::move(socket));

    while (1)
    {
      std::cout << "Enter a message: ";
      std::string message;
      std::cin >> message;

      std::vector<char> send_buf{message.begin(), message.end()};
      std::cout << "Sending \"" << send_buf << "\"" << std::endl;
      send_buf.push_back('\0'); // null terminate.
      bool write_ok = std::visit(
        match
        {
          [](eof&&)
          {
            std::cout << "eof" << std::endl;
            return false;
          },

          [](blocked&&)
          {
            std::cout << "block" << std::endl;
            return false;
          },

          [](ssize_t&& bytes_written) mutable
          {
            std::cout << "read " << bytes_written << " bytes" << std::endl;
            return true;
          }            
        },
        stream.write(send_buf));
      if (!write_ok)
      {
        std::cout << "write failed - quitting" << std::endl;
        break;
      }

      std::vector<char> read_buf;
      bool read_ok = std::visit(
        match
        {
          [](blocked&&)
          {
            std::cout << "read blocked" << std::endl;
            return false;
          },

          [](eof&&)
          {
            std::cout << "read blocked" << std::endl;
            return false;
          },

          [&](std::vector<char>&& buf) mutable
          {
            std::cout << "read ok \"" << buf << "\"" << std::endl;
            read_buf = std::move(buf);
            return true;
          }
        },
        stream.read(1024));
      if (!read_ok)
      {
        std::cout << "read failed - quitting" << std::endl;
      }
    }

  }
  catch(const std::exception& e)
  {
    std::cerr << e.what() << '\n';
  }
  
  return 0;
}