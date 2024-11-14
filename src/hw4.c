#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT_PLAYER1 2201
#define PORT_PLAYER2 2202
#define BUFFER_SIZE 1024
#define HIT 'H'
#define MISS 'M'
#define EMPTY 0

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
void print_board(int **board, int width, int height) {
    printf("Current Board State:\n");
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            printf("%d ", board[i][j]);
        }
        printf("\n");
    }
    printf("\n");
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
int place_piece(int **board, int board_width, int board_height, int piece_type, int rotation, int ref_row, int ref_col, int piece_id) {
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

    // Place piece on the board using piece_id as the identifier
    for (int i = 0; i < 4; i++) {
        int row = coords[i][0];
        int col = coords[i][1];
        board[row][col] = piece_id;  // Mark cell with unique identifier
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
        int parsed = sscanf(packet + offset, "%d %d %d %d", &piece_type, &rotation, &ref_row, &ref_col);
        if (parsed != 4) {
            lowest_error = lowest_error == 0 ? 201 : (lowest_error > 201 ? 201 : lowest_error);
            continue;
        }

        // Validate piece type and rotation
        if (piece_type < 1 || piece_type > 7) {
            lowest_error = lowest_error == 0 ? 300 : (lowest_error > 300 ? 300 : lowest_error);
        }
        if (rotation < 1 || rotation > 4) {
            lowest_error = lowest_error == 0 ? 301 : (lowest_error > 301 ? 301 : lowest_error);
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
        
        // Use (i + 1) as a unique identifier for each piece
        place_piece(board, board_width, board_height, piece_type, rotation, ref_row, ref_col, i + 1);

        // Update the offset to move to the next set of four integers
        offset += snprintf(NULL, 0, "%d %d %d %d ", piece_type + 1, rotation + 1, ref_row, ref_col);
    }

    // Send acknowledgment after all pieces are successfully placed
    send(conn_fd, "A\n", strlen("A\n"), 0);
    return 0;
}

int is_ship_sunk(int **board, int width, int height, int piece_id) {
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            if (board[i][j] == piece_id) {
                return 0;  // Ship is not yet sunk, as there is still a cell with the piece_id
            }
        }
    }
    return 1;  // Ship is fully sunk
}

int count_remaining_ships(int **board, int width, int height) {
    bool ship_found[6] = {false};  // Tracks if ship IDs 1 through 5 have been counted
    int remaining_ships = 0;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int piece_id = board[i][j];
            // Only count IDs between 1 and 5, and only if we haven't counted this ship ID yet
            if (piece_id >= 1 && piece_id <= 5 && !ship_found[piece_id]) {
                ship_found[piece_id] = true;  // Mark this piece ID as counted
                remaining_ships++;  // Increment remaining ships count
            }
        }
    }
    return remaining_ships;
}

char **initialize_shot_history(int width, int height) {
    char **history = malloc(height * sizeof(char *));
    if (!history) return NULL;

    for (int i = 0; i < height; i++) {
        history[i] = malloc(width * sizeof(char));
        if (!history[i]) {
            for (int j = 0; j < i; j++) {
                free(history[j]);
            }
            free(history);
            return NULL;
        }
        memset(history[i], 0, width * sizeof(char));  // Initialize all cells to 0 (no shot taken)
    }
    return history;
}

void free_shot_history(char **history, int height) {
    for (int i = 0; i < height; i++) {
        free(history[i]);
    }
    free(history);
}

int handle_shoot_packet(int conn_fd, int **opponent_board, char **shot_history, int board_width, int board_height, int *remaining_ships, int conn_fd_opponent, char *packet) {
    int row, col;

    // Parse and validate the shoot packet
    if (sscanf(packet, "S %d %d", &row, &col) != 2) {
        send(conn_fd, "E 202\n", strlen("E 202\n"), 0);  // Invalid number of parameters
        return -1;
    }

    // Check if the shot is out of bounds
    if (row < 0 || row >= board_height || col < 0 || col >= board_width) {
        send(conn_fd, "E 400\n", strlen("E 400\n"), 0);
        return -1;
    }

    // Check if the shot was already taken
    if (shot_history[row][col] != EMPTY) {
        send(conn_fd, "E 401\n", strlen("E 401\n"), 0);
        return -1;
    }

    // Determine if it's a hit or miss
    char shot_result;
    int piece_id = opponent_board[row][col];
    if (piece_id != 0) {
        // It's a hit
        shot_result = 'H';
        shot_history[row][col] = HIT;
        opponent_board[row][col] = 'H';  // Mark the hit location with 'H'

        // Check if the hit has sunk the ship
        if (is_ship_sunk(opponent_board, board_width, board_height, piece_id)) {
            (*remaining_ships)--;  // Decrement remaining ships count if the ship is sunk
        }
    } else {
        // It's a miss
        shot_result = 'M';
        shot_history[row][col] = MISS;
    }

    // Send the shot response to the shooter
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "R %d %c\n", *remaining_ships, shot_result);
    send(conn_fd, response, strlen(response), 0);

    // If all ships are sunk, send halt packets
    if (*remaining_ships == 0) {
        // Send Halt to the opponent (loss)
        send(conn_fd_opponent, "H 0\n", strlen("H 0\n"), 0);
        // Wait for opponent to read before sending halt to the shooter
        recv(conn_fd_opponent, response, BUFFER_SIZE, 0);

        // Send Halt to the shooter (win)
        send(conn_fd, "H 1\n", strlen("H 1\n"), 0);
        return 1;  // Game over
    }

    return 0;
}

void handle_query_packet(int conn_fd, char **shot_history, int **opponent_board, int board_width, int board_height) {
    char response[BUFFER_SIZE];
    int response_offset = 0;

    // Add remaining ships count
    int remaining_ships = count_remaining_ships(opponent_board, board_width, board_height);
    response_offset += snprintf(response + response_offset, sizeof(response) - response_offset, "G %d ", remaining_ships);

    // Add each shot to the response in the format {M|H} column row
    for (int i = 0; i < board_height; i++) {
        for (int j = 0; j < board_width; j++) {
            if (shot_history[i][j] == HIT) {
                response_offset += snprintf(response + response_offset, sizeof(response) - response_offset, "H %d %d ", j, i);
            } else if (shot_history[i][j] == MISS) {
                response_offset += snprintf(response + response_offset, sizeof(response) - response_offset, "M %d %d ", j, i);
            }
        }
    }

    // Send the response to the client
    send(conn_fd, response, strlen(response), 0);
}

void handle_forfeit_packet(int conn_fd, int conn_fd_opponent) {
    // Send Halt to the forfeiting player (loss)
    send(conn_fd, "H 0\n", strlen("H 0\n"), 0);
    // Wait for the forfeiting player to read
    char buffer[BUFFER_SIZE];
    recv(conn_fd, buffer, BUFFER_SIZE, 0);

    // Send Halt to the opponent (win)
    send(conn_fd_opponent, "H 1\n", strlen("H 1\n"), 0);
}

void game_loop(int conn_fd1, int conn_fd2) {
    char buffer[BUFFER_SIZE];
    int board_width, board_height;
    int **player1_board = NULL;
    int **player2_board = NULL;
    char **player1_shot_history = NULL;
    char **player2_shot_history = NULL;

    // Phase 1: Waiting for "Begin" or "Forfeit" packets from both players
    // Player 1 - Set up board dimensions or forfeit
    printf("[Server] Waiting for valid Begin or Forfeit packet from Player 1...\n");
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(conn_fd1, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive Begin or Forfeit packet from Player 1");
            exit(EXIT_FAILURE);
        }

        // Check for "Begin" packet from Player 1 with dimensions
        if (sscanf(buffer, "B %d %d", &board_width, &board_height) == 2) {
            if (board_width >= 10 && board_height >= 10) {
                // Valid packet, initialize board and send acknowledgment
                send(conn_fd1, "A\n", strlen("A\n"), 0);
                printf("[Server] Board initialized with size %dx%d\n", board_width, board_height);
                break;
            } else {
                send(conn_fd1, "E 200\n", strlen("E 200\n"), 0);
                fprintf(stderr, "[Server] Invalid board dimensions received from Player 1\n");
            }
        } 
        // Check for "Forfeit" packet from Player 1
        else if (strcmp(buffer, "F\n") == 0 || strcmp(buffer, "F") == 0) {
            // Player 1 forfeits, send Halt packets to both players
            send(conn_fd1, "H 0\n", strlen("H 0\n"), 0);  // Halt with loss to Player 1
            send(conn_fd2, "H 1\n", strlen("H 1\n"), 0);  // Halt with win to Player 2
            printf("[Server] Player 1 forfeited during Begin phase. Game halted.\n");
            exit(EXIT_SUCCESS);  // End the game
        } else {
            send(conn_fd1, "E 100\n", strlen("E 100\n"), 0);
            fprintf(stderr, "[Server] Invalid Begin or Forfeit packet format from Player 1\n");
        }
    }

    // Player 2 - Acknowledge Begin packet with no parameters or forfeit
    printf("[Server] Waiting for valid Begin or Forfeit packet from Player 2...\n");
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(conn_fd2, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive Begin or Forfeit packet from Player 2");
            exit(EXIT_FAILURE);
        }

        // Check if the packet is exactly "B" or "B\n" for Player 2
        if (strcmp(buffer, "B\n") == 0 || strcmp(buffer, "B") == 0) {
            // Valid packet, send acknowledgment and break the loop
            send(conn_fd2, "A\n", strlen("A\n"), 0);
            printf("[Server] Valid Begin packet received from Player 2\n");
            break;
        }
        // Check if the packet starts with "B" but has extra parameters (e.g., "B 1 1")
        else if (strncmp(buffer, "B", 1) == 0) {
            // Player 2 sent additional parameters with "B", return E 200 for invalid format
            send(conn_fd2, "E 200\n", strlen("E 200\n"), 0);
            fprintf(stderr, "[Server] Invalid Begin packet format for Player 2: extra parameters\n");
        }
        // Check for "Forfeit" packet from Player 2
        else if (strcmp(buffer, "F\n") == 0 || strcmp(buffer, "F") == 0) {
            // Player 2 forfeits, send Halt packets to both players
            send(conn_fd2, "H 0\n", strlen("H 0\n"), 0);  // Halt with loss to Player 2
            send(conn_fd1, "H 1\n", strlen("H 1\n"), 0);  // Halt with win to Player 1
            printf("[Server] Player 2 forfeited during Begin phase. Game halted.\n");
            exit(EXIT_SUCCESS);  // End the game
        } 
        else {
            // Invalid packet type, send E 100
            send(conn_fd2, "E 100\n", strlen("E 100\n"), 0);
            fprintf(stderr, "[Server] Invalid packet type received from Player 2 during Begin phase\n");
        }
    }

    // Initialize the boards after both players send valid Begin packets
    player1_board = initialize_board(board_width, board_height);
    player2_board = initialize_board(board_width, board_height);
    if (player1_board == NULL || player2_board == NULL) {
        perror("Failed to allocate memory for player boards");
        exit(EXIT_FAILURE);
    }

    // Phase 2: Waiting for "Initialize" packets from both players
   printf("[Server] Waiting for valid Initialize or Forfeit packet from Player 1...\n");
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(conn_fd1, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive Initialize or Forfeit packet from Player 1");
            exit(EXIT_FAILURE);
        }

        // Check for "Forfeit" packet from Player 1 first
        if (strcmp(buffer, "F\n") == 0 || strcmp(buffer, "F") == 0) {
            // Player 1 forfeits, send Halt packets to both players
            send(conn_fd1, "H 0\n", strlen("H 0\n"), 0);  // Halt with loss to Player 1
            send(conn_fd2, "H 1\n", strlen("H 1\n"), 0);  // Halt with win to Player 2
            printf("[Server] Player 1 forfeited during Initialize phase. Game halted.\n");
            exit(EXIT_SUCCESS);  // End the game
        }
        // Check for "Initialize" packet from Player 1 if no Forfeit packet is found
        else if (handle_initialize_packet(conn_fd1, player1_board, board_width, board_height, buffer) == 0) {
            printf("[Server] Player 1's board initialized successfully.\n");
            break;  // Successfully initialized, move to Player 2's Initialize
        } 
        else {
            // Invalid packet type during Initialize phase, send E 101 error
            send(conn_fd1, "E 101\n", strlen("E 101\n"), 0);
            fprintf(stderr, "[Server] Invalid Initialize or Forfeit packet format from Player 1\n");
        }
    }

    // Player 2 - Initialize board or Forfeit
    printf("[Server] Waiting for valid Initialize or Forfeit packet from Player 2...\n");
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(conn_fd2, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive Initialize or Forfeit packet from Player 2");
            exit(EXIT_FAILURE);
        }

        // Check for "Forfeit" packet from Player 2 first
        if (strcmp(buffer, "F\n") == 0 || strcmp(buffer, "F") == 0) {
            // Player 2 forfeits, send Halt packets to both players
            send(conn_fd2, "H 0\n", strlen("H 0\n"), 0);  // Halt with loss to Player 2
            send(conn_fd1, "H 1\n", strlen("H 1\n"), 0);  // Halt with win to Player 1
            printf("[Server] Player 2 forfeited during Initialize phase. Game halted.\n");
            exit(EXIT_SUCCESS);  // End the game
        }
        // Check for "Initialize" packet from Player 2 if no Forfeit packet is found
        else if (handle_initialize_packet(conn_fd2, player2_board, board_width, board_height, buffer) == 0) {
            printf("[Server] Player 2's board initialized successfully.\n");
            break;  // Successfully initialized, move to the game loop
        }
        else {
            // Invalid packet type during Initialize phase, send E 101 error
            send(conn_fd2, "E 101\n", strlen("E 101\n"), 0);
            fprintf(stderr, "[Server] Invalid Initialize or Forfeit packet format from Player 2\n");
        }
    }

    printf("[Server] Both players have successfully initialized their boards.\n");

    // Initialize shot histories for both players
    player1_shot_history = initialize_shot_history(board_width, board_height);
    player2_shot_history = initialize_shot_history(board_width, board_height);

    // Phase 3: Begin the main game loop (turn-based)

    int player1_remaining_ships = count_remaining_ships(player2_board, board_width, board_height);
    int player2_remaining_ships = count_remaining_ships(player1_board, board_width, board_height);
    int game_is_active = 1;

    // Main Game Loop
    while (game_is_active) {
        // Player 1's turn
        while (1) {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_received = recv(conn_fd1, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0) {
                perror("Failed to receive packet from Player 1");
                game_is_active = 0;
                break;
            }

            if (strncmp(buffer, "S ", 2) == 0) {
                // Handle Shoot packet from Player 1
                int result = handle_shoot_packet(conn_fd1, player2_board, player1_shot_history, board_width, board_height, &player2_remaining_ships, conn_fd2, buffer);
                if (result == 1) {  // Last ship sunk; Player 1 wins
                    // Send Shoot response to Player 1, then read Player 2's turn and send Halt
                    recv(conn_fd2, buffer, BUFFER_SIZE, 0);  // Read Player 2's turn
                    send(conn_fd2, "H 0\n", strlen("H 0\n"), 0);  // Halt with loss to Player 2
                    recv(conn_fd1, buffer, BUFFER_SIZE, 0);  // Read Player 1's turn
                    send(conn_fd1, "H 1\n", strlen("H 1\n"), 0);  // Halt with win to Player 1
                    game_is_active = 0;
                }
                break;  // Switch to Player 2's turn
            } else if (strcmp(buffer, "Q\n") == 0) {
                // Handle Query packet from Player 1
                handle_query_packet(conn_fd1, player1_shot_history, player2_board, board_width, board_height);
            } else if (strcmp(buffer, "F\n") == 0) {
                // Player 1 forfeits; send Halt to Player 1, then read Player 2's turn and send Halt
                send(conn_fd1, "H 0\n", strlen("H 0\n"), 0);  // Halt with loss to Player 1
                recv(conn_fd2, buffer, BUFFER_SIZE, 0);  // Read Player 2's turn
                send(conn_fd2, "H 1\n", strlen("H 1\n"), 0);  // Halt with win to Player 2
                game_is_active = 0;
                break;
            } else {
                // Invalid packet type
                send(conn_fd1, "E 102\n", strlen("E 102\n"), 0);
            }
        }

        if (!game_is_active) break;

        // Player 2's turn
        while (1) {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_received = recv(conn_fd2, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0) {
                perror("Failed to receive packet from Player 2");
                game_is_active = 0;
                break;
            }

            if (strncmp(buffer, "S ", 2) == 0) {
                // Handle Shoot packet from Player 2
                int result = handle_shoot_packet(conn_fd2, player1_board, player2_shot_history, board_width, board_height, &player1_remaining_ships, conn_fd1, buffer);
                if (result == 1) {  // Last ship sunk; Player 2 wins
                    // Send Shoot response to Player 2, then read Player 1's turn and send Halt
                    recv(conn_fd1, buffer, BUFFER_SIZE, 0);  // Read Player 1's turn
                    send(conn_fd1, "H 0\n", strlen("H 0\n"), 0);  // Halt with loss to Player 1
                    recv(conn_fd2, buffer, BUFFER_SIZE, 0);  // Read Player 2's turn
                    send(conn_fd2, "H 1\n", strlen("H 1\n"), 0);  // Halt with win to Player 2
                    game_is_active = 0;
                }
                break;  // Switch to Player 1's turn
            } else if (strcmp(buffer, "Q\n") == 0) {
                // Handle Query packet from Player 2
                handle_query_packet(conn_fd2, player2_shot_history, player1_board, board_width, board_height);
            } else if (strcmp(buffer, "F\n") == 0) {
                // Player 2 forfeits; send Halt to Player 2, then read Player 1's turn and send Halt
                send(conn_fd2, "H 0\n", strlen("H 0\n"), 0);  // Halt with loss to Player 2
                recv(conn_fd1, buffer, BUFFER_SIZE, 0);  // Read Player 1's turn
                send(conn_fd1, "H 1\n", strlen("H 1\n"), 0);  // Halt with win to Player 1
                game_is_active = 0;
                break;
            } else {
                // Invalid packet type
                send(conn_fd2, "E 102\n", strlen("E 102\n"), 0);
            }
        }
    }

    // Cleanup: Free allocated boards and histories after the game
    free_board(player1_board, board_height);
    free_board(player2_board, board_height);
    free_shot_history(player1_shot_history, board_height);
    free_shot_history(player2_shot_history, board_height);
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

    // Start the game loop
    game_loop(conn_fd1, conn_fd2);

    // Close connections after the game ends
    close(conn_fd1);
    close(conn_fd2);
    close(listen_fd1);
    close(listen_fd2);

    return 0;
}

