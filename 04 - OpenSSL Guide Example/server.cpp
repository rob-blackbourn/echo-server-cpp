/*
 *  Copyright 2022-2024 The OpenSSL Project Authors. All Rights Reserved.
 *
 *  Licensed under the Apache License 2.0 (the "License").  You may not use
 *  this file except in compliance with the License.  You can obtain a copy
 *  in the file LICENSE in the source distribution or at
 *  https://www.openssl.org/source/license.html
 */

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

/*
 * This flag won't be useful until both accept/read (TCP & SSL) methods
 * can be called with a timeout. TBD.
 */
static volatile bool    server_running = true;

#define BUFFERSIZE 1024
int main(int argc, char **argv)
{
    /* ignore SIGPIPE so that server can continue running when client pipe closes abruptly */
    signal(SIGPIPE, SIG_IGN);

    /* Splash */
    printf("\nsslecho : Simple Echo Server : %s : %s\n\n", __DATE__, __TIME__);

    printf("We are the server on port: %d\n\n", server_port);

    /* Create server socket; will bind with server port and listen */
    int server_skt = socket(AF_INET, SOCK_STREAM, 0);
    if (server_skt < 0)
    {
        perror("Unable to create socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* Reuse the address; good for quick restarts */
    int optval = 1;
    if (setsockopt(server_skt, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }

    if (bind(server_skt, (struct sockaddr*) &addr, sizeof(addr)) < 0)
    {
        perror("Unable to bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_skt, 1) < 0)
    {
        perror("Unable to listen");
        exit(EXIT_FAILURE);
    }


    /* Create context used by both client and server */
    SSL_CTX* ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (ssl_ctx == nullptr)
    {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    /* Configure server context with appropriate key files */

    /* Set the key and cert */
    if (SSL_CTX_use_certificate_chain_file(ssl_ctx, "/home/rtb/.keys/server.crt") <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, "/home/rtb/.keys/server.key", SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    /*
    * Loop to accept clients.
    * Need to implement timeouts on TCP & SSL connect/read functions
    * before we can catch a CTRL-C and kill the server.
    */
    while (server_running)
    {
        /* Wait for TCP connection from client */
        struct sockaddr_in caddr;
        unsigned int caddr_len = sizeof(caddr);
        int client_skt = accept(server_skt, (struct sockaddr*) &caddr, &caddr_len);
        if (client_skt < 0)
        {
            perror("Unable to accept");
            exit(EXIT_FAILURE);
        }

        printf("Client TCP connection accepted\n");

        /* Create server SSL structure using newly accepted client socket */
        SSL* ssl = SSL_new(ssl_ctx);
        if (!SSL_set_fd(ssl, client_skt)) {
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }

        /* Wait for SSL connection from the client */
        if (SSL_accept(ssl) <= 0)
        {
            ERR_print_errors_fp(stderr);
            server_running = false;
        }
        else
        {
            printf("Client SSL connection accepted\n\n");

            /* Echo loop */
            while (true)
            {
                /* Get message from client; will fail if client closes connection */
                char rxbuf[128];
                int rxlen = rxlen = SSL_read(ssl, rxbuf, sizeof(rxbuf));
                if (rxlen <= 0)
                {
                    if (rxlen == 0)
                    {
                        printf("Client closed connection\n");
                    }
                    else
                    {
                        printf("SSL_read returned %d\n", rxlen);
                    }
                    ERR_print_errors_fp(stderr);
                    break;
                }
                /* Insure null terminated input */
                rxbuf[rxlen] = 0;
                /* Look for kill switch */
                if (strcmp(rxbuf, "kill\n") == 0) {
                    /* Terminate...with extreme prejudice */
                    printf("Server received 'kill' command\n");
                    server_running = false;
                    break;
                }
                /* Show received message */
                printf("Received: %s", rxbuf);
                /* Echo it back */
                if (SSL_write(ssl, rxbuf, rxlen) <= 0) {
                    ERR_print_errors_fp(stderr);
                }
            }
        }
        if (server_running) {
            /* Cleanup for next client */
            printf("Client shutdown\n");
            SSL_shutdown(ssl);
            SSL_free(ssl);
            int res = close(client_skt);
            printf("Client close retcode=%d\n", res);
        }
    }
    printf("Server exiting...\n");

    /* Close up */
    SSL_CTX_free(ssl_ctx);

    if (server_skt != -1)
        close(server_skt);

    printf("sslecho exiting\n");

    return EXIT_SUCCESS;
}
