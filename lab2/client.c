/*
 Chat client that uses select() to read from stdin and socket concurrently.
 Compile: gcc client.c -o client
 Run: ./client 127.0.0.1 8080
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);

    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    // create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return 1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address\n");
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        return 1;
    }

    printf("Connected to %s:%d. Type messages and press Enter to send.\n", server_ip, port);

    fd_set readfds;
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds); // stdin
        FD_SET(sock, &readfds);         // socket
        int maxfd = (sock > STDIN_FILENO) ? sock : STDIN_FILENO;

        int activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            perror("select");
            break;
        }

        // If input from stdin
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
                // EOF (Ctrl+D) -> exit
                printf("Leaving chat.\n");
                break;
            }
            // send to server
            send(sock, buffer, strlen(buffer), 0);
        }

        // If message from server
        if (FD_ISSET(sock, &readfds)) {
            memset(buffer, 0, BUFFER_SIZE);
            int n = read(sock, buffer, BUFFER_SIZE - 1);
            if (n <= 0) {
                printf("Server closed connection.\n");
                break;
            }
            // print message
            printf("%s", buffer);
            fflush(stdout);
        }
    }

    close(sock);
    return 0;
}
