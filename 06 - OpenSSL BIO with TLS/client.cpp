#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <iostream>
#include <cerrno>
#include <cstring>
#include <string>

int main(int argc, char **argv)
{
    const char* host_address = "127.0.0.1";
    const char* host_name = "beast.jetblack.net";
    const uint16_t port = 22000;

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1)
    {
        std::cerr
            << "failed to create client socket: "
            << std::strerror(errno)
            << std::endl;
        return 1;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, host_address, &(servaddr.sin_addr));

    if (connect(client_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        std::cerr
            << "failed to connect socket to port " << port << ": "
            << std::strerror(errno)
            << std::endl;
        return 1;
    }

    BIO* bio = BIO_new_socket(client_fd, BIO_NOCLOSE);

    SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (ssl_ctx == nullptr)
    {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    /*
     * Configure the client to abort the handshake if certificate verification
     * fails
     */
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, nullptr);

    /*
     * In a real application you would probably just use the default system certificate trust store and call:
     *     SSL_CTX_set_default_verify_paths(ssl_ctx);
     * In this demo though we are using a self-signed certificate, so the client must trust it directly.
     */
    if (!SSL_CTX_load_verify_locations(ssl_ctx, "/home/rtb/.keys/chain-certs.crt", nullptr))
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    BIO* ssl_bio = BIO_new_ssl(ssl_ctx, 1);
    BIO_push(ssl_bio, bio);

    SSL *ssl = nullptr;
    BIO_get_ssl(ssl_bio, &ssl);

    /* Set hostname for SNI */
    SSL_set_tlsext_host_name(ssl, host_name);

    /* Configure server hostname check */
    if (!SSL_set1_host(ssl, host_name))
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    std::cout << "client handshake ..." << std::endl;
    if (BIO_do_handshake(ssl_bio) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }    
    std::cout << "... succeeded" << std::endl;

    while (1)
    {
        std::cout << "Enter a message: ";
        std::string message;
        std::cin >> message;

        if (message == "SHUTDOWN")
        {
            std::cout << "Shutting down ..." << std::endl;
            BIO_ssl_shutdown(ssl_bio);
            std::cout << "... done" << std::endl;
            break;
        }

        const char* send_buf = message.c_str();
        std::size_t send_buf_len = message.size() + 1;
        std::cout << "Sending " << send_buf_len << " bytes for \"" << send_buf << "\"" << std::endl;
        size_t nbytes_written;
        int write_retcode = BIO_write_ex(ssl_bio, send_buf, send_buf_len, &nbytes_written);
        std::cout << "write_retcode " << write_retcode << std::endl;
        BIO_flush(ssl_bio);

        char buf[100];
        memset(buf, 0, sizeof(buf));
        size_t nbytes_read;
        int read_retcode = BIO_read_ex(ssl_bio, buf, sizeof(buf), &nbytes_read);
        if (read_retcode == 0)
        {
            std::cout << "Shutting down" << std::endl;
            BIO_ssl_shutdown(ssl_bio);
            break;
        }
        std::cout << "read_retcode " << read_retcode << std::endl;
        std::cout << "Received " << (int)nbytes_read << " bytes of \"" << buf << "\"" << std::endl;
    }
}
