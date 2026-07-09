#include "eval.h"
#include "movegen.h"

static const int pawn_pst[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10,-20,-20, 10, 10,  5,
     5, -5,-10,  0,  0,-10, -5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5,  5, 10, 25, 25, 10,  5,  5,
    10, 10, 20, 30, 30, 20, 10, 10,
    50, 50, 50, 50, 50, 50, 50, 50,
     0,  0,  0,  0,  0,  0,  0,  0
};

static const int knight_pst[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};

static const int bishop_pst[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};

static const int rook_pst[64] = {
     0,  0,  0,  5,  5,  0,  0,  0,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     5, 10, 10, 10, 10, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

static const int queen_pst[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -10,  5,  5,  5,  5,  5,  0,-10,
      0,  0,  5,  5,  5,  5,  0, -5,
     -5,  0,  5,  5,  5,  5,  0, -5,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

static const int king_pst[64] = {
     20, 30, 10,  0,  0, 10, 30, 20,
     20, 20,  0,  0,  0,  0, 20, 20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30
};

static const int* pst[PIECE_TYPE_NB] = {
    NULL, pawn_pst, knight_pst, bishop_pst, rook_pst, queen_pst, king_pst
};

static const int mobility_bonus[PIECE_TYPE_NB] = { 0, 0, 4, 5, 2, 1, 0 };

static const int passed_pawn_bonus[8] = { 0, 10, 20, 35, 60, 100, 175, 0 };

void eval_init(void) {
}

static int eval_passed_pawns(const Board *b, int color) {
    int score = 0;
    Bitboard pawns = b->piece_bb[PAWN] & b->color_bb[color];
    Bitboard enemy_pawns = b->piece_bb[PAWN] & b->color_bb[color ^ 1];

    while (pawns) {
        int sq = __builtin_ctzll(pawns);
        pawns &= pawns - 1;

        int file = sq_file(sq);
        int rank = sq_rank(sq);

        Bitboard mask = 0;
        for (int f = (file > 0 ? file - 1 : 0); f <= (file < 7 ? file + 1 : 7); f++) {
            if (color == WHITE) {
                for (int r = rank + 1; r <= 7; r++)
                    mask |= BIT(make_sq(f, r));
            } else {
                for (int r = rank - 1; r >= 0; r--)
                    mask |= BIT(make_sq(f, r));
            }
        }

        if (!(mask & enemy_pawns)) {
            int rel_rank = color == WHITE ? rank : 7 - rank;
            score += passed_pawn_bonus[rel_rank];
        }
    }

    return score;
}

static int eval_mobility(const Board *b, int color) {
    int score = 0;
    Bitboard own = b->color_bb[color];
    Bitboard own_pawns = b->piece_bb[PAWN] & own;
    Bitboard mask = ~(own_pawns | (b->piece_bb[KING] & own));

    Bitboard knights = b->piece_bb[KNIGHT] & own;
    while (knights) {
        int sq = __builtin_ctzll(knights);
        knights &= knights - 1;
        score += __builtin_popcountll(knight_attacks[sq] & mask) * mobility_bonus[KNIGHT];
    }

    Bitboard bishops = b->piece_bb[BISHOP] & own;
    while (bishops) {
        int sq = __builtin_ctzll(bishops);
        bishops &= bishops - 1;
        score += __builtin_popcountll(bishop_attacks(sq, b->occupied) & mask) * mobility_bonus[BISHOP];
    }

    Bitboard rooks = b->piece_bb[ROOK] & own;
    while (rooks) {
        int sq = __builtin_ctzll(rooks);
        rooks &= rooks - 1;
        score += __builtin_popcountll(rook_attacks(sq, b->occupied) & mask) * mobility_bonus[ROOK];
    }

    Bitboard queens = b->piece_bb[QUEEN] & own;
    while (queens) {
        int sq = __builtin_ctzll(queens);
        queens &= queens - 1;
        score += __builtin_popcountll(queen_attacks(sq, b->occupied) & mask) * mobility_bonus[QUEEN];
    }

    return score;
}

static int eval_bishop_pair(const Board *b, int color) {
    Bitboard bishops = b->piece_bb[BISHOP] & b->color_bb[color];
    if (__builtin_popcountll(bishops) >= 2)
        return 30;
    return 0;
}

static int eval_rook_open_file(const Board *b, int color) {
    int score = 0;
    Bitboard rooks = b->piece_bb[ROOK] & b->color_bb[color];
    Bitboard all_pawns = b->piece_bb[PAWN];
    Bitboard own_pawns = b->piece_bb[PAWN] & b->color_bb[color];

    while (rooks) {
        int sq = __builtin_ctzll(rooks);
        rooks &= rooks - 1;
        int file = sq_file(sq);

        Bitboard file_mask = 0;
        for (int r = 0; r < 8; r++)
            file_mask |= BIT(make_sq(file, r));

        if (!(file_mask & all_pawns))
            score += 20;
        else if (!(file_mask & own_pawns))
            score += 10;
    }

    return score;
}

static int eval_king_safety(const Board *b, int color) {
    int score = 0;
    int enemy = color ^ 1;
    int ksq = board_king_square(b, enemy);
    if (ksq == SQ_NONE) return 0;

    Bitboard king_zone = king_attacks[ksq] | BIT(ksq);
    int attack_weight = 0;
    int attack_count = 0;

    Bitboard knights = b->piece_bb[KNIGHT] & b->color_bb[color];
    while (knights) {
        int sq = __builtin_ctzll(knights);
        knights &= knights - 1;
        Bitboard attacks = knight_attacks[sq] & king_zone;
        if (attacks) {
            attack_count += __builtin_popcountll(attacks);
            attack_weight += 2;
        }
    }

    Bitboard bishops = b->piece_bb[BISHOP] & b->color_bb[color];
    while (bishops) {
        int sq = __builtin_ctzll(bishops);
        bishops &= bishops - 1;
        Bitboard attacks = bishop_attacks(sq, b->occupied) & king_zone;
        if (attacks) {
            attack_count += __builtin_popcountll(attacks);
            attack_weight += 2;
        }
    }

    Bitboard rooks = b->piece_bb[ROOK] & b->color_bb[color];
    while (rooks) {
        int sq = __builtin_ctzll(rooks);
        rooks &= rooks - 1;
        Bitboard attacks = rook_attacks(sq, b->occupied) & king_zone;
        if (attacks) {
            attack_count += __builtin_popcountll(attacks);
            attack_weight += 3;
        }
    }

    Bitboard queens = b->piece_bb[QUEEN] & b->color_bb[color];
    while (queens) {
        int sq = __builtin_ctzll(queens);
        queens &= queens - 1;
        Bitboard attacks = queen_attacks(sq, b->occupied) & king_zone;
        if (attacks) {
            attack_count += __builtin_popcountll(attacks);
            attack_weight += 5;
        }
    }

    if (attack_count >= 2) {
        score = attack_weight * attack_count;
    }

    Bitboard pawn_shield = 0;
    int kfile = sq_file(ksq);
    int krank = sq_rank(ksq);

    for (int f = (kfile > 0 ? kfile - 1 : 0); f <= (kfile < 7 ? kfile + 1 : 7); f++) {
        if (enemy == WHITE) {
            if (krank < 7) pawn_shield |= BIT(make_sq(f, krank + 1));
        } else {
            if (krank > 0) pawn_shield |= BIT(make_sq(f, krank - 1));
        }
    }

    Bitboard shield_pawns = pawn_shield & b->piece_bb[PAWN] & b->color_bb[enemy];
    int shield_count = __builtin_popcountll(shield_pawns);
    score += shield_count * 10;

    return score;
}

int eval_evaluate(const Board *b) {
    int score = 0;

    for (int c = 0; c < COLOR_NB; c++) {
        int sign = c == WHITE ? 1 : -1;
        for (int pt = PAWN; pt <= KING; pt++) {
            Bitboard bb = b->piece_bb[pt] & b->color_bb[c];
            while (bb) {
                int sq = __builtin_ctzll(bb);
                bb &= bb - 1;
                int idx = c == WHITE ? sq : sq_flip(sq);
                score += sign * (piece_values[pt] + pst[pt][idx]);
            }
        }

        score += sign * eval_mobility(b, c);
        score += sign * eval_passed_pawns(b, c);
        score += sign * eval_bishop_pair(b, c);
        score += sign * eval_rook_open_file(b, c);
        score += sign * eval_king_safety(b, c);
    }

    return b->side == WHITE ? score : -score;
}
