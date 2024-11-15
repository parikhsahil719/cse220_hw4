#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <asm-generic/socket.h>

#define PORT_PLAYER1 2201
#define PORT_PLAYER2 2202
#define BUFFER_SIZE 1024
#define HIT 'H'
#define MISS 'M'
#define EMPTY 0

int **initialize_board(int width, int height) {
    int **board = malloc(height * sizeof(int *));
    if (!board) {
        return NULL;
    }
    for (int i = 0; i < height; i++) {
        board[i] = calloc(width, sizeof(int));
        if (!board[i]) {
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

int base_shapes[7][4][2] = {
    {{0, 0}, {0, 1}, {1, 0}, {1, 1}},  // O-piece
    {{0, 0}, {1, 0}, {2, 0}, {3, 0}},  // I-piece
    {{0, 0}, {0, 1}, {1, 1}, {1, 2}},  // S-piece
    {{0, 0}, {1, 0}, {2, 0}, {2, 1}},  // L-piece
    {{0, 1}, {0, 0}, {1, 1}, {1, 2}},  // Z-piece
    {{0, 0}, {1, 0}, {2, 0}, {2, -1}}, // J-piece
    {{0, 0}, {1, -1}, {1, 0}, {1, 1}}  // T-piece
};

void rotate_90(int *x, int *y) {
    int temp = *x;
    *x = *y;
    *y = -temp;
}

void get_piece_coordinates(int piece_type, int rotation, int ref_row, int ref_col, int coords[4][2]) {
    for (int i = 0; i < 4; i++) {
        int x = base_shapes[piece_type][i][0];
        int y = base_shapes[piece_type][i][1];

        if (rotation > 1) {
            for (int r = 0; r < rotation - 1; r++) {
                rotate_90(&x, &y);
            }
        }
        coords[i][0] = ref_row + x;
        coords[i][1] = ref_col + y;
    }
}

int place_piece(int **board, int board_width, int board_height, int piece_type, int rotation, int ref_row, int ref_col, int piece_id) {
    int coords[4][2];
    get_piece_coordinates(piece_type, rotation, ref_row, ref_col, coords);

    for (int i = 0; i < 4; i++) {
        int row = coords[i][0];
        int col = coords[i][1];

        if (row < 0 || row >= board_height || col < 0 || col >= board_width) {
            return 302;
        }
        if (board[row][col] != 0) {
            return 303;
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

        if (row < 0 || row >= board_height || col < 0 || col >= board_width) {
            return 302;
        }

        if (board[row][col] != 0) {
            return 303;
        }
    }

    return 0;
}

int handle_initialize_packet(int conn_fd, int **board, int board_width, int board_height, char *packet) {
    int piece_type, rotation, ref_row, ref_col;
    int num_pieces = 5;
    int offset = 2;
    int lowest_error = 0;

    if (strncmp(packet, "I ", 2) != 0) {
        send(conn_fd, "E 101", strlen("E 101"), 0);
        return -1;
    }

    int parameter_count = 0;
    for (int i = 2; packet[i] != '\0'; i++) {
        if (!isspace(packet[i]) && (i == 2 || isspace(packet[i - 1]))) {
            parameter_count++;
        }
    }

    if (parameter_count != (num_pieces * 4)) {
        send(conn_fd, "E 201", strlen("E 201"), 0);
        return -1;
    }

    int **temp_board = initialize_board(board_width, board_height);
    if (!temp_board) {
        perror("Failed to allocate temporary board");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_pieces; i++) {
        int parsed = sscanf(packet + offset, "%d %d %d %d", &piece_type, &rotation, &ref_row, &ref_col);

        if (parsed != 4) {
            if (lowest_error == 0 || lowest_error > 201) {
                lowest_error = 201;
            }
            continue;
        }

        if (piece_type < 1 || piece_type > 7) {
            if (lowest_error == 0 || lowest_error > 300) {
                lowest_error = 300;
            }
        }

        if (rotation < 1 || rotation > 4) {
            if (lowest_error == 0 || lowest_error > 301) {
                lowest_error = 301;
            }
        }

        piece_type -= 1;
        rotation -= 1;

        int error_code = place_piece(temp_board, board_width, board_height, piece_type, rotation, ref_row, ref_col, i + 1);
        if (error_code && (lowest_error == 0 || lowest_error > error_code)) {
            lowest_error = error_code;
        }

        offset += snprintf(NULL, 0, "%d %d %d %d ", piece_type + 1, rotation + 1, ref_row, ref_col);
    }

    free_board(temp_board, board_height);

    if (lowest_error != 0) {
        char error_msg[BUFFER_SIZE];
        snprintf(error_msg, sizeof(error_msg), "E %d", lowest_error);
        send(conn_fd, error_msg, strlen(error_msg), 0);
        return -1; 
    }

    offset = 2;
    for (int i = 0; i < num_pieces; i++) {
        sscanf(packet + offset, "%d %d %d %d", &piece_type, &rotation, &ref_row, &ref_col);
        piece_type -= 1;
        rotation -= 1;

        place_piece(board, board_width, board_height, piece_type, rotation, ref_row, ref_col, i + 1);

        offset += snprintf(NULL, 0, "%d %d %d %d ", piece_type + 1, rotation + 1, ref_row, ref_col);
    }

    send(conn_fd, "A", strlen("A"), 0);
    return 0;
}

int is_ship_sunk(int **board, int width, int height, int piece_id) {
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            if (board[i][j] == piece_id) {
                return 0;
            }
        }
    }
    return 1;  // Ship is fully sunk
}

int count_remaining_ships(int **opponent_board, int board_width, int board_height) {
    int ship_counts[5] = {0};
    int remaining_ships = 0;

    for (int i = 0; i < board_height; i++) {
        for (int j = 0; j < board_width; j++) {
            int cell = opponent_board[i][j];
            if (cell >= 1 && cell <= 5) {
                ship_counts[cell - 1] = 1;
            }
        }
    }

    for (int i = 0; i < 5; i++) {
        remaining_ships += ship_counts[i];
    }

    return remaining_ships;
}

char **initialize_shot_history(int width, int height) {
    char **history = malloc(height * sizeof(char *));
    if (!history){
        return NULL;
    }
    for (int i = 0; i < height; i++) {
        history[i] = malloc(width * sizeof(char));
        if (!history[i]) {
            for (int j = 0; j < i; j++) {
                free(history[j]);
            }
            free(history);
            return NULL;
        }
        memset(history[i], 0, width * sizeof(char));
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
    char extra;

    if (sscanf(packet, "S %d %d %c", &row, &col, &extra) != 2) {
        printf("[Server] Invalid shoot packet format: '%s'\n", packet);
        send(conn_fd, "E 202", strlen("E 202"), 0);
        return -1;
    }

    if (row < 0 || row >= board_height || col < 0 || col >= board_width) {
        printf("[Server] Out-of-bounds coordinates: row=%d, col=%d (board: %dx%d)\n", row, col, board_width, board_height);
        send(conn_fd, "E 400", strlen("E 400"), 0);
        return -1;
    }

    if (shot_history[row][col] != EMPTY) {
        printf("[Server] Cell already shot at: row=%d, col=%d\n", row, col);
        send(conn_fd, "E 401", strlen("E 401"), 0);
        return -1;
    }

    char shot_result;
    int piece_id = opponent_board[row][col];
    if (piece_id != 0) {
        shot_result = 'H';
        shot_history[row][col] = HIT;
        opponent_board[row][col] = 'H';
        printf("[Server] Hit detected at row=%d, col=%d\n", row, col);

        if (is_ship_sunk(opponent_board, board_width, board_height, piece_id)) {
            (*remaining_ships)--;
            printf("[Server] Ship sunk! Remaining ships: %d\n", *remaining_ships);
        }
    } else {
        shot_result = 'M';
        shot_history[row][col] = MISS;
        printf("[Server] Miss at row=%d, col=%d\n", row, col);
    }

    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "R %d %c", *remaining_ships, shot_result);
    send(conn_fd, response, strlen(response), 0);
    printf("[Server] Shot result sent: %s\n", response);

    if (*remaining_ships == 0) {
        printf("[Server] All ships sunk. Ending game.\n");
        send(conn_fd_opponent, "H 0", strlen("H 0"), 0);
        recv(conn_fd_opponent, response, BUFFER_SIZE, 0);
        send(conn_fd, "H 1", strlen("H 1"), 0);
        return 1;
    }

    return 0;
}

void handle_query_packet(int conn_fd, char **shot_history, int **opponent_board, int board_width, int board_height) {
    int remaining_ships = count_remaining_ships(opponent_board, board_width, board_height);

    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "G %d", remaining_ships);

    for (int i = 0; i < board_height; i++) {
        for (int j = 0; j < board_width; j++) {
            if (shot_history[i][j] == 'H' || shot_history[i][j] == 'M') {
                char shot_entry[16];
                snprintf(shot_entry, sizeof(shot_entry), " %c %d %d", shot_history[i][j], i, j);
                strncat(response, shot_entry, sizeof(response) - strlen(response) - 1);
            }
        }
    }

    send(conn_fd, response, strlen(response), 0);
}

void handle_forfeit_packet(int conn_fd, int conn_fd_opponent) {
    send(conn_fd, "H 0", strlen("H 0"), 0);
    char buffer[BUFFER_SIZE];
    recv(conn_fd, buffer, BUFFER_SIZE, 0);
    send(conn_fd_opponent, "H 1", strlen("H 1"), 0);
}

void game_loop(int conn_fd1, int conn_fd2) {
    char buffer[BUFFER_SIZE];
    int board_width, board_height;
    int **player1_board = NULL;
    int **player2_board = NULL;
    char **player1_shot_history = NULL;
    char **player2_shot_history = NULL;

    // Phase 1: Waiting for "Begin" or "Forfeit" packets from both players
    printf("[Server] Waiting for valid Begin or Forfeit packet from Player 1...\n");
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(conn_fd1, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive Begin or Forfeit packet from Player 1");
            exit(EXIT_FAILURE);
        }

        // Ensure null-termination for safety
        buffer[bytes_received] = '\0';

        // Check if the packet starts with 'B'
        if (strncmp(buffer, "B", 1) == 0) {
            char remaining_chars;
            int parsed = sscanf(buffer, "B %d %d%c", &board_width, &board_height, &remaining_chars);
            
            if (parsed == 2) {  // Valid format with exactly two parameters
                if (board_width >= 10 && board_height >= 10) {
                    send(conn_fd1, "A", strlen("A"), 0);
                    printf("[Server] Board initialized with size %dx%d\n", board_width, board_height);
                    break;
                } else {  // Invalid dimensions
                    send(conn_fd1, "E 200", strlen("E 200"), 0);
                    fprintf(stderr, "[Server] Invalid board dimensions received from Player 1\n");
                }
            } else {  // Malformed "B" packet
                send(conn_fd1, "E 200", strlen("E 200"), 0);
                fprintf(stderr, "[Server] Malformed Begin packet received from Player 1\n");
            }
        }
        // Check if the packet is a "Forfeit" packet
        else if (strcmp(buffer, "F") == 0 || strcmp(buffer, "F\n") == 0) {
            send(conn_fd1, "H 0", strlen("H 0"), 0);
            send(conn_fd2, "H 1", strlen("H 1"), 0);
            printf("[Server] Player 1 forfeited during Begin phase. Game halted.\n");
            exit(EXIT_SUCCESS);
        }
        // If the packet is neither "B" nor "F", it is an invalid command
        else {
            send(conn_fd1, "E 100", strlen("E 100"), 0);
            fprintf(stderr, "[Server] Invalid packet type received from Player 1\n");
        }
    }

    printf("[Server] Waiting for valid Begin or Forfeit packet from Player 2...\n");
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(conn_fd2, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive Begin or Forfeit packet from Player 2");
            exit(EXIT_FAILURE);
        }

        // Ensure null-termination for safety
        buffer[bytes_received] = '\0';

        // Validate Player 2's packet strictly
        if (strcmp(buffer, "B") == 0 || strcmp(buffer, "B\n") == 0) {  // Accept "B" or "B\n" only
            send(conn_fd2, "A", strlen("A"), 0);
            printf("[Server] Valid Begin packet received from Player 2\n");
            break;
        } 
        else if (strncmp(buffer, "B ", 2) == 0) {  // Reject "B" with parameters
            send(conn_fd2, "E 200", strlen("E 200"), 0);
            fprintf(stderr, "[Server] Invalid Begin packet format for Player 2: extra parameters\n");
        }
        else if (strcmp(buffer, "F") == 0 || strcmp(buffer, "F\n") == 0) {  // Check for "Forfeit"
            send(conn_fd2, "H 0", strlen("H 0"), 0);
            send(conn_fd1, "H 1", strlen("H 1"), 0);
            printf("[Server] Player 2 forfeited during Begin phase. Game halted.\n");
            exit(EXIT_SUCCESS);
        } 
        else {  // Any other invalid format
            send(conn_fd2, "E 100", strlen("E 100"), 0);
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

        buffer[bytes_received] = '\0';

        // Check for "Forfeit" packet from Player 1
        if (strcmp(buffer, "F\n") == 0 || strcmp(buffer, "F") == 0) {
            send(conn_fd1, "H 0", strlen("H 0"), 0);
            send(conn_fd2, "H 1", strlen("H 1"), 0);
            printf("[Server] Player 1 forfeited during Initialize phase. Game halted.\n");
            exit(EXIT_SUCCESS);
        }

        if (handle_initialize_packet(conn_fd1, player1_board, board_width, board_height, buffer) == 0) {
            printf("[Server] Player 1's board initialized successfully.\n");
            break;
        }
    }

    printf("[Server] Waiting for valid Initialize or Forfeit packet from Player 2...\n");
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(conn_fd2, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive Initialize or Forfeit packet from Player 2");
            exit(EXIT_FAILURE);
        }

        buffer[bytes_received] = '\0';

        // Check for "Forfeit" packet from Player 2
        if (strcmp(buffer, "F\n") == 0 || strcmp(buffer, "F") == 0) {
            send(conn_fd2, "H 0", strlen("H 0"), 0);
            send(conn_fd1, "H 1", strlen("H 1"), 0);
            printf("[Server] Player 2 forfeited during Initialize phase. Game halted.\n");
            exit(EXIT_SUCCESS);
        }

        if (handle_initialize_packet(conn_fd2, player2_board, board_width, board_height, buffer) == 0) {
            printf("[Server] Player 2's board initialized successfully.\n");
            break;
        }
    }

    printf("[Server] Both players have successfully initialized their boards.\n");

    // Initialize shot histories for both players
    player1_shot_history = initialize_shot_history(board_width, board_height);
    player2_shot_history = initialize_shot_history(board_width, board_height);

    int player1_remaining_ships = count_remaining_ships(player2_board, board_width, board_height);
    int player2_remaining_ships = count_remaining_ships(player1_board, board_width, board_height);
    int game_is_active = 1;

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
                int result = handle_shoot_packet(conn_fd1, player2_board, player1_shot_history, board_width, board_height, &player2_remaining_ships, conn_fd2, buffer);
                if (result == 1) {
                    recv(conn_fd2, buffer, BUFFER_SIZE, 0);
                    send(conn_fd2, "H 0", strlen("H 0"), 0);
                    recv(conn_fd1, buffer, BUFFER_SIZE, 0);
                    send(conn_fd1, "H 1", strlen("H 1"), 0);
                    game_is_active = 0;
                }
                if (result == 0) {
                    break;
                }
                continue;
            } 
            else if (strcmp(buffer, "Q\n") == 0 || strcmp(buffer, "Q") == 0) {
                handle_query_packet(conn_fd1, player1_shot_history, player2_board, board_width, board_height);
                continue;
            } 
            else if (strcmp(buffer, "F\n") == 0 || strcmp(buffer, "F") == 0) {
                send(conn_fd1, "H 0", strlen("H 0"), 0);
                recv(conn_fd2, buffer, BUFFER_SIZE, 0);
                send(conn_fd2, "H 1", strlen("H 1"), 0);
                game_is_active = 0;
                break;
            } else {
                send(conn_fd1, "E 102", strlen("E 102"), 0);
            }
        }

        if (!game_is_active) {
            break;
        }

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
                int result = handle_shoot_packet(conn_fd2, player1_board, player2_shot_history, board_width, board_height, &player1_remaining_ships, conn_fd1, buffer);
                if (result == 1) {
                    recv(conn_fd1, buffer, BUFFER_SIZE, 0);
                    send(conn_fd1, "H 0", strlen("H 0"), 0);
                    recv(conn_fd2, buffer, BUFFER_SIZE, 0);
                    send(conn_fd2, "H 1", strlen("H 1"), 0);
                    game_is_active = 0;
                }
                if (result == 0) {
                    break;
                }
                continue;
            } 
            else if (strcmp(buffer, "Q\n") == 0 || strcmp(buffer, "Q") == 0) {
                handle_query_packet(conn_fd2, player2_shot_history, player1_board, board_width, board_height);
                continue;
            } 
            else if (strcmp(buffer, "F\n") == 0 || strcmp(buffer, "F") == 0) {
                send(conn_fd2, "H 0", strlen("H 0"), 0);
                recv(conn_fd1, buffer, BUFFER_SIZE, 0);
                send(conn_fd1, "H 1", strlen("H 1"), 0);
                game_is_active = 0;
                break;
            } else {
                send(conn_fd2, "E 102", strlen("E 102"), 0);
            }
        }
    }

    // Free allocated boards and histories after the game ends
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

    // Socket creation for Player 1
    if ((listen_fd1 = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[Server] socket() failed for Player 1");
        exit(EXIT_FAILURE);
    }

    // Set socket options for Player 1
    if (setsockopt(listen_fd1, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
        perror("[Server] setsockopt() failed for Player 1");
        close(listen_fd1);
        exit(EXIT_FAILURE);
    }

    // Initialize address structure for Player 1
    memset(&address1, 0, sizeof(address1));
    address1.sin_family = AF_INET;
    address1.sin_addr.s_addr = INADDR_ANY;
    address1.sin_port = htons(PORT_PLAYER1);

    // Bind socket to port for Player 1
    if (bind(listen_fd1, (struct sockaddr *)&address1, sizeof(address1)) == -1) {
        perror("[Server] bind() failed for Player 1");
        close(listen_fd1);
        exit(EXIT_FAILURE);
    }

    // Listen for Player 1
    if (listen(listen_fd1, 1) == -1) {
        perror("[Server] listen() failed for Player 1");
        close(listen_fd1);
        exit(EXIT_FAILURE);
    }
    printf("[Server] Listening for Player 1 on port %d...\n", PORT_PLAYER1);

    // Socket creation for Player 2
    if ((listen_fd2 = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[Server] socket() failed for Player 2");
        close(listen_fd1);
        exit(EXIT_FAILURE);
    }

    // Set socket options for Player 2
    if (setsockopt(listen_fd2, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
        perror("[Server] setsockopt() failed for Player 2");
        close(listen_fd1);
        close(listen_fd2);
        exit(EXIT_FAILURE);
    }

    // Initialize address structure for Player 2
    memset(&address2, 0, sizeof(address2));
    address2.sin_family = AF_INET;
    address2.sin_addr.s_addr = INADDR_ANY;
    address2.sin_port = htons(PORT_PLAYER2);

    // Bind socket to port for Player 2
    if (bind(listen_fd2, (struct sockaddr *)&address2, sizeof(address2)) == -1) {
        perror("[Server] bind() failed for Player 2");
        close(listen_fd1);
        close(listen_fd2);
        exit(EXIT_FAILURE);
    }

    // Listen for Player 2
    if (listen(listen_fd2, 1) == -1) {
        perror("[Server] listen() failed for Player 2");
        close(listen_fd1);
        close(listen_fd2);
        exit(EXIT_FAILURE);
    }
    printf("[Server] Listening for Player 2 on port %d...\n", PORT_PLAYER2);

    // Accept connection from Player 1
    if ((conn_fd1 = accept(listen_fd1, (struct sockaddr *)&address1, &addrlen)) == -1) {
        perror("[Server] accept() failed for Player 1");
        close(listen_fd1);
        close(listen_fd2);
        exit(EXIT_FAILURE);
    }
    printf("[Server] Player 1 connected!\n");

    // Accept connection from Player 2
    if ((conn_fd2 = accept(listen_fd2, (struct sockaddr *)&address2, &addrlen)) == -1) {
        perror("[Server] accept() failed for Player 2");
        close(listen_fd1);
        close(listen_fd2);
        close(conn_fd1);
        exit(EXIT_FAILURE);
    }
    printf("[Server] Player 2 connected!\n");

    // Start the game loop
    game_loop(conn_fd1, conn_fd2);

    close(conn_fd1);
    close(conn_fd2);
    close(listen_fd1);
    close(listen_fd2);

    return 0;
}