#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

static const int server_port = 4433;

int main(int argc, char **argv)
{
    int result;

    struct sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    /* ignore SIGPIPE so that server can continue running when client pipe closes abruptly */
    signal(SIGPIPE, SIG_IGN);

    /* Splash */
    printf("\nsslecho : Simple Echo Client : %s : %s\n\n", __DATE__, __TIME__);

    /* Create "bare" socket */
    int client_skt = socket(AF_INET, SOCK_STREAM, 0);
    if (client_skt < 0)
    {
        perror("Unable to create socket");
        exit(EXIT_FAILURE);
    }

    /* Set up connect address */
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.1.1", &addr.sin_addr.s_addr);
    addr.sin_port = htons(server_port);
    /* Do TCP connect with server */
    if (connect(client_skt, (struct sockaddr*) &addr, sizeof(addr)) != 0)
    {
        perror("Unable to TCP connect to server");
        exit(EXIT_FAILURE);
    }

    printf("TCP connection to server successful\n");

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

    /* Create client SSL structure using dedicated client socket */
    SSL* ssl = SSL_new(ssl_ctx);
    if (!SSL_set_fd(ssl, client_skt))
    {
        ERR_print_errors_fp(stderr);
        goto exit;
    }
    /* Set hostname for SNI */
    SSL_set_tlsext_host_name(ssl, "beast.jetblack.net");
    /* Configure server hostname check */
    if (!SSL_set1_host(ssl, "beast.jetblack.net"))
    {
        ERR_print_errors_fp(stderr);
        goto exit;
    }

    /* Now do SSL connect with server */
    if (SSL_connect(ssl) == 1)
    {
        printf("SSL connection to server successful\n\n");

        /* Loop to send input from keyboard */
        while (true)
        {
            /* Get a line of input */
            char buffer[1024];
            memset(buffer, 0, sizeof(buffer));
            char* txbuf = fgets(buffer, sizeof(buffer), stdin);

            /* Exit loop on error */
            if (txbuf == nullptr) {
                break;
            }
            /* Exit loop if just a carriage return */
            if (txbuf[0] == '\n') {
                break;
            }
            /* Send it to the server */
            if ((result = SSL_write(ssl, txbuf, strlen(txbuf))) <= 0) {
                printf("Server closed connection\n");
                ERR_print_errors_fp(stderr);
                break;
            }

            /* Wait for the echo */
            char rxbuf[128];
            int rxlen = SSL_read(ssl, rxbuf, sizeof(rxbuf));
            if (rxlen <= 0) {
                printf("Server closed connection\n");
                ERR_print_errors_fp(stderr);
                break;
            } else {
                /* Show it */
                rxbuf[rxlen] = 0;
                printf("Received: %s", rxbuf);
            }
        }
        printf("Client exiting...\n");
    } else {

        printf("SSL connection to server failed\n\n");

        ERR_print_errors_fp(stderr);
    }

exit:
    /* Close up */
    if (ssl != nullptr) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
    SSL_CTX_free(ssl_ctx);

    if (client_skt != -1)
        close(client_skt);

    printf("sslecho exiting\n");

    return EXIT_SUCCESS;
}
