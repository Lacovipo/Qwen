#ifndef EVAL_H
#define EVAL_H

#include "types.h"
#include "board.h"

void eval_init(void);
int eval_evaluate(const Board *b);

#endif
