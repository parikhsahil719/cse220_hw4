#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT_PLAYER1 2201
#define PORT_PLAYER2 2202
#define BUFFER_SIZE 1024

// Function prototypes
void *handle_client(void *socket_desc);
void process_begin_packet(int socket, char *packet);
void process_initialize_packet(int socket, char *packet);
void process_game_packet(int socket, char *packet);
void send_acknowledgment(int socket);
void send_error(int socket, int error_code);

// Main function
int main() {
    int server_socket1, server_socket2, client_socket1, client_socket2;
    struct sockaddr_in server_addr1, server_addr2, client_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    // Initialize server sockets for both players
    server_socket1 = socket(AF_INET, SOCK_STREAM, 0);
    server_socket2 = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket1 == -1 || server_socket2 == -1) {
        perror("Socket creation failed");
        return 1;
    }

    // Server 1 setup for Player 1
    server_addr1.sin_family = AF_INET;
    server_addr1.sin_addr.s_addr = INADDR_ANY;
    server_addr1.sin_port = htons(PORT_PLAYER1);
    
    if (bind(server_socket1, (struct sockaddr *)&server_addr1, sizeof(server_addr1)) < 0) {
        perror("Bind failed on port 2201");
        return 1;
    }
    listen(server_socket1, 3);

    // Server 2 setup for Player 2
    server_addr2.sin_family = AF_INET;
    server_addr2.sin_addr.s_addr = INADDR_ANY;
    server_addr2.sin_port = htons(PORT_PLAYER2);
    
    if (bind(server_socket2, (struct sockaddr *)&server_addr2, sizeof(server_addr2)) < 0) {
        perror("Bind failed on port 2202");
        return 1;
    }
    listen(server_socket2, 3);

    // Accept Player 1 connection
    printf("Waiting for Player 1 to connect...\n");
    client_socket1 = accept(server_socket1, (struct sockaddr *)&client_addr, &addr_len);
    if (client_socket1 < 0) {
        perror("Player 1 connection failed");
        return 1;
    }
    printf("Player 1 connected.\n");

    // Accept Player 2 connection
    printf("Waiting for Player 2 to connect...\n");
    client_socket2 = accept(server_socket2, (struct sockaddr *)&client_addr, &addr_len);
    if (client_socket2 < 0) {
        perror("Player 2 connection failed");
        return 1;
    }
    printf("Player 2 connected.\n");

    // Start game handling in threads or sequentially (simple example with sequential handling here)
    handle_client((void *)&client_socket1);
    handle_client((void *)&client_socket2);

    // Close sockets
    close(client_socket1);
    close(client_socket2);
    close(server_socket1);
    close(server_socket2);

    return 0;
}

// Handle client packets
void *handle_client(void *socket_desc) {
    int socket = *(int *)socket_desc;
    char buffer[BUFFER_SIZE];
    int read_size;

    // Game loop for receiving packets
    while ((read_size = recv(socket, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[read_size] = '\0';
        
        // Determine packet type and process accordingly
        if (buffer[0] == 'B') {
            process_begin_packet(socket, buffer);
        } else if (buffer[0] == 'I') {
            process_initialize_packet(socket, buffer);
        } else {
            process_game_packet(socket, buffer);
        }
    }
    return 0;
}

// Process Begin packet (initial connection setup)
void process_begin_packet(int socket, char *packet) {
    // Example parsing and validation logic
    printf("Received Begin packet: %s\n", packet);
    // ... Process Begin packet and send acknowledgment or error
    send_acknowledgment(socket);
}

// Process Initialize packet (ship placement)
void process_initialize_packet(int socket, char *packet) {
    printf("Received Initialize packet: %s\n", packet);
    // ... Process Initialize packet, check validity
    send_acknowledgment(socket);
}

// Process game packets (Shoot, Query, Forfeit)
void process_game_packet(int socket, char *packet) {
    printf("Received game packet: %s\n", packet);
    if (packet[0] == 'S') {
        // Handle Shoot
    } else if (packet[0] == 'Q') {
        // Handle Query
    } else if (packet[0] == 'F') {
        // Handle Forfeit
    } else {
        send_error(socket, 100); // Invalid packet type
    }
}

// Send acknowledgment packet
void send_acknowledgment(int socket) {
    char *ack = "A";
    send(socket, ack, strlen(ack), 0);
}

// Send error packet with code
void send_error(int socket, int error_code) {
    char error_msg[BUFFER_SIZE];
    snprintf(error_msg, sizeof(error_msg), "E %d", error_code);
    send(socket, error_msg, strlen(error_msg), 0);
}