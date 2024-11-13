#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT1 2201
#define PORT2 2202
#define BUFFER_SIZE 1024

// Error codes
#define ERROR_INVALID_PACKET_TYPE 100
#define ERROR_INVALID_BEGIN_PARAMS 200

// Packet handling
void handle_begin_packet(int client_fd, char *packet, int *board_width, int *board_height, int player_number);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT1 2201
#define PORT2 2202
#define BUFFER_SIZE 1024

// Error codes
#define ERROR_INVALID_PACKET_TYPE 100
#define ERROR_INVALID_BEGIN_PARAMS 200

int main() {
    int server_fd1, server_fd2, new_socket1, new_socket2;
    struct sockaddr_in address1, address2;
    int addrlen = sizeof(address1);
    char buffer[BUFFER_SIZE] = {0};
    int board_width = 0, board_height = 0;  // Board dimensions initialized by player 1
    
    // Set up server sockets for both players
    if ((server_fd1 = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed for player 1");
        exit(EXIT_FAILURE);
    }
    if ((server_fd2 = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed for player 2");
        close(server_fd1);
        exit(EXIT_FAILURE);
    }

    // Allow immediate reuse of ports if needed
    int opt = 1;
    setsockopt(server_fd1, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(server_fd2, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configure player 1 address
    address1.sin_family = AF_INET;
    address1.sin_addr.s_addr = INADDR_ANY;
    address1.sin_port = htons(PORT1);
    // Bind to PORT1
    if (bind(server_fd1, (struct sockaddr *)&address1, sizeof(address1)) < 0) {
        perror("Bind failed for player 1");
        close(server_fd1);
        close(server_fd2);
        exit(EXIT_FAILURE);
    }

    // Configure player 2 address
    address2.sin_family = AF_INET;
    address2.sin_addr.s_addr = INADDR_ANY;
    address2.sin_port = htons(PORT2);
    // Bind to PORT2
    if (bind(server_fd2, (struct sockaddr *)&address2, sizeof(address2)) < 0) {
        perror("Bind failed for player 2");
        close(server_fd1);
        close(server_fd2);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd1, 1) < 0 || listen(server_fd2, 1) < 0) {
        perror("Listen failed");
        close(server_fd1);
        close(server_fd2);
        exit(EXIT_FAILURE);
    }

    printf("[Server] Waiting for Player 1 to connect...\n");
    new_socket1 = accept(server_fd1, (struct sockaddr *)&address1, (socklen_t *)&addrlen);
    if (new_socket1 < 0) {
        perror("Accept failed for player 1");
        close(server_fd1);
        close(server_fd2);
        exit(EXIT_FAILURE);
    }
    printf("[Server] Player 1 connected.\n");

    printf("[Server] Waiting for Player 2 to connect...\n");
    new_socket2 = accept(server_fd2, (struct sockaddr *)&address2, (socklen_t *)&addrlen);
    if (new_socket2 < 0) {
        perror("Accept failed for player 2");
        close(new_socket1);
        close(server_fd1);
        close(server_fd2);
        exit(EXIT_FAILURE);
    }
    printf("[Server] Player 2 connected.\n");

    // Receive Begin packet from Player 1 to set board dimensions
    int player1_init = 0, player2_init = 0;
    int game_active = 1; // Game state flag

    // Main loop for handling packets from both players
    while (game_active) {
        // Check for Player 1 Begin packet
        if (player1_init == 0) {
            int nbytes = read(new_socket1, buffer, BUFFER_SIZE);
            if (nbytes <= 0) {
                perror("[Server] Read error from Player 1");
                break;
            }
            buffer[nbytes] = '\0';
            if (buffer[0] == 'B') {
                int width, height;
                if (sscanf(buffer + 2, "%d %d", &width, &height) == 2 && width >= 10 && height >= 10) {
                    board_width = width;
                    board_height = height;
                    snprintf(buffer, BUFFER_SIZE, "A");  // Acknowledge packet
                    send(new_socket1, buffer, strlen(buffer), 0);
                    printf("[Server] Received valid Begin packet from Player 1: Board %dx%d\n", width, height);
                    player1_init = 1;
                } else {
                    snprintf(buffer, BUFFER_SIZE, "E %d", ERROR_INVALID_BEGIN_PARAMS);
                    send(new_socket1, buffer, strlen(buffer), 0);
                    printf("[Server] Invalid Begin packet parameters from Player 1.\n");
                }
            } else {
                snprintf(buffer, BUFFER_SIZE, "E %d", ERROR_INVALID_PACKET_TYPE);
                send(new_socket1, buffer, strlen(buffer), 0);
                printf("[Server] Invalid packet type from Player 1.\n");
            }
        }
        
        // Check for Player 2 Begin packet
        if (player2_init == 0 && player1_init == 1) {  // Ensure player 1 has initialized board dimensions
            int nbytes = read(new_socket2, buffer, BUFFER_SIZE);
            if (nbytes <= 0) {
                perror("[Server] Read error from Player 2");
                break;
            }
            buffer[nbytes] = '\0';
            if (strcmp(buffer, "B") == 0) {
                snprintf(buffer, BUFFER_SIZE, "A");  // Acknowledge packet
                send(new_socket2, buffer, strlen(buffer), 0);
                printf("[Server] Received valid Begin packet from Player 2\n");
                player2_init = 1;
            } else {
                snprintf(buffer, BUFFER_SIZE, "E %d", ERROR_INVALID_PACKET_TYPE);
                send(new_socket2, buffer, strlen(buffer), 0);
                printf("[Server] Invalid Begin packet from Player 2.\n");
            }
        }

        // Handle other packets (e.g., Forfeit) from both players once both are initialized
        if (game_active && player1_init == 1 && player2_init == 1) {
            int nbytes1 = read(new_socket1, buffer, BUFFER_SIZE);
            if (nbytes1 > 0) {
                buffer[nbytes1] = '\0';
                if (buffer[0] == 'F') {
                    snprintf(buffer, BUFFER_SIZE, "H 0");  // Forfeiting player receives H 0
                    send(new_socket1, buffer, strlen(buffer), 0);
                    snprintf(buffer, BUFFER_SIZE, "H 1");  // Other player receives H 1
                    send(new_socket2, buffer, strlen(buffer), 0);
                    game_active = 0;  // End the game
                    printf("[Server] Player 1 forfeited. Game over.\n");
                }
            }
            
            if (game_active) { // Ensure the game hasn't ended before checking Player 2
                int nbytes2 = read(new_socket2, buffer, BUFFER_SIZE);
                if (nbytes2 > 0) {
                    buffer[nbytes2] = '\0';
                    if (buffer[0] == 'F') {
                        snprintf(buffer, BUFFER_SIZE, "H 0");  // Forfeiting player receives H 0
                        send(new_socket2, buffer, strlen(buffer), 0);
                        snprintf(buffer, BUFFER_SIZE, "H 1");  // Other player receives H 1
                        send(new_socket1, buffer, strlen(buffer), 0);
                        game_active = 0;  // End the game
                        printf("[Server] Player 2 forfeited. Game over.\n");
                    }
                }
            }
        }
    }

    // Close all sockets and exit
    close(new_socket1);
    close(new_socket2);
    close(server_fd1);
    close(server_fd2);
    printf("[Server] Game shutdown completed.\n");
    return 0;
}

// Function to handle different types of packets
void handle_packet(int client_fd, char *packet, int *board_width, int *board_height, int player_number, int *game_active, int other_client_fd) {
    char response[BUFFER_SIZE];

    if (packet[0] == 'B') {
        // Handle Begin packet
        if (player_number == 1) {
            int width, height;
            if (sscanf(packet + 2, "%d %d", &width, &height) == 2 && width >= 10 && height >= 10) {
                *board_width = width;
                *board_height = height;
                snprintf(response, BUFFER_SIZE, "A");  // Acknowledge packet
                send(client_fd, response, strlen(response), 0);
                printf("[Server] Received valid Begin packet from Player 1: Board %dx%d\n", width, height);
            } else {
                snprintf(response, BUFFER_SIZE, "E %d", ERROR_INVALID_BEGIN_PARAMS);
                send(client_fd, response, strlen(response), 0);
                printf("[Server] Invalid Begin packet parameters from Player 1.\n");
            }
        } else if (player_number == 2) {
            if (strcmp(packet, "B") == 0) {
                snprintf(response, BUFFER_SIZE, "A");  // Acknowledge packet
                send(client_fd, response, strlen(response), 0);
                printf("[Server] Received valid Begin packet from Player 2\n");
            } else {
                snprintf(response, BUFFER_SIZE, "E %d", ERROR_INVALID_PACKET_TYPE);
                send(client_fd, response, strlen(response), 0);
                printf("[Server] Invalid Begin packet from Player 2.\n");
            }
        }
    } else if (packet[0] == 'F') {
        // Forfeit packet handling
        snprintf(response, BUFFER_SIZE, "H 0");  // Loss for the forfeiting player
        send(client_fd, response, strlen(response), 0);

        snprintf(response, BUFFER_SIZE, "H 1");  // Win for the other player
        send(other_client_fd, response, strlen(response), 0);

        *game_active = 0;  // End the game
        printf("[Server] Player %d forfeited. Game over.\n", player_number);
    } else {
        // Handle invalid packet type
        snprintf(response, BUFFER_SIZE, "E %d", ERROR_INVALID_PACKET_TYPE);
        send(client_fd, response, strlen(response), 0);
        printf("[Server] Invalid packet type from Player %d.\n", player_number);
    }
}