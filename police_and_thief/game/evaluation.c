#include "evaluation.h"
#include <string.h>

static GameStats stats;

void eval_init() {
    memset(&stats, 0, sizeof(stats));
}

void eval_record_move(bool is_legal, bool followed_suggestion) {
    stats.move_count++;
    if(!is_legal) {
        stats.illegal_moves++;
    }
    if(followed_suggestion) {
        stats.suggestion_followed++;
    }
}

void eval_update_time(uint32_t elapsed_ms) {
    stats.total_time_ms += elapsed_ms;
}

void eval_generate_report(char* buffer, size_t size) {
    // 计算得分 (100分制)
    stats.score = 100;
    if(stats.move_count > 0) {
        stats.score -= (stats.illegal_moves * 20);
        stats.score -= ((stats.move_count - stats.suggestion_followed) * 5);
        stats.score = stats.score > 0 ? stats.score : 0;
    }

    snprintf(buffer, size, 
        "游戏统计:\n"
        "总步数: %u\n"
        "非法移动: %u\n"
        "采纳建议: %u\n"
        "用时: %u ms\n"
        "得分: %u/100\n",
        stats.move_count,
        stats.illegal_moves,
        stats.suggestion_followed,
        stats.total_time_ms,
        stats.score);
}

GameStats* eval_get_stats() {
    return &stats;
}
