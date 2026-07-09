#ifndef BOARD_H
#define BOARD_H

#include "types.h"

void board_init(Board *b);
void board_from_fen(Board *b, const char *fen);
void board_print(const Board *b);
void board_to_fen(const Board *b, char *fen);

void board_make_move(Board *b, Move m);
void board_unmake_move(Board *b, Move m);
void board_make_null(Board *b);
void board_unmake_null(Board *b);

bool board_is_attacked(const Board *b, int sq, int by_color);
int board_king_square(const Board *b, int color);
bool board_in_check(const Board *b);
bool board_is_repetition(const Board *b);
bool board_is_draw(const Board *b);
bool board_has_non_pawn_material(const Board *b, int color);

void zobrist_init(void);
Key zobrist_compute(const Board *b);

extern Key zobrist_pieces[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
extern Key zobrist_castling[16];
extern Key zobrist_ep[FILE_NB];
extern Key zobrist_side;

void move_to_str(Move m, char *str);
Move str_to_move(const Board *b, const char *str);

#endif
