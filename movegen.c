#include "movegen.h"
#include <string.h>

Bitboard knight_attacks[SQUARE_NB];
Bitboard king_attacks[SQUARE_NB];
Bitboard pawn_attacks[COLOR_NB][SQUARE_NB];

static Bitboard between_bb[SQUARE_NB][SQUARE_NB];
static Bitboard line_bb[SQUARE_NB][SQUARE_NB];

static const int bishop_dirs[] = { -9, -7, 7, 9 };
static const int rook_dirs[] = { -8, -1, 1, 8 };

static Bitboard ray_attack(int sq, int dir, Bitboard occupied) {
    Bitboard attacks = 0;
    int r = sq_rank(sq), f = sq_file(sq);
    int s = sq;
    while (1) {
        int nr = r, nf = f;
        if (dir == -9 || dir == -7 || dir == 7 || dir == 9) {
            if (dir == -9) { nr--; nf--; }
            else if (dir == -7) { nr--; nf++; }
            else if (dir == 7) { nr++; nf--; }
            else { nr++; nf++; }
        } else {
            if (dir == -8) nr--;
            else if (dir == 8) nr++;
            else if (dir == -1) nf--;
            else nf++;
        }
        if (nr < 0 || nr > 7 || nf < 0 || nf > 7) break;
        s = make_sq(nf, nr);
        attacks |= BIT(s);
        if (occupied & BIT(s)) break;
        r = nr;
        f = nf;
    }
    return attacks;
}

Bitboard bishop_attacks(int sq, Bitboard occupied) {
    Bitboard attacks = 0;
    for (int i = 0; i < 4; i++)
        attacks |= ray_attack(sq, bishop_dirs[i], occupied);
    return attacks;
}

Bitboard rook_attacks(int sq, Bitboard occupied) {
    Bitboard attacks = 0;
    for (int i = 0; i < 4; i++)
        attacks |= ray_attack(sq, rook_dirs[i], occupied);
    return attacks;
}

Bitboard queen_attacks(int sq, Bitboard occupied) {
    return bishop_attacks(sq, occupied) | rook_attacks(sq, occupied);
}

static void init_between(void) {
    memset(between_bb, 0, sizeof(between_bb));
    memset(line_bb, 0, sizeof(line_bb));

    for (int sq1 = 0; sq1 < 64; sq1++) {
        for (int dir_idx = 0; dir_idx < 4; dir_idx++) {
            int dir = rook_dirs[dir_idx];
            Bitboard ray = ray_attack(sq1, dir, 0);
            int s = sq1;
            int r = sq_rank(s), f = sq_file(s);
            while (1) {
                int nr = r, nf = f;
                if (dir == -8) nr--;
                else if (dir == 8) nr++;
                else if (dir == -1) nf--;
                else nf++;
                if (nr < 0 || nr > 7 || nf < 0 || nf > 7) break;
                s = make_sq(nf, nr);
                Bitboard ray2 = ray_attack(s, dir, 0);
                between_bb[sq1][s] = ray & ray2;
                between_bb[s][sq1] = between_bb[sq1][s];
                line_bb[sq1][s] = (ray | BIT(sq1)) & (ray2 | BIT(s));
                line_bb[s][sq1] = line_bb[sq1][s];
                r = nr;
                f = nf;
            }
        }
        for (int dir_idx = 0; dir_idx < 4; dir_idx++) {
            int dir = bishop_dirs[dir_idx];
            Bitboard ray = ray_attack(sq1, dir, 0);
            int s = sq1;
            int r = sq_rank(s), f = sq_file(s);
            while (1) {
                int nr = r, nf = f;
                if (dir == -9) { nr--; nf--; }
                else if (dir == -7) { nr--; nf++; }
                else if (dir == 7) { nr++; nf--; }
                else { nr++; nf++; }
                if (nr < 0 || nr > 7 || nf < 0 || nf > 7) break;
                s = make_sq(nf, nr);
                Bitboard ray2 = ray_attack(s, dir, 0);
                between_bb[sq1][s] = ray & ray2;
                between_bb[s][sq1] = between_bb[sq1][s];
                line_bb[sq1][s] = (ray | BIT(sq1)) & (ray2 | BIT(s));
                line_bb[s][sq1] = line_bb[sq1][s];
                r = nr;
                f = nf;
            }
        }
    }
}

void movegen_init(void) {
    memset(knight_attacks, 0, sizeof(knight_attacks));
    memset(king_attacks, 0, sizeof(king_attacks));
    memset(pawn_attacks, 0, sizeof(pawn_attacks));

    for (int sq = 0; sq < 64; sq++) {
        int r = sq_rank(sq), f = sq_file(sq);

        const int kn_dr[] = { -2, -2, -1, -1, 1, 1, 2, 2 };
        const int kn_df[] = { -1, 1, -2, 2, -2, 2, -1, 1 };
        for (int i = 0; i < 8; i++) {
            int nr = r + kn_dr[i], nf = f + kn_df[i];
            if (nr >= 0 && nr <= 7 && nf >= 0 && nf <= 7)
                knight_attacks[sq] |= BIT(make_sq(nf, nr));
        }

        const int ki_dr[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
        const int ki_df[] = { -1, 0, 1, -1, 1, -1, 0, 1 };
        for (int i = 0; i < 8; i++) {
            int nr = r + ki_dr[i], nf = f + ki_df[i];
            if (nr >= 0 && nr <= 7 && nf >= 0 && nf <= 7)
                king_attacks[sq] |= BIT(make_sq(nf, nr));
        }

        if (r < 7) {
            if (f > 0) pawn_attacks[WHITE][sq] |= BIT(make_sq(f - 1, r + 1));
            if (f < 7) pawn_attacks[WHITE][sq] |= BIT(make_sq(f + 1, r + 1));
        }
        if (r > 0) {
            if (f > 0) pawn_attacks[BLACK][sq] |= BIT(make_sq(f - 1, r - 1));
            if (f < 7) pawn_attacks[BLACK][sq] |= BIT(make_sq(f + 1, r - 1));
        }
    }

    init_between();
}

static void add_move(MoveList *list, int from, int to, int flags, int promo) {
    if (list->count < MAX_MOVES) {
        list->moves[list->count++] = make_move(from, to, flags, promo);
    }
}

static void generate_pawn_moves(const Board *b, MoveList *list, int us) {
    int them = us ^ 1;
    int push_dir = us == WHITE ? 8 : -8;
    Bitboard pawns = b->piece_bb[PAWN] & b->color_bb[us];
    Bitboard enemies = b->color_bb[them];
    Bitboard rank7 = us == WHITE ? 0x00FF000000000000ULL : 0x000000000000FF00ULL;

    Bitboard promoting = pawns & rank7;
    Bitboard non_promoting = pawns & ~rank7;

    while (non_promoting) {
        int from = __builtin_ctzll(non_promoting);
        non_promoting &= non_promoting - 1;

        int to = from + push_dir;
        if (!(BIT(to) & b->occupied)) {
            add_move(list, from, to, MF_NORMAL, 0);

            if ((us == WHITE && sq_rank(from) == RANK_2) || (us == BLACK && sq_rank(from) == RANK_7)) {
                int to2 = from + 2 * push_dir;
                if (!(BIT(to2) & b->occupied))
                    add_move(list, from, to2, MF_NORMAL, 0);
            }
        }

        Bitboard attacks = pawn_attacks[us][from] & enemies;
        while (attacks) {
            to = __builtin_ctzll(attacks);
            attacks &= attacks - 1;
            add_move(list, from, to, MF_NORMAL, 0);
        }
    }

    while (promoting) {
        int from = __builtin_ctzll(promoting);
        promoting &= promoting - 1;

        int to = from + push_dir;
        if (!(BIT(to) & b->occupied)) {
            for (int p = 0; p < 4; p++)
                add_move(list, from, to, MF_PROMO, p);
        }

        Bitboard attacks = pawn_attacks[us][from] & enemies;
        while (attacks) {
            to = __builtin_ctzll(attacks);
            attacks &= attacks - 1;
            for (int p = 0; p < 4; p++)
                add_move(list, from, to, MF_PROMO, p);
        }
    }

    if (b->ep_square != SQ_NONE) {
        Bitboard ep_pawns = pawns & pawn_attacks[them][b->ep_square];
        while (ep_pawns) {
            int from = __builtin_ctzll(ep_pawns);
            ep_pawns &= ep_pawns - 1;
            add_move(list, from, b->ep_square, MF_EP, 0);
        }
    }
}

static void generate_knight_moves(const Board *b, MoveList *list, int us) {
    Bitboard knights = b->piece_bb[KNIGHT] & b->color_bb[us];
    Bitboard own = b->color_bb[us];

    while (knights) {
        int from = __builtin_ctzll(knights);
        knights &= knights - 1;
        Bitboard attacks = knight_attacks[from] & ~own;
        while (attacks) {
            int to = __builtin_ctzll(attacks);
            attacks &= attacks - 1;
            add_move(list, from, to, MF_NORMAL, 0);
        }
    }
}

static void generate_bishop_moves(const Board *b, MoveList *list, int us) {
    Bitboard bishops = b->piece_bb[BISHOP] & b->color_bb[us];
    Bitboard own = b->color_bb[us];

    while (bishops) {
        int from = __builtin_ctzll(bishops);
        bishops &= bishops - 1;
        Bitboard attacks = bishop_attacks(from, b->occupied) & ~own;
        while (attacks) {
            int to = __builtin_ctzll(attacks);
            attacks &= attacks - 1;
            add_move(list, from, to, MF_NORMAL, 0);
        }
    }
}

static void generate_rook_moves(const Board *b, MoveList *list, int us) {
    Bitboard rooks = b->piece_bb[ROOK] & b->color_bb[us];
    Bitboard own = b->color_bb[us];

    while (rooks) {
        int from = __builtin_ctzll(rooks);
        rooks &= rooks - 1;
        Bitboard attacks = rook_attacks(from, b->occupied) & ~own;
        while (attacks) {
            int to = __builtin_ctzll(attacks);
            attacks &= attacks - 1;
            add_move(list, from, to, MF_NORMAL, 0);
        }
    }
}

static void generate_queen_moves(const Board *b, MoveList *list, int us) {
    Bitboard queens = b->piece_bb[QUEEN] & b->color_bb[us];
    Bitboard own = b->color_bb[us];

    while (queens) {
        int from = __builtin_ctzll(queens);
        queens &= queens - 1;
        Bitboard attacks = queen_attacks(from, b->occupied) & ~own;
        while (attacks) {
            int to = __builtin_ctzll(attacks);
            attacks &= attacks - 1;
            add_move(list, from, to, MF_NORMAL, 0);
        }
    }
}

static void generate_king_moves(const Board *b, MoveList *list, int us) {
    int ksq = board_king_square(b, us);
    if (ksq == SQ_NONE) return;
    Bitboard own = b->color_bb[us];
    Bitboard attacks = king_attacks[ksq] & ~own;

    while (attacks) {
        int to = __builtin_ctzll(attacks);
        attacks &= attacks - 1;
        add_move(list, ksq, to, MF_NORMAL, 0);
    }

    int them = us ^ 1;
    if (us == WHITE) {
        if ((b->castling & WHITE_OO) &&
            !(b->occupied & (BIT(SQ_F1) | BIT(SQ_G1))) &&
            !board_is_attacked(b, SQ_E1, them) &&
            !board_is_attacked(b, SQ_F1, them) &&
            !board_is_attacked(b, SQ_G1, them))
            add_move(list, SQ_E1, SQ_G1, MF_CASTLING, 0);
        if ((b->castling & WHITE_OOO) &&
            !(b->occupied & (BIT(SQ_D1) | BIT(SQ_C1) | BIT(SQ_B1))) &&
            !board_is_attacked(b, SQ_E1, them) &&
            !board_is_attacked(b, SQ_D1, them) &&
            !board_is_attacked(b, SQ_C1, them))
            add_move(list, SQ_E1, SQ_C1, MF_CASTLING, 0);
    } else {
        if ((b->castling & BLACK_OO) &&
            !(b->occupied & (BIT(SQ_F8) | BIT(SQ_G8))) &&
            !board_is_attacked(b, SQ_E8, them) &&
            !board_is_attacked(b, SQ_F8, them) &&
            !board_is_attacked(b, SQ_G8, them))
            add_move(list, SQ_E8, SQ_G8, MF_CASTLING, 0);
        if ((b->castling & BLACK_OOO) &&
            !(b->occupied & (BIT(SQ_D8) | BIT(SQ_C8) | BIT(SQ_B8))) &&
            !board_is_attacked(b, SQ_E8, them) &&
            !board_is_attacked(b, SQ_D8, them) &&
            !board_is_attacked(b, SQ_C8, them))
            add_move(list, SQ_E8, SQ_C8, MF_CASTLING, 0);
    }
}

static bool is_square_attacked_occ_cap(const Board *b, int sq, int by_color, Bitboard occupied, int cap_sq) {
    Bitboard enemy_pawns = b->piece_bb[PAWN] & b->color_bb[by_color];
    Bitboard enemy_knights = b->piece_bb[KNIGHT] & b->color_bb[by_color];
    Bitboard enemy_bishops = (b->piece_bb[BISHOP] | b->piece_bb[QUEEN]) & b->color_bb[by_color];
    Bitboard enemy_rooks = (b->piece_bb[ROOK] | b->piece_bb[QUEEN]) & b->color_bb[by_color];
    Bitboard enemy_kings = b->piece_bb[KING] & b->color_bb[by_color];

    if (cap_sq != SQ_NONE) {
        Bitboard mask = ~BIT(cap_sq);
        enemy_pawns &= mask;
        enemy_knights &= mask;
        enemy_bishops &= mask;
        enemy_rooks &= mask;
        enemy_kings &= mask;
    }

    if (pawn_attacks[by_color ^ 1][sq] & enemy_pawns)
        return true;
    if (knight_attacks[sq] & enemy_knights)
        return true;
    if (king_attacks[sq] & enemy_kings)
        return true;
    if (bishop_attacks(sq, occupied) & enemy_bishops)
        return true;
    if (rook_attacks(sq, occupied) & enemy_rooks)
        return true;
    return false;
}

static bool is_legal(Board *b, Move m) {
    int us = b->side;
    int ksq = board_king_square(b, us);
    int from = move_from(m);
    int to = move_to(m);
    int flags = move_flags(m);
    int them = us ^ 1;

    if (flags == MF_EP) {
        int cap_sq = make_sq(sq_file(to), sq_rank(from));
        Bitboard occupied = (b->occupied ^ BIT(from) ^ BIT(cap_sq)) | BIT(to);
        return !is_square_attacked_occ_cap(b, ksq, them, occupied, cap_sq);
    }

    if (piece_type(b->squares[from]) == KING) {
        if (flags == MF_CASTLING) return true;
        Bitboard occupied = b->occupied ^ BIT(from);
        int cap_sq = (b->squares[to] != NO_PIECE) ? to : SQ_NONE;
        if (cap_sq != SQ_NONE)
            occupied ^= BIT(to);
        occupied |= BIT(to);
        return !is_square_attacked_occ_cap(b, to, them, occupied, cap_sq);
    }

    Bitboard occupied = b->occupied;
    int cap_sq = SQ_NONE;
    if (b->squares[to] != NO_PIECE) {
        occupied ^= BIT(to);
        cap_sq = to;
    }
    occupied ^= BIT(from) | BIT(to);

    return !is_square_attacked_occ_cap(b, ksq, them, occupied, cap_sq);
}

void movegen_generate_legal(const Board *b, MoveList *list) {
    list->count = 0;
    MoveList pseudo;
    pseudo.count = 0;

    int us = b->side;
    generate_pawn_moves(b, &pseudo, us);
    generate_knight_moves(b, &pseudo, us);
    generate_bishop_moves(b, &pseudo, us);
    generate_rook_moves(b, &pseudo, us);
    generate_queen_moves(b, &pseudo, us);
    generate_king_moves(b, &pseudo, us);

    Board temp = *b;
    for (int i = 0; i < pseudo.count; i++) {
        if (is_legal(&temp, pseudo.moves[i])) {
            list->moves[list->count++] = pseudo.moves[i];
        }
    }
}

static void generate_pawn_captures(const Board *b, MoveList *list, int us) {
    int them = us ^ 1;
    int push_dir = us == WHITE ? 8 : -8;
    Bitboard pawns = b->piece_bb[PAWN] & b->color_bb[us];
    Bitboard enemies = b->color_bb[them];
    Bitboard rank7 = us == WHITE ? 0x00FF000000000000ULL : 0x000000000000FF00ULL;

    Bitboard promoting = pawns & rank7;
    Bitboard non_promoting = pawns & ~rank7;

    while (non_promoting) {
        int from = __builtin_ctzll(non_promoting);
        non_promoting &= non_promoting - 1;
        Bitboard attacks = pawn_attacks[us][from] & enemies;
        while (attacks) {
            int to = __builtin_ctzll(attacks);
            attacks &= attacks - 1;
            add_move(list, from, to, MF_NORMAL, 0);
        }
    }

    while (promoting) {
        int from = __builtin_ctzll(promoting);
        promoting &= promoting - 1;
        int to = from + push_dir;
        if (!(BIT(to) & b->occupied)) {
            for (int p = 0; p < 4; p++)
                add_move(list, from, to, MF_PROMO, p);
        }
        Bitboard attacks = pawn_attacks[us][from] & enemies;
        while (attacks) {
            to = __builtin_ctzll(attacks);
            attacks &= attacks - 1;
            for (int p = 0; p < 4; p++)
                add_move(list, from, to, MF_PROMO, p);
        }
    }

    if (b->ep_square != SQ_NONE) {
        Bitboard ep_pawns = pawns & pawn_attacks[us ^ 1][b->ep_square];
        while (ep_pawns) {
            int from = __builtin_ctzll(ep_pawns);
            ep_pawns &= ep_pawns - 1;
            add_move(list, from, b->ep_square, MF_EP, 0);
        }
    }
}

static void generate_piece_captures(const Board *b, MoveList *list, int us, int pt) {
    Bitboard pieces = b->piece_bb[pt] & b->color_bb[us];
    Bitboard enemies = b->color_bb[us ^ 1];

    while (pieces) {
        int from = __builtin_ctzll(pieces);
        pieces &= pieces - 1;
        Bitboard attacks;
        switch (pt) {
            case KNIGHT: attacks = knight_attacks[from]; break;
            case BISHOP: attacks = bishop_attacks(from, b->occupied); break;
            case ROOK: attacks = rook_attacks(from, b->occupied); break;
            case QUEEN: attacks = queen_attacks(from, b->occupied); break;
            case KING: attacks = king_attacks[from]; break;
            default: attacks = 0; break;
        }
        attacks &= enemies;
        while (attacks) {
            int to = __builtin_ctzll(attacks);
            attacks &= attacks - 1;
            add_move(list, from, to, MF_NORMAL, 0);
        }
    }
}

void movegen_generate_captures(const Board *b, MoveList *list) {
    list->count = 0;
    MoveList pseudo;
    pseudo.count = 0;

    int us = b->side;
    generate_pawn_captures(b, &pseudo, us);
    generate_piece_captures(b, &pseudo, us, KNIGHT);
    generate_piece_captures(b, &pseudo, us, BISHOP);
    generate_piece_captures(b, &pseudo, us, ROOK);
    generate_piece_captures(b, &pseudo, us, QUEEN);
    generate_piece_captures(b, &pseudo, us, KING);

    Board temp = *b;
    for (int i = 0; i < pseudo.count; i++) {
        if (is_legal(&temp, pseudo.moves[i])) {
            list->moves[list->count++] = pseudo.moves[i];
        }
    }
}

int movegen_count_legal(const Board *b) {
    MoveList list;
    movegen_generate_legal(b, &list);
    return list.count;
}
