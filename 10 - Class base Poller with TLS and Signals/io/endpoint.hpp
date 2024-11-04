#ifndef SQUAWKBUS_IO_ENDPOINT_HPP
#define SQUAWKBUS_IO_ENDPOINT_HPP

#include <cstdint>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>

namespace jetblack::io
{
  class Endpoint
  {
  private:
    std::string host_;
    std::uint16_t port_;

  public:
    Endpoint() : host_(""), port_(0)
    {
    }
    Endpoint(const std::string& host, std::uint16_t port) noexcept
      : host_(host),
        port_(port)
    {
    }

    const std::string& host() const noexcept { return host_; }
    std::uint16_t port() const noexcept { return port_; }

    operator std::string()
    {
      return std::format("{}:{}", host_, port_);
    }

    bool empty() const noexcept { return host_.empty() && port_ == 0; }

    static Endpoint parse(const std::string& text)
    {
      auto colon = text.find(':');
      if (colon == std::string::npos)
        throw std::runtime_error("invalid endpoint");

      auto host = text.substr(0, colon);
      auto port = std::stoi(text.substr(colon+1));

      if (port <= 0 || port > std::numeric_limits<std::uint16_t>::max())
        throw std::runtime_error("invalid port");

      return Endpoint(host, port);
    }
  };
}

#endif // SQUAWKBUS_IO_ENDPOINT_HPP
