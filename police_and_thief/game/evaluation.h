#ifndef EVALUATION_H
#define EVALUATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint32_t move_count;
    uint32_t illegal_moves;
    uint32_t suggestion_followed;
    uint32_t total_time_ms;
    uint32_t score;
} GameStats;

void eval_init();
void eval_record_move(bool is_legal, bool followed_suggestion);
void eval_update_time(uint32_t elapsed_ms);
void eval_generate_report(char* buffer, size_t size);
GameStats* eval_get_stats();

#endif
