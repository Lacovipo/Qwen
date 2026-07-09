#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"
#include "board.h"

typedef struct {
    int depth;
    int64_t nodes;
    int time_ms;
    int64_t nodes_limit;
    int64_t time_limit;
    int64_t time_inc;
    bool infinite;
    bool stopped;
    int64_t start_time;
    Move best_move;
    Move ponder_move;
    int sel_depth;
} SearchInfo;

typedef struct {
    Key hash;
    Move best_move;
    int depth;
    int score;
    int flag;
    int age;
} TTEntry;

#define TT_SIZE (1 << 20)
#define TT_MASK (TT_SIZE - 1)
#define HASH_EXACT 0
#define HASH_ALPHA 1
#define HASH_BETA  2

void search_init(void);
void search_clear_tt(void);
Move search_best_move(Board *b, SearchInfo *info);
void search_set_time(SearchInfo *info, int time_ms, int inc_ms);

extern TTEntry tt_table[TT_SIZE];
extern int tt_age;

int64_t get_time_ms(void);

#endif
