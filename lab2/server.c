/*
 Multi-client chat server using select()
 Compile: gcc server.c -o server
 Run: ./server
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

int main() {
    int server_fd, new_socket, client_socket[MAX_CLIENTS];
    int max_sd, sd, activity, valread;
    struct sockaddr_in address;
    socklen_t addrlen;
    fd_set readfds;
    char buffer[BUFFER_SIZE];

    // initialize all client_socket[] to 0 (not checked)
    for (int i = 0; i < MAX_CLIENTS; i++) client_socket[i] = 0;

    // create listening socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // allow address reuse
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // bind
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // listen
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("=== Chat server started on port %d ===\n", PORT);

    while (1) {
        // clear the socket set
        FD_ZERO(&readfds);

        // add listening socket to set
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        // add client sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_socket[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // wait for activity
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            perror("select error");
        }

        // if something on the listening socket -> new connection
        if (FD_ISSET(server_fd, &readfds)) {
            socklen_t addrlen = sizeof(address);
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
                perror("accept");
                continue;
            }

            // add new socket to array
            int added = 0;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_socket[i] == 0) {
                    client_socket[i] = new_socket;
                    added = 1;
                    break;
                }
            }
            if (!added) {
                // server full
                const char *msg = "Server full. Try later.\n";
                send(new_socket, msg, strlen(msg), 0);
                close(new_socket);
            } else {
                printf("New connection: socket fd %d, IP %s, PORT %d\n",
                       new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                const char *welcome = "Welcome to the chat!\n";
                send(new_socket, welcome, strlen(welcome), 0);
            }
        }

        // else check all clients for data
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_socket[i];
            if (sd == 0) continue;
            if (FD_ISSET(sd, &readfds)) {
                // incoming message
                memset(buffer, 0, BUFFER_SIZE);
                valread = read(sd, buffer, BUFFER_SIZE - 1);
                if (valread <= 0) {
                    // client disconnected
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    printf("Client disconnected: socket fd %d\n", sd);
                    close(sd);
                    client_socket[i] = 0;
                } else {
                    // trim newline from end if present
                    if (buffer[valread-1] == '\n') buffer[valread-1] = '\0';

                    // construct broadcast message (simple prefix)
                    char outmsg[BUFFER_SIZE + 64];
                    snprintf(outmsg, sizeof(outmsg), "Client %d: %s\n", sd, buffer);

                    // broadcast to all other clients
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        int out_sd = client_socket[j];
                        if (out_sd != 0 && out_sd != sd) {
                            send(out_sd, outmsg, strlen(outmsg), 0);
                        }
                    }
                    // also print on server console
                    printf("%s", outmsg);
                }
            }
        }
    }

    // cleanup (unreachable in current infinite loop)
    close(server_fd);
    return 0;
}
