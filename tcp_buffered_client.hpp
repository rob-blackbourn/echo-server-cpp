#ifndef __tcp_buffered_client_hpp
#define __tcp_buffered_client_hpp

#include <deque>
#include <optional>

#include "match.hpp"
#include "tcp_client.hpp"

class tcp_buffered_client : public tcp_client
{
private:
  std::deque<std::vector<char>> _read_queue;
  std::deque<std::pair<std::vector<char>, std::size_t>> _write_queue;

public:
  const std::size_t read_bufsiz;
  const std::size_t write_bufsiz;

  tcp_buffered_client(
    int fd,
    const std::string& address,
    uint16_t port,
    std::size_t read_bufsiz,
    std::size_t write_bufsiz)
    : tcp_client(fd, address, port),
      read_bufsiz(read_bufsiz),
      write_bufsiz(write_bufsiz)
  {
  }

  bool is_read_queue_empty() const noexcept { return _read_queue.empty(); }

  std::vector<char> dequeue_read() noexcept
  {
    auto buf { std::move(_read_queue.front()) };
    _read_queue.pop_front();
    return buf;
  }

  std::optional<std::vector<char>> read() noexcept
  {
    if (_read_queue.empty())
      return std::nullopt;

    auto buf = _read_queue.front();
    _read_queue.pop_front();
    return buf;
  }

  bool enqueue_reads()
  {
    bool ok = is_open();
    while (ok) {
      ok = std::visit(match {
        
        [](blocked&&)
        {
          return false;
        },

        [](eof&&)
        {
          return false;
        },

        [&](std::vector<char>&& buf) mutable
        {
          _read_queue.push_back(std::move(buf));
          return is_open();
        }

      },
      tcp_client::read(read_bufsiz));
    }
    return is_open();
  }

  bool is_write_queue_empty() const noexcept { return _write_queue.empty(); }

  void enqueue_write(std::vector<char> buf)
  {
    _write_queue.push_back(std::move(std::make_pair(std::move(buf), 0)));
  }

  bool write()
  {
    bool can_write = is_open() && !_write_queue.empty();
    while (can_write) {

      auto& [orig_buf, offset] = _write_queue.front();
      std::size_t count = std::min(orig_buf.size() - offset, write_bufsiz);
      const auto& buf = std::span<char>(orig_buf).subspan(offset, count);

      can_write = std::visit(match {
        
        [](eof&&)
        {
          return false;
        },

        [](blocked&&)
        {
          return false;
        },

        [&](ssize_t&& bytes_written) mutable
        {
          // Update the offset reference by the number of bytes written.
          offset += bytes_written;
          // Are we there yet?
          if (offset == orig_buf.size()) {
            // The buffer has been completely used. Remove it from the
            // queue.
            _write_queue.pop_front();
          }
          return is_open() && !_write_queue.empty();
        }
        
      },
      tcp_client::write(buf));
    }

    return is_open();
  }
};

#endif // __tcp_buffered_client_hpp
