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

static const int mobility_bonus[PIECE_TYPE_NB] = { 0, 0, 3, 4, 2, 1, 0 };

static const int passed_pawn_bonus[8] = { 0, 5, 12, 20, 35, 60, 100, 0 };

void eval_init(void) {
}

static int eval_pawn_structure(const Board *b, int color) {
    int score = 0;
    Bitboard pawns = b->piece_bb[PAWN] & b->color_bb[color];

    while (pawns) {
        int sq = __builtin_ctzll(pawns);
        pawns &= pawns - 1;

        int file = sq_file(sq);
        int rank = sq_rank(sq);

        Bitboard file_mask = 0;
        for (int r = 0; r < 8; r++)
            file_mask |= BIT(make_sq(file, r));

        int own_on_file = __builtin_popcountll(file_mask & b->piece_bb[PAWN] & b->color_bb[color]);
        if (own_on_file > 1)
            score -= 10;

        Bitboard adj_files = 0;
        if (file > 0) {
            for (int r = 0; r < 8; r++)
                adj_files |= BIT(make_sq(file - 1, r));
        }
        if (file < 7) {
            for (int r = 0; r < 8; r++)
                adj_files |= BIT(make_sq(file + 1, r));
        }

        if (!(adj_files & b->piece_bb[PAWN] & b->color_bb[color]))
            score -= 15;

        Bitboard front_mask = 0;
        if (color == WHITE) {
            for (int r = rank + 1; r < 8; r++) {
                if (file > 0) front_mask |= BIT(make_sq(file - 1, r));
                front_mask |= BIT(make_sq(file, r));
                if (file < 7) front_mask |= BIT(make_sq(file + 1, r));
            }
        } else {
            for (int r = rank - 1; r >= 0; r--) {
                if (file > 0) front_mask |= BIT(make_sq(file - 1, r));
                front_mask |= BIT(make_sq(file, r));
                if (file < 7) front_mask |= BIT(make_sq(file + 1, r));
            }
        }

        if (!(front_mask & b->piece_bb[PAWN] & b->color_bb[color])) {
            Bitboard support = 0;
            if (color == WHITE) {
                if (rank > 0) {
                    if (file > 0) support |= BIT(make_sq(file - 1, rank - 1));
                    if (file < 7) support |= BIT(make_sq(file + 1, rank - 1));
                }
            } else {
                if (rank < 7) {
                    if (file > 0) support |= BIT(make_sq(file - 1, rank + 1));
                    if (file < 7) support |= BIT(make_sq(file + 1, rank + 1));
                }
            }
            if (!(support & b->piece_bb[PAWN] & b->color_bb[color]))
                score -= 5;
        }
    }

    return score;
}

static int eval_knight_outpost(const Board *b, int color) {
    int score = 0;
    Bitboard knights = b->piece_bb[KNIGHT] & b->color_bb[color];
    Bitboard enemy_pawns = b->piece_bb[PAWN] & b->color_bb[color ^ 1];

    while (knights) {
        int sq = __builtin_ctzll(knights);
        knights &= knights - 1;

        int file = sq_file(sq);
        int rank = sq_rank(sq);
        int rel_rank = color == WHITE ? rank : 7 - rank;

        if (rel_rank >= 3 && rel_rank <= 6 && file >= 2 && file <= 5) {
            Bitboard attack_mask = 0;
            if (color == WHITE) {
                for (int r = rank + 1; r < 8; r++) {
                    if (file > 0) attack_mask |= BIT(make_sq(file - 1, r));
                    attack_mask |= BIT(make_sq(file, r));
                    if (file < 7) attack_mask |= BIT(make_sq(file + 1, r));
                }
            } else {
                for (int r = rank - 1; r >= 0; r--) {
                    if (file > 0) attack_mask |= BIT(make_sq(file - 1, r));
                    attack_mask |= BIT(make_sq(file, r));
                    if (file < 7) attack_mask |= BIT(make_sq(file + 1, r));
                }
            }

            if (!(attack_mask & enemy_pawns)) {
                Bitboard support = pawn_attacks[color ^ 1][sq] & b->piece_bb[PAWN] & b->color_bb[color];
                if (support)
                    score += 15;
                else
                    score += 5;
            }
        }
    }

    return score;
}

static int eval_rook_seventh(const Board *b, int color) {
    int score = 0;
    Bitboard rooks = b->piece_bb[ROOK] & b->color_bb[color];
    int seventh_rank = color == WHITE ? 6 : 1;

    while (rooks) {
        int sq = __builtin_ctzll(rooks);
        rooks &= rooks - 1;

        if (sq_rank(sq) == seventh_rank) {
            int enemy_king_rank = sq_rank(board_king_square(b, color ^ 1));
            if ((color == WHITE && enemy_king_rank >= 6) || (color == BLACK && enemy_king_rank <= 1))
                score += 12;
            else
                score += 6;
        }
    }

    return score;
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

    int shelter_score = 0;
    for (int f = (kfile > 0 ? kfile - 1 : 0); f <= (kfile < 7 ? kfile + 1 : 7); f++) {
        int closest_rank = -1;
        for (int r = 0; r < 8; r++) {
            int sq = make_sq(f, r);
            if (b->squares[sq] != NO_PIECE && piece_type(b->squares[sq]) == PAWN && piece_color(b->squares[sq]) == enemy) {
                if (enemy == WHITE) {
                    if (closest_rank == -1 || r < closest_rank) closest_rank = r;
                } else {
                    if (closest_rank == -1 || r > closest_rank) closest_rank = r;
                }
            }
        }

        if (closest_rank != -1) {
            int dist = (enemy == WHITE) ? (closest_rank - krank) : (krank - closest_rank);
            if (dist == 1) shelter_score += 12;
            else if (dist == 2) shelter_score += 6;
            else if (dist == 3) shelter_score += 3;
        }
    }
    score += shelter_score;

    int storm_score = 0;
    for (int f = (kfile > 1 ? kfile - 2 : 0); f <= (kfile < 6 ? kfile + 2 : 7); f++) {
        int closest_rank = -1;
        for (int r = 0; r < 8; r++) {
            int sq = make_sq(f, r);
            if (b->squares[sq] != NO_PIECE && piece_type(b->squares[sq]) == PAWN && piece_color(b->squares[sq]) == color) {
                if (color == WHITE) {
                    if (closest_rank == -1 || r > closest_rank) closest_rank = r;
                } else {
                    if (closest_rank == -1 || r < closest_rank) closest_rank = r;
                }
            }
        }

        if (closest_rank != -1) {
            int dist_to_king = (color == WHITE) ? (krank - closest_rank) : (closest_rank - krank);
            if (dist_to_king <= 3 && dist_to_king > 0) {
                storm_score += 10 / dist_to_king;
            }
        }
    }
    score -= storm_score;

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
        score += sign * eval_pawn_structure(b, c);
        score += sign * eval_knight_outpost(b, c);
        score += sign * eval_rook_seventh(b, c);
    }

    return b->side == WHITE ? score : -score;
}
