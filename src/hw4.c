#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT_PLAYER1 2201
#define PORT_PLAYER2 2202
#define BUFFER_SIZE 1024

int **initialize_board(int width, int height) {
    int **board = malloc(height * sizeof(int *));
    if (!board) return NULL;

    for (int i = 0; i < height; i++) {
        board[i] = calloc(width, sizeof(int));  // Initialize all cells to 0
        if (!board[i]) {
            // Free any allocated rows in case of failure
            for (int j = 0; j < i; j++) {
                free(board[j]);
            }
            free(board);
            return NULL;
        }
    }
    return board;
}

void free_board(int **board, int height) {
    for (int i = 0; i < height; i++) {
        free(board[i]);
    }
    free(board);
}

// Define only the base (initial) rotation for each shape, with the circled reference cell at (0, 0)
int base_shapes[7][4][2] = {
    {{0, 0}, {0, 1}, {1, 0}, {1, 1}},  // O-piece
    {{0, 0}, {1, 0}, {2, 0}, {3, 0}},  // I-piece
    {{0, 0}, {0, 1}, {1, 1}, {1, 2}},  // S-piece
    {{0, 0}, {1, 0}, {2, 0}, {2, 1}},  // L-piece
    {{0, 1}, {0, 0}, {1, 1}, {1, 2}},  // Z-piece
    {{0, 0}, {1, 0}, {2, 0}, {2, -1}}, // J-piece
    {{0, 0}, {1, -1}, {1, 0}, {1, 1}}  // T-piece
};

// Rotate a point 90 degrees clockwise around the origin (reference cell at (0,0))
void rotate_90(int *x, int *y) {
    int temp = *x;
    *x = *y;
    *y = -temp;
}

// Get coordinates for the rotated piece around the reference (circled) cell
void get_piece_coordinates(int piece_type, int rotation, int ref_row, int ref_col, int coords[4][2]) {
    for (int i = 0; i < 4; i++) {
        int x = base_shapes[piece_type][i][0];
        int y = base_shapes[piece_type][i][1];

        if (rotation > 1) {// Adjust the rotation count to apply the correct number of 90-degree rotations
            for (int r = 0; r < rotation - 1; r++) {
                rotate_90(&x, &y);
            }
        }
        // Translate to board position using the circled cell as the reference
        coords[i][0] = ref_row + x;
        coords[i][1] = ref_col + y;
    }
}

// Place a piece on the board, using the reference cell and rotation
int place_piece(int **board, int board_width, int board_height, int piece_type, int rotation, int ref_row, int ref_col) {
    int coords[4][2];
    get_piece_coordinates(piece_type, rotation, ref_row, ref_col, coords);

    // Validate each cell in the piece
    for (int i = 0; i < 4; i++) {
        int row = coords[i][0];
        int col = coords[i][1];

        // Check if within bounds
        if (row < 0 || row >= board_height || col < 0 || col >= board_width) {
            return 302;  // Ship doesn’t fit in game board
        }

        // Check for overlap
        if (board[row][col] != 0) {
            return 303;  // Ships overlap
        }
    }

    // Place piece on the board
    for (int i = 0; i < 4; i++) {
        int row = coords[i][0];
        int col = coords[i][1];
        board[row][col] = 1;  // Mark cell as occupied
    }

    return 0;  // Success
}

int validate_piece_placement(int **board, int board_width, int board_height, int piece_type, int rotation, int ref_row, int ref_col) {
    int coords[4][2];
    get_piece_coordinates(piece_type, rotation, ref_row, ref_col, coords);

    for (int i = 0; i < 4; i++) {
        int row = coords[i][0];
        int col = coords[i][1];

        // Check if within bounds
        if (row < 0 || row >= board_height || col < 0 || col >= board_width) {
            return 302;  // Ship doesn’t fit in game board
        }

        // Check for overlap
        if (board[row][col] != 0) {
            return 303;  // Ships overlap
        }
    }

    return 0;  // Valid placement
}

// Handle the Initialize packet for setting up pieces
int handle_initialize_packet(int conn_fd, int **board, int board_width, int board_height, char *packet) {
    int piece_type, rotation, ref_row, ref_col;
    int num_pieces = 5;  // Expecting 5 pieces in the initialize packet, i.e., 20 integers
    int offset = 2;      // Start after "I " to skip the initial character and space
    int lowest_error = 0;  // Track the lowest error code; 0 indicates no errors

    // First pass: Validation only
    for (int i = 0; i < num_pieces; i++) {
        // Parse the next set of four integers directly from the packet using sscanf
        int parsed = sscanf(packet + offset, "%d %d %d %d", &piece_type, &rotation, &ref_row, &ref_col);
        
        // Check if sscanf successfully parsed 4 values
        if (parsed != 4) {
            lowest_error = lowest_error == 0 ? 201 : (lowest_error > 201 ? 201 : lowest_error);  // Invalid number of parameters
            continue;
        }

        // Validate piece type and rotation before attempting placement
        if (piece_type < 1 || piece_type > 7) {
            lowest_error = lowest_error == 0 ? 300 : (lowest_error > 300 ? 300 : lowest_error);  // Shape out of range
        }
        if (rotation < 1 || rotation > 4) {
            lowest_error = lowest_error == 0 ? 301 : (lowest_error > 301 ? 301 : lowest_error);  // Rotation out of range
        }

        // Adjust for 0-based indexing for rotation in later placement
        piece_type -= 1;
        rotation -= 1;

        // Check if placement would be out of bounds or overlap, but don’t place yet
        int error_code = validate_piece_placement(board, board_width, board_height, piece_type, rotation, ref_row, ref_col);
        if (error_code) {
            lowest_error = lowest_error == 0 ? error_code : (lowest_error > error_code ? error_code : lowest_error);
        }

        // Update the offset to move to the next set of four integers
        offset += snprintf(NULL, 0, "%d %d %d %d ", piece_type + 1, rotation + 1, ref_row, ref_col);
    }

    // If there's any error, return the lowest error code
    if (lowest_error != 0) {
        char error_msg[BUFFER_SIZE];
        snprintf(error_msg, sizeof(error_msg), "E %d\n", lowest_error);
        send(conn_fd, error_msg, strlen(error_msg), 0);
        return -1;
    }

    // Second pass: Placement (only if validation was successful)
    offset = 2;  // Reset offset to start after "I "
    for (int i = 0; i < num_pieces; i++) {
        sscanf(packet + offset, "%d %d %d %d", &piece_type, &rotation, &ref_row, &ref_col);
        piece_type -= 1;
        rotation -= 1;
        
        // Place the piece on the board
        place_piece(board, board_width, board_height, piece_type, rotation, ref_row, ref_col);

        // Update the offset to move to the next set of four integers
        offset += snprintf(NULL, 0, "%d %d %d %d ", piece_type + 1, rotation + 1, ref_row, ref_col);
    }

    // Send acknowledgment after all pieces are successfully placed
    send(conn_fd, "A\n", strlen("A\n"), 0);
    return 0;
}

int main() {
    int listen_fd1, listen_fd2, conn_fd1, conn_fd2;
    struct sockaddr_in address1, address2;
    int opt = 1;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    char buffer[BUFFER_SIZE] = {0};
    int board_width, board_height;

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

    int **player1_board = initialize_board(board_width, board_height);
    int **player2_board = initialize_board(board_width, board_height);

    if (player1_board == NULL || player2_board == NULL) {
        perror("Failed to allocate memory for player boards");
        exit(EXIT_FAILURE);
    }

    // Wait for a valid "Initialize" packet from Player 1
    printf("[Server] Waiting for valid Initialize packet from Player 1...\n");
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(conn_fd1, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive Initialize packet from Player 1");
            exit(EXIT_FAILURE);
        }

        // Use player1_board specifically for Player 1
        if (handle_initialize_packet(conn_fd1, player1_board, board_width, board_height, buffer) == 0) {
            printf("[Server] Player 1's board initialized successfully.\n");
            break;
        }
        // If invalid, handle_initialize_packet will send the error; continue loop to ask again
    }

    // Process "Initialize" packet for Player 2
    printf("[Server] Waiting for valid Initialize packet from Player 2...\n");
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(conn_fd2, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive Initialize packet from Player 2");
            exit(EXIT_FAILURE);
        }

        // Use player2_board specifically for Player 2
        if (handle_initialize_packet(conn_fd2, player2_board, board_width, board_height, buffer) == 0) {
            printf("[Server] Player 2's board initialized successfully.\n");
            break;
        }
        // If invalid, handle_initialize_packet will send the error; continue loop to ask again
    }

    printf("[Server] Both players have successfully initialized their boards.\n");

    // Proceed with the game loop or further logic as needed

    // Free the allocated boards after the game
    free_board(player1_board, board_height);
    free_board(player2_board, board_height);


    // Close connections after game
    close(conn_fd1);
    close(conn_fd2);
    close(listen_fd1);
    close(listen_fd2);

    return 0;
}

