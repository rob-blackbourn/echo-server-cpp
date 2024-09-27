#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <iostream>
#include <cerrno>
#include <cstring>

int main(int argc, char** argv)
{
    const uint16_t port = 22000;

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1)
    {
        std::cerr
            << "failed to create listener socket: "
            << std::strerror(errno)
            << std::endl;
        return 1;
    }

    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(listen_fd, (sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        std::cerr
            << "failed to bind listener socket to port " << port << ": "
            << std::strerror(errno)
            << std::endl;
        return 1;
    }

    if (listen(listen_fd, 10) == -1)
    {
        std::cerr
            << "failed to listen to bound socket: "
            << std::strerror(errno)
            << std::endl;
        return 1;
    }

    SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (ssl_ctx == nullptr)
    {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_certificate_file(ssl_ctx, "/home/rtb/.keys/server.crt", SSL_FILETYPE_PEM) <= 0)
    {
        perror("Error loading server certificate");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
  
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, "/home/rtb/.keys/server.key", SSL_FILETYPE_PEM) <= 0)
    {
        perror("Error loading server private key");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    int client_fd = accept(listen_fd, (sockaddr *)nullptr, nullptr);
    if (client_fd == -1)
    {
        std::cerr
            << "failed to accept client socket: "
            << std::strerror(errno)
            << std::endl;
        return 1;
    }

    BIO* bio = BIO_new_socket(client_fd, BIO_NOCLOSE);

    BIO* ssl_bio = BIO_new_ssl(ssl_ctx, 0);
    BIO_push(ssl_bio, bio);

    SSL *ssl = nullptr;
    BIO_get_ssl(ssl_bio, &ssl);

    std::cout << "server handshake ..." << std::endl;
    if (BIO_do_handshake(ssl_bio) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }    
    std::cout << "... succeeded" << std::endl;

    bool is_ok = true;
    while (is_ok)
    {
        char buf[100];
        memset(buf, 0, sizeof(buf));

        size_t nbytes_read;
        int read_retcode = BIO_read_ex(ssl_bio, buf, sizeof(buf), &nbytes_read);
        std::cout << "read_retcode " << read_retcode << std::endl;
        if (read_retcode == 0)
        {
            if  (BIO_should_retry(ssl_bio))
            {
                std::cout << "should retry" << std::endl;
                if (BIO_should_read(ssl_bio))
                {
                    std::cout << "should read" << std::endl;
                }
                if (BIO_should_write(ssl_bio))
                {
                    std::cout << "should write" << std::endl;
                }
            }
            else
            {
                std::cout << "Can't retry" << std::endl;
            }
            std::cout << "Shutting down" << std::endl;
            BIO_ssl_shutdown(ssl_bio);
            break;
        }
        std::cout << "Received " << (int)nbytes_read << " bytes of \"" << buf << "\"" << std::endl;
        if (nbytes_read <= 0)
        {
            std::cerr << "Exiting read" << std::endl;
            is_ok = false;
            continue;
        }

        if (strcmp(buf, "KILL") == 0)
        {
            std::cout << "Shutdown client socket ..." << std::endl;
            BIO_ssl_shutdown(ssl_bio);
            std:: cout << "... done" << std::endl;
            break;
        }

        std::cout << "Echoing back - " << buf << std::endl;
        std::cout << "Writing " << strlen(buf) + 1 << std::endl;
        size_t nbytes_written;
        int write_retcode = BIO_write_ex(ssl_bio, buf, strlen(buf) + 1, &nbytes_written);
        std::cout << "write_retcode " << write_retcode << std::endl;
        std::cout << "Wrote " << (int)nbytes_written <<" bytes" << std::endl;
        BIO_flush(ssl_bio);
    }

    return 0;
}
