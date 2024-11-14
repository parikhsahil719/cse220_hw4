#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT_PLAYER1 2201
#define PORT_PLAYER2 2202
#define BUFFER_SIZE 1024

int **initialize_board(int width, int height) {
    int **board = (int **)malloc(height * sizeof(int *));
    for (int i = 0; i < height; i++) {
        board[i] = (int *)calloc(width, sizeof(int));  // Initialize all cells to 0
    }
    return board;
}

void free_board(int **board, int height) {
    for (int i = 0; i < height; i++) {
        free(board[i]);
    }
    free(board);
}

int main() {
    int listen_fd1, listen_fd2, conn_fd1, conn_fd2;
    struct sockaddr_in address1, address2;
    int opt = 1;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    char buffer[BUFFER_SIZE] = {0};
    int board_width, board_height;
    int **game_board = NULL;

    // Setting up socket for Player 1
    if ((listen_fd1 = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed for Player 1");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(listen_fd1, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt for Player 1");
        exit(EXIT_FAILURE);
    }

    address1.sin_family = AF_INET;
    address1.sin_addr.s_addr = INADDR_ANY;
    address1.sin_port = htons(PORT_PLAYER1);

    if (bind(listen_fd1, (struct sockaddr *)&address1, sizeof(address1)) < 0) {
        perror("bind failed for Player 1");
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd1, 1) < 0) {
        perror("listen failed for Player 1");
        exit(EXIT_FAILURE);
    }

    // Setting up socket for Player 2
    if ((listen_fd2 = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed for Player 2");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(listen_fd2, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt for Player 2");
        exit(EXIT_FAILURE);
    }

    address2.sin_family = AF_INET;
    address2.sin_addr.s_addr = INADDR_ANY;
    address2.sin_port = htons(PORT_PLAYER2);

    if (bind(listen_fd2, (struct sockaddr *)&address2, sizeof(address2)) < 0) {
        perror("bind failed for Player 2");
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd2, 1) < 0) {
        perror("listen failed for Player 2");
        exit(EXIT_FAILURE);
    }

    printf("[Server] Waiting for Player 1 on port %d...\n", PORT_PLAYER1);
    if ((conn_fd1 = accept(listen_fd1, (struct sockaddr *)&address1, &addrlen)) < 0) {
        perror("accept failed for Player 1");
        exit(EXIT_FAILURE);
    }
    printf("[Server] Player 1 connected!\n");

    printf("[Server] Waiting for Player 2 on port %d...\n", PORT_PLAYER2);
    if ((conn_fd2 = accept(listen_fd2, (struct sockaddr *)&address2, &addrlen)) < 0) {
        perror("accept failed for Player 2");
        exit(EXIT_FAILURE);
    }
    printf("[Server] Player 2 connected!\n");

    // Add game logic here, as needed

    // Wait for a valid "Begin" packet from Player 1
    // Wait for "Begin" packet from Player 1 to set up the board
    printf("[Server] Waiting for valid Begin packet from Player 1...\n");
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(conn_fd1, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive Begin packet from Player 1");
            exit(EXIT_FAILURE);
        }

        // Parse "Begin" packet from Player 1
        if (sscanf(buffer, "B %d %d", &board_width, &board_height) == 2) {
            if (board_width >= 10 && board_height >= 10) {
                // Valid packet, initialize board and send acknowledgment
                game_board = initialize_board(board_width, board_height);
                if (game_board == NULL) {
                    perror("Failed to allocate game board");
                    exit(EXIT_FAILURE);
                }
                send(conn_fd1, "A\n", strlen("A\n"), 0);
                printf("[Server] Board initialized with size %dx%d\n", board_width, board_height);
                break;
            } else {
                // Invalid dimensions, send error and continue waiting
                send(conn_fd1, "E 200\n", strlen("E 200\n"), 0);
                fprintf(stderr, "[Server] Invalid board dimensions received from Player 1\n");
            }
        } else {
            // Invalid packet format, send error and continue waiting
            send(conn_fd1, "E 100\n", strlen("E 100\n"), 0);
            fprintf(stderr, "[Server] Invalid Begin packet format from Player 1\n");
        }
    }

    // Wait for a valid "Begin" packet from Player 2
    printf("[Server] Waiting for valid Begin packet from Player 2...\n");
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(conn_fd2, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
        perror("Failed to receive Begin packet from Player 2");
        exit(EXIT_FAILURE);
        }

        // Process "Begin" packet from Player 2
        if (strncmp(buffer, "B", 1) == 0 && (buffer[1] == '\n' || buffer[1] == '\0')) {
            // Valid packet, send acknowledgment and break the loop
            send(conn_fd2, "A\n", strlen("A\n"), 0);
            printf("[Server] Valid Begin packet received from Player 2\n");
            break;
        }
        else {
            // Invalid packet format, send error and continue waiting
            send(conn_fd2, "E 100\n", strlen("E 100\n"), 0);
            fprintf(stderr, "[Server] Invalid Begin packet format from Player 2\n");
        }
    }

    if (game_board) {
        free_board(game_board, board_height);
    }

    // Close connections after game
    close(conn_fd1);
    close(conn_fd2);
    close(listen_fd1);
    close(listen_fd2);

    return 0;
}

