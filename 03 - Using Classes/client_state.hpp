#ifndef __client_state_hpp
#define __client_state_hpp

#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <map>
#include <vector>
#include <deque>
#include <optional>
#include <utility>

class tcp_socket
{
private:
    int _fd;

public:
    tcp_socket() {}
    tcp_socket(int fd) : _fd(fd) {}
    tcp_socket(const tcp_socket& other) : _fd(other._fd) {}

    int fd() const { return _fd; }

    bool set_blocking(bool is_blocking)
    {
        int flags = fcntl(_fd, F_GETFL, 0);
        if (flags == -1) return false;
        flags = is_blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
        return (fcntl(_fd, F_SETFL, flags) == 0);        
    }
};

class tcp_client : public tcp_socket
{
private:
    bool _is_open = true;
    
public:
    std::optional<std::vector<char>> read(std::vector<char>& buf)
    {
        ssize_t bytes_read = ::read(fd(), buf.data(), buf.size());

        // Check for an error.
        if (bytes_read == -1)
        {
            // Check if it's flow control.
            if (!(errno == EAGAIN || errno == EWOULDBLOCK))
            {
                // Not a flow control error; the socket has faulted.
                _is_open = false;
            }

            // Any error terminates the read session.
            return std::nullopt;
        }
        else if (bytes_read == 0)
        {
            // A read of zero bytes indicates socket has closed.
            _is_open = false;
            return std::nullopt;
        }
        else
        {
            // Data has been read successfully. Size the data to the amount read and add the data to the read queue.
            buf.resize(bytes_read);
            return buf;
        }
    }
};

const std::size_t NETWORK_READ_BUFSIZE = 8096;
const std::size_t NETWORK_WRITE_BUFSIZE = 8096;

class ClientState : public tcp_socket
{
private:
    std::size_t _read_bufsiz;
    std::deque<std::vector<char>> _read_queue;

    std::size_t _write_bufsiz;
    std::deque<std::pair<std::vector<char>,std::size_t>> _write_queue;

    bool _is_open = true;

public:
    ClientState() {}

    ClientState(
        int _fd,
        std::size_t read_bufsiz = NETWORK_READ_BUFSIZE,
        std::size_t write_bufsiz = NETWORK_WRITE_BUFSIZE)
        :   tcp_socket { _fd },
            _read_bufsiz { read_bufsiz },
            _write_bufsiz { write_bufsiz }
    {
    }

    bool can_write() const { return _is_open && !_write_queue.empty(); }

    std::optional<std::vector<char>> read()
    {
        if (_read_queue.empty())
            return std::nullopt;

        auto buf = _read_queue.front();
        _read_queue.pop_front();
        return buf;
    }

    bool enqueue_reads()
    {
        while (_is_open)
        {
            auto buf = std::vector<char>(_read_bufsiz);
            ssize_t bytes_read = ::read(fd(), buf.data(), buf.size());

            // Check for an error.
            if (bytes_read == -1)
            {
                // Check if it's flow control.
                if (!(errno == EAGAIN || errno == EWOULDBLOCK))
                {
                    // Not a flow control error; the socket has faulted.
                    _is_open = false;
                }

                // Any error terminates the read session.
                break;
            }
            else if (bytes_read == 0)
            {
                // A read of zero bytes indicates socket has closed.
                _is_open = false;
            }
            else
            {
                // Data has been read successfully. Size the data to the amount read and add the data to the read queue.
                buf.resize(bytes_read);
                _read_queue.push_back(std::move(buf));
            }
        }

        return _is_open;
    }

    void enqueue_write(std::vector<char> buf)
    {
        _write_queue.push_back(std::move(std::make_pair(std::move(buf), 0)));
    }

    bool write()
    {
        while (_is_open && !_write_queue.empty())
        {
            const auto& buf = _write_queue.front().first;
            auto& offset = _write_queue.front().second;
            ssize_t bytes_written = ::write(fd(), buf.data(), std::min(buf.size() - offset, _write_bufsiz));

            // Check for errors.
            if (bytes_written == -1)
            {
                // Check if it's flow control.
                if (!(errno == EAGAIN || errno == EWOULDBLOCK))
                {
                    // Not flow control; the socket has faulted.
                    _is_open = false;
                }

                // Any error terminates the write session.
                break;
            }

            if (bytes_written == 0)
            {
                // A write of zero bytes indicates socket has closed.
                _is_open = false;
            }
            else
            {
                // Some bytes were written. Manage the write queue.
                offset += bytes_written;
                if (offset == buf.size())
                {
                    // The buffer has been completely used. Remove it from the queue.
                    _write_queue.pop_front();
                }
            }
        }

        return _is_open;
    }
};

#endif // __client_state_hpp
