#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define ENGINE_NAME "Qwen"
#define ENGINE_AUTHOR "Qwen"
#define ENGINE_VERSION "1.2"

#define MAX_PLY 128
#define MAX_MOVES 256
#define MAX_GAME_LENGTH 2048

enum Color { WHITE = 0, BLACK = 1, COLOR_NB = 2 };
enum PieceType { NONE = 0, PAWN = 1, KNIGHT = 2, BISHOP = 3, ROOK = 4, QUEEN = 5, KING = 6, PIECE_TYPE_NB = 7 };
enum Piece { NO_PIECE = 0, W_PAWN = 1, W_KNIGHT = 2, W_BISHOP = 3, W_ROOK = 4, W_QUEEN = 5, W_KING = 6,
             B_PAWN = 9, B_KNIGHT = 10, B_BISHOP = 11, B_ROOK = 12, B_QUEEN = 13, B_KING = 14 };

enum Square {
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
    SQ_NONE = 64, SQUARE_NB = 64
};

enum File { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_NB };
enum Rank { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_NB };

enum CastleRight {
    NO_CASTLING = 0,
    WHITE_OO  = 1,
    WHITE_OOO = 2,
    BLACK_OO  = 4,
    BLACK_OOO = 8,
    KING_SIDE = WHITE_OO | BLACK_OO,
    QUEEN_SIDE = WHITE_OOO | BLACK_OOO,
    WHITE_CASTLING = WHITE_OO | WHITE_OOO,
    BLACK_CASTLING = BLACK_OO | BLACK_OOO,
    ANY_CASTLING = WHITE_CASTLING | BLACK_CASTLING
};

enum MoveFlag {
    MF_NORMAL   = 0,
    MF_PROMO    = 1,
    MF_EP       = 2,
    MF_CASTLING = 3
};

typedef uint32_t Move;

#define MOVE_NONE 0
#define MOVE_NULL (1 << 16)

static inline int move_from(Move m) { return m & 0x3F; }
static inline int move_to(Move m) { return (m >> 6) & 0x3F; }
static inline int move_flags(Move m) { return (m >> 12) & 0x3; }
static inline int move_promo(Move m) { return (m >> 14) & 0x3; }

static inline Move make_move(int from, int to, int flags, int promo) {
    return (Move)(from | (to << 6) | (flags << 12) | (promo << 14));
}

static inline int sq_file(int sq) { return sq & 7; }
static inline int sq_rank(int sq) { return sq >> 3; }
static inline int make_sq(int file, int rank) { return file + (rank << 3); }
static inline int sq_flip(int sq) { return sq ^ 56; }

static inline int piece_type(int piece) { return piece & 7; }
static inline int piece_color(int piece) { return (piece >> 3) & 1; }
static inline int make_piece(int color, int type) { return (color << 3) | type; }

#define BIT(sq) (1ULL << (sq))

#define INFINITY_SCORE 30000
#define MATE_SCORE 29000
#define MATE_IN_MAX (MATE_SCORE - MAX_PLY)
#define VALUE_DRAW 0

typedef uint64_t Bitboard;
typedef uint64_t Key;

typedef struct {
    int castling;
    int ep_square;
    int halfmove;
    Key hash;
    Key pawn_hash;
    int captured_piece;
    int rule50;
} StateInfo;

typedef struct {
    Bitboard piece_bb[PIECE_TYPE_NB];
    Bitboard color_bb[COLOR_NB];
    Bitboard occupied;
    int squares[SQUARE_NB];
    int side;
    int castling;
    int ep_square;
    int halfmove;
    int fullmove;
    Key hash;
    Key pawn_hash;
    StateInfo history[MAX_GAME_LENGTH];
    int hist_ply;
    Key repetition_table[MAX_GAME_LENGTH];
    int rep_count;
} Board;

typedef struct {
    Move moves[MAX_MOVES];
    int scores[MAX_MOVES];
    int count;
} MoveList;

static const char* const START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

static const int piece_values[] = { 0, 100, 320, 330, 500, 900, 20000 };

#endif
