#ifndef __tcp_buffered_stream_hpp
#define __tcp_buffered_stream_hpp

#include <deque>

#include "match.hpp"
#include "tcp_stream.hpp"

template<class TSocket>
class tcp_buffered_stream : public tcp_stream<TSocket>
{
private:
  std::deque<std::vector<char>> _read_queue;
  std::deque<std::pair<std::vector<char>, std::size_t>> _write_queue;

public:
  const std::size_t read_bufsiz;
  const std::size_t write_bufsiz;

  tcp_buffered_stream(
    std::shared_ptr<TSocket> socket,
    std::size_t read_bufsiz,
    std::size_t write_bufsiz)
    : tcp_stream<TSocket>(socket),
      read_bufsiz(read_bufsiz),
      write_bufsiz(write_bufsiz)
  {
  }

  bool can_read() const noexcept { return !_read_queue.empty(); }

  std::vector<char> read() noexcept
  {
    auto buf { std::move(_read_queue.front()) };
    _read_queue.pop_front();
    return buf;
  }

  bool enqueue_reads()
  {
    bool ok = tcp_stream<TSocket>::is_open();
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
          return tcp_stream<TSocket>::is_open();
        }

      },
      tcp_stream<TSocket>::read(read_bufsiz));
    }
    return tcp_stream<TSocket>::is_open();
  }

  bool has_writes() const noexcept { return !_write_queue.empty(); }

  void enqueue_write(std::vector<char> buf)
  {
    _write_queue.push_back(std::make_pair(std::move(buf), 0));
  }

  bool write_enqueued()
  {
    bool has_writes = tcp_stream<TSocket>::is_open() && !_write_queue.empty();
    while (has_writes) {

      auto& [orig_buf, offset] = _write_queue.front();
      std::size_t count = std::min(orig_buf.size() - offset, write_bufsiz);
      const auto& buf = std::span<char>(orig_buf).subspan(offset, count);

      has_writes = std::visit(match {
        
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
          return tcp_stream<TSocket>::is_open() && !_write_queue.empty();
        }
        
      },
      tcp_stream<TSocket>::write(buf));
    }

    return tcp_stream<TSocket>::is_open();
  }
};

#endif // __tcp_buffered_stream_hpp
