#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "types.h"
#include "board.h"

void movegen_init(void);
void movegen_generate_legal(const Board *b, MoveList *list);
void movegen_generate_captures(const Board *b, MoveList *list);
int movegen_count_legal(const Board *b);

extern Bitboard knight_attacks[SQUARE_NB];
extern Bitboard king_attacks[SQUARE_NB];
extern Bitboard pawn_attacks[COLOR_NB][SQUARE_NB];

Bitboard bishop_attacks(int sq, Bitboard occupied);
Bitboard rook_attacks(int sq, Bitboard occupied);
Bitboard queen_attacks(int sq, Bitboard occupied);

#endif
