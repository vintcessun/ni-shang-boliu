#ifndef RIVER_CROSSING_H
#define RIVER_CROSSING_H

#include <stdbool.h>

typedef enum {
    LEFT_BANK,
    RIGHT_BANK
} BankPosition;

typedef struct {
    int police;
    int thief;
} Bank;

typedef struct {
    Bank left;
    Bank right;
    BankPosition boat_position;
} GameMap;

typedef enum {
    PROCESSING,
    WIN,
    LOSE
} GameState;

typedef struct {
    int police;
    int thief;
    bool to_left; // true: to left bank, false: to right bank
} Suggestion;

void game_init();
GameState game_update(int police_move, int thief_move, bool to_left);
const Suggestion* get_suggestions(int* count);
GameMap get_current_map();

#endif
