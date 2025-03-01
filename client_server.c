#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <fcntl.h>

#define PORT 42000
#define HOST "128.59.64.121"
#define RECV_BUFFER 4096
#define MAX_CLIENTS 10

fd_set master_set, read_fds;
int server_socket, max_fd;
int client_sockets[MAX_CLIENTS];

void broadcast_data(int sender, const char *message) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != 0 && client_sockets[i] != sender) {
            send(client_sockets[i], message, strlen(message), 0);
        }
    }
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[RECV_BUFFER];

    // Initialize client sockets array
    memset(client_sockets, 0, sizeof(client_sockets));

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(HOST);
    server_addr.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    // Initialize the master file descriptor set
    FD_ZERO(&master_set);
    FD_SET(server_socket, &master_set);
    max_fd = server_socket;

    printf("## Listening on %s:%d\n", HOST, PORT);

    while (1) {
        read_fds = master_set;
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("Select failed");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i <= max_fd; i++) {
            if (FD_ISSET(i, &read_fds)) {
                // New connection
                if (i == server_socket) {
                    int new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
                    if (new_socket < 0) {
                        perror("Accept failed");
                        continue;
                    }

                    // Add new socket to client list
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (client_sockets[j] == 0) {
                            client_sockets[j] = new_socket;
                            break;
                        }
                    }

                    FD_SET(new_socket, &master_set);
                    if (new_socket > max_fd) {
                        max_fd = new_socket;
                    }

                    printf("## New Connection %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    char join_msg[256];
                    snprintf(join_msg, sizeof(join_msg), "## %s has joined\n", inet_ntoa(client_addr.sin_addr));
                    broadcast_data(new_socket, join_msg);
                } else {
                    // Data from client
                    int bytes_received = recv(i, buffer, RECV_BUFFER, 0);
                    if (bytes_received > 0) {
                        buffer[bytes_received] = '\0';
                        struct sockaddr_in peer_addr;
                        socklen_t peer_addr_len = sizeof(peer_addr);
                        getpeername(i, (struct sockaddr *)&peer_addr, &peer_addr_len);
                        char message[RECV_BUFFER + 50];
                        snprintf(message, sizeof(message), "<%s> %s", inet_ntoa(peer_addr.sin_addr), buffer);
                        broadcast_data(i, message);
                        printf("%s", message);
                    } else {
                        // Client disconnected
                        getpeername(i, (struct sockaddr *)&client_addr, &addr_len);
                        printf("## Closed connection %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                        char leave_msg[256];
                        snprintf(leave_msg, sizeof(leave_msg), "## %s has left\n", inet_ntoa(client_addr.sin_addr));
                        broadcast_data(i, leave_msg);

                        close(i);
                        FD_CLR(i, &master_set);
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (client_sockets[j] == i) {
                                client_sockets[j] = 0;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    close(server_socket);
    return 0;
}
