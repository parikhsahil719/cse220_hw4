#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT_PLAYER1 2201
#define PORT_PLAYER2 2202
#define BUFFER_SIZE 1024
#define MAX_GUESSES 100

// Struct to represent each guess in the game
typedef struct {
    int row;
    int column;
    char result;
} Guess;

// Struct to represent the game state
typedef struct {
    int ships_remaining;
    Guess guesses[MAX_GUESSES];
    int guess_count;
} GameState;

// Initialize the game state
void init_game_state(GameState *state, int initial_ships) {
    state->ships_remaining = initial_ships;
    state->guess_count = 0;
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

// Send halt packet to indicate game end
void send_halt(int socket, int win_status) {
    char halt_msg[BUFFER_SIZE];
    snprintf(halt_msg, sizeof(halt_msg), "H %d", win_status);
    send(socket, halt_msg, strlen(halt_msg), 0);
}

// Add a guess to the game state
void add_guess(GameState *state, int row, int column, char result) {
    if (state->guess_count < MAX_GUESSES) {
        state->guesses[state->guess_count].row = row;
        state->guesses[state->guess_count].column = column;
        state->guesses[state->guess_count].result = result;
        state->guess_count++;
    }
}

// Send query response packet with game state
void send_query_response(int socket, GameState *state) {
    char query_msg[BUFFER_SIZE];
    int offset = snprintf(query_msg, sizeof(query_msg), "G %d ", state->ships_remaining);

    // Add each guess in row-major order
    for (int i = 0; i < state->guess_count; i++) {
        offset += snprintf(query_msg + offset, sizeof(query_msg) - offset, "%c %d %d ",
                           state->guesses[i].result,
                           state->guesses[i].row,
                           state->guesses[i].column);
    }

    if (offset > 0 && query_msg[offset - 1] == ' ') {
        query_msg[offset - 1] = '\0';
    }

    send(socket, query_msg, strlen(query_msg), 0);
}

// Send shot response packet with hit or miss result
void send_shot_response(int socket, GameState *state, int row, int column, char hit_miss) {
    char shot_msg[BUFFER_SIZE];
    snprintf(shot_msg, sizeof(shot_msg), "R %d %c", state->ships_remaining, hit_miss);
    send(socket, shot_msg, strlen(shot_msg), 0);

    add_guess(state, row, column, hit_miss);
    if (hit_miss == 'H') {
        state->ships_remaining--;
    }
}

// Main server function
int main() {
    int server_socket1, server_socket2, client_socket1, client_socket2;
    struct sockaddr_in server_addr1, server_addr2, client_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    GameState game_state1, game_state2;
    init_game_state(&game_state1, 5);
    init_game_state(&game_state2, 5);

    // Initialize server sockets for both players
    server_socket1 = socket(AF_INET, SOCK_STREAM, 0);
    server_socket2 = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket1 == -1 || server_socket2 == -1) {
        return 1;
    }

    // Server 1 setup for Player 1
    server_addr1.sin_family = AF_INET;
    server_addr1.sin_addr.s_addr = INADDR_ANY;
    server_addr1.sin_port = htons(PORT_PLAYER1);
    
    if (bind(server_socket1, (struct sockaddr *)&server_addr1, sizeof(server_addr1)) < 0) {
        return 1;
    }
    listen(server_socket1, 3);

    // Server 2 setup for Player 2
    server_addr2.sin_family = AF_INET;
    server_addr2.sin_addr.s_addr = INADDR_ANY;
    server_addr2.sin_port = htons(PORT_PLAYER2);
    
    if (bind(server_socket2, (struct sockaddr *)&server_addr2, sizeof(server_addr2)) < 0) {
        return 1;
    }
    listen(server_socket2, 3);

    // Accept Player 1 connection
    client_socket1 = accept(server_socket1, (struct sockaddr *)&client_addr, &addr_len);
    if (client_socket1 < 0) {
        return 1;
    }

    // Accept Player 2 connection
    client_socket2 = accept(server_socket2, (struct sockaddr *)&client_addr, &addr_len);
    if (client_socket2 < 0) {
        return 1;
    }

    // Main game loop
    int game_over = 0;
    int current_player = 1;
    char buffer[BUFFER_SIZE];
    GameState *current_game_state, *opponent_game_state;
    int current_socket, opponent_socket;

    while (!game_over) {
        // Set current and opponent based on turn
        if (current_player == 1) {
            current_socket = client_socket1;
            opponent_socket = client_socket2;
            current_game_state = &game_state1;
            opponent_game_state = &game_state2;
        } else {
            current_socket = client_socket2;
            opponent_socket = client_socket1;
            current_game_state = &game_state2;
            opponent_game_state = &game_state1;
        }

        int bytes_received = recv(current_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            send_halt(opponent_socket, 1);
            break;
        }
        buffer[bytes_received] = '\0';

        if (buffer[0] == 'S') {
            int row, column;
            sscanf(buffer + 2, "%d %d", &row, &column);

            char hit_miss = (row + column) % 2 == 0 ? 'H' : 'M';
            send_shot_response(current_socket, opponent_game_state, row, column, hit_miss);

            if (hit_miss == 'H' && opponent_game_state->ships_remaining == 0) {
                send_halt(current_socket, 1);
                send_halt(opponent_socket, 0);
                game_over = 1;
            }
        } else if (buffer[0] == 'Q') {
            send_query_response(current_socket, current_game_state);
        } else if (buffer[0] == 'F') {
            send_halt(current_socket, 0);
            send_halt(opponent_socket, 1);
            game_over = 1;
        } else {
            send_error(current_socket, 102);
        }

        if (!game_over) {
            current_player = (current_player == 1) ? 2 : 1;
        }
    }

    // Close sockets
    close(client_socket1);
    close(client_socket2);
    close(server_socket1);
    close(server_socket2);

    return 0;
}