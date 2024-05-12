#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_CLIENTS 2

int client_count = 0;
int client_sockets[MAX_CLIENTS];
pthread_t threads[MAX_CLIENTS];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int active_clients = 0; // Biến đếm số lượng client còn kết nối

void *handle_client(void *arg) {
    int client_socket = *((int *) arg);
    char buffer[1024];
    int read_size;

    pthread_mutex_lock(&mutex);
    active_clients++; // Tăng số lượng client còn kết nối
    pthread_mutex_unlock(&mutex);

    while ((read_size = recv(client_socket, buffer, sizeof(buffer), 0)) > 0) {
        pthread_mutex_lock(&mutex);

        for (int i = 0; i < client_count; i++) {
            if (client_sockets[i] != client_socket) {
                send(client_sockets[i], buffer, strlen(buffer), 0);
            }
        }

        pthread_mutex_unlock(&mutex);
        memset(buffer, 0, sizeof(buffer));
    }

    pthread_mutex_lock(&mutex);
    active_clients--; // Giảm số lượng client còn kết nối

    if (active_clients == 1) { // Nếu chỉ còn một client kết nối
        for (int i = 0; i < client_count; i++) {
            if (client_sockets[i] != client_socket) {
                close(client_sockets[i]); // Ngắt kết nối cho client còn lại
                break;
            }
        }
    }

    for (int i = 0; i < client_count; i++) {
        if (client_sockets[i] == client_socket) {
            client_sockets[i] = client_sockets[client_count - 1];
            client_count--;
            break;
        }
    }

    pthread_mutex_unlock(&mutex);
    close(client_socket);
    pthread_exit(NULL);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_size = sizeof(client_address);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        perror("Failed to bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Failed to listen");
        exit(EXIT_FAILURE);
    }

    printf("Server started, waiting for connections...\n");

    while ((client_socket = accept(server_socket, (struct sockaddr *) &client_address, &client_address_size))) {
        pthread_mutex_lock(&mutex);

        if (client_count < MAX_CLIENTS) {
            client_sockets[client_count++] = client_socket;
            pthread_create(&threads[client_count - 1], NULL, handle_client, (void *) &client_sockets[client_count - 1]);

            if (client_count == MAX_CLIENTS) {
                printf("Two clients connected\n");
            }
        } else {
            printf("Maximum number of clients reached\n");
            close(client_socket);
        }

        pthread_mutex_unlock(&mutex);
    }

    return 0;
}