#include "board.h"
#include "movegen.h"
#include <ctype.h>

Key zobrist_pieces[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
Key zobrist_castling[16];
Key zobrist_ep[FILE_NB];
Key zobrist_side;

static uint64_t xorshift64(uint64_t *state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

void zobrist_init(void) {
    uint64_t seed = 0x12345678ABCDEF01ULL;
    for (int c = 0; c < COLOR_NB; c++)
        for (int pt = 0; pt < PIECE_TYPE_NB; pt++)
            for (int sq = 0; sq < SQUARE_NB; sq++)
                zobrist_pieces[c][pt][sq] = xorshift64(&seed);
    for (int i = 0; i < 16; i++)
        zobrist_castling[i] = xorshift64(&seed);
    for (int f = 0; f < FILE_NB; f++)
        zobrist_ep[f] = xorshift64(&seed);
    zobrist_side = xorshift64(&seed);
}

Key zobrist_compute(const Board *b) {
    Key key = 0;
    for (int c = 0; c < COLOR_NB; c++) {
        for (int pt = PAWN; pt <= KING; pt++) {
            Bitboard bb = b->piece_bb[pt] & b->color_bb[c];
            while (bb) {
                int sq = __builtin_ctzll(bb);
                key ^= zobrist_pieces[c][pt][sq];
                bb &= bb - 1;
            }
        }
    }
    key ^= zobrist_castling[b->castling];
    if (b->ep_square != SQ_NONE)
        key ^= zobrist_ep[sq_file(b->ep_square)];
    if (b->side == BLACK)
        key ^= zobrist_side;
    return key;
}

void board_init(Board *b) {
    memset(b, 0, sizeof(Board));
    b->ep_square = SQ_NONE;
    board_from_fen(b, START_FEN);
}

void board_from_fen(Board *b, const char *fen) {
    memset(b, 0, sizeof(Board));
    b->ep_square = SQ_NONE;
    b->hist_ply = 0;
    b->rep_count = 0;

    int sq = SQ_A8;
    const char *p = fen;

    while (*p && *p != ' ') {
        if (*p == '/') {
            sq -= 16;
        } else if (*p >= '1' && *p <= '8') {
            sq += (*p - '0');
        } else {
            int color = isupper(*p) ? WHITE : BLACK;
            int type = NONE;
            switch (tolower(*p)) {
                case 'p': type = PAWN; break;
                case 'n': type = KNIGHT; break;
                case 'b': type = BISHOP; break;
                case 'r': type = ROOK; break;
                case 'q': type = QUEEN; break;
                case 'k': type = KING; break;
            }
            if (type != NONE) {
                int piece = make_piece(color, type);
                b->piece_bb[type] |= BIT(sq);
                b->color_bb[color] |= BIT(sq);
                b->squares[sq] = piece;
            }
            sq++;
        }
        p++;
    }

    b->occupied = b->color_bb[WHITE] | b->color_bb[BLACK];

    while (*p == ' ') p++;
    b->side = (*p == 'b') ? BLACK : WHITE;
    while (*p && *p != ' ') p++;

    while (*p == ' ') p++;
    b->castling = NO_CASTLING;
    while (*p && *p != ' ') {
        switch (*p) {
            case 'K': b->castling |= WHITE_OO; break;
            case 'Q': b->castling |= WHITE_OOO; break;
            case 'k': b->castling |= BLACK_OO; break;
            case 'q': b->castling |= BLACK_OOO; break;
        }
        p++;
    }

    while (*p == ' ') p++;
    if (*p != '-') {
        int file = p[0] - 'a';
        int rank = p[1] - '1';
        b->ep_square = make_sq(file, rank);
        p += 2;
    } else {
        p++;
    }

    while (*p == ' ') p++;
    b->halfmove = 0;
    if (*p) {
        b->halfmove = atoi(p);
        while (*p && *p != ' ') p++;
    }

    while (*p == ' ') p++;
    b->fullmove = 1;
    if (*p) b->fullmove = atoi(p);

    b->hash = zobrist_compute(b);
    b->pawn_hash = 0;

    b->repetition_table[0] = b->hash;
    b->rep_count = 1;
}

void board_print(const Board *b) {
    const char piece_chars[] = ".PNBRQK  .pnbrqk";
    printf("\n");
    for (int rank = RANK_8; rank >= RANK_1; rank--) {
        printf("  %d ", rank + 1);
        for (int file = FILE_A; file <= FILE_H; file++) {
            int sq = make_sq(file, rank);
            int piece = b->squares[sq];
            if (piece == NO_PIECE)
                printf(". ");
            else
                printf("%c ", piece_chars[piece]);
        }
        printf("\n");
    }
    printf("    a b c d e f g h\n\n");
    printf("  Side: %s\n", b->side == WHITE ? "White" : "Black");
    printf("  Castling: %s%s%s%s\n",
           (b->castling & WHITE_OO) ? "K" : "",
           (b->castling & WHITE_OOO) ? "Q" : "",
           (b->castling & BLACK_OO) ? "k" : "",
           (b->castling & BLACK_OOO) ? "q" : "");
    printf("  EP: %d\n", b->ep_square);
    printf("  Halfmove: %d\n", b->halfmove);
    printf("  Hash: %016llx\n", (unsigned long long)b->hash);
}

void board_to_fen(const Board *b, char *fen) {
    char *p = fen;
    for (int rank = RANK_8; rank >= RANK_1; rank--) {
        int empty = 0;
        for (int file = FILE_A; file <= FILE_H; file++) {
            int sq = make_sq(file, rank);
            int piece = b->squares[sq];
            if (piece == NO_PIECE) {
                empty++;
            } else {
                if (empty > 0) { *p++ = '0' + empty; empty = 0; }
                const char chars[] = ".PNBRQK";
                char c = chars[piece_type(piece)];
                if (piece_color(piece) == BLACK) c = tolower(c);
                *p++ = c;
            }
        }
        if (empty > 0) *p++ = '0' + empty;
        if (rank > RANK_1) *p++ = '/';
    }
    *p++ = ' ';
    *p++ = b->side == WHITE ? 'w' : 'b';
    *p++ = ' ';
    if (b->castling == NO_CASTLING) {
        *p++ = '-';
    } else {
        if (b->castling & WHITE_OO) *p++ = 'K';
        if (b->castling & WHITE_OOO) *p++ = 'Q';
        if (b->castling & BLACK_OO) *p++ = 'k';
        if (b->castling & BLACK_OOO) *p++ = 'q';
    }
    *p++ = ' ';
    if (b->ep_square == SQ_NONE) {
        *p++ = '-';
    } else {
        *p++ = 'a' + sq_file(b->ep_square);
        *p++ = '1' + sq_rank(b->ep_square);
    }
    sprintf(p, " %d %d", b->halfmove, b->fullmove);
}

static const int castling_update[64] = {
    13, 15, 15, 15, 12, 15, 15, 14,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
     7, 15, 15, 15,  3, 15, 15, 11
};

void board_make_move(Board *b, Move m) {
    StateInfo *si = &b->history[b->hist_ply];
    si->castling = b->castling;
    si->ep_square = b->ep_square;
    si->halfmove = b->halfmove;
    si->hash = b->hash;
    si->pawn_hash = b->pawn_hash;
    si->rule50 = b->halfmove;
    b->hist_ply++;

    int from = move_from(m);
    int to = move_to(m);
    int flags = move_flags(m);
    int us = b->side;
    int them = us ^ 1;
    int piece = b->squares[from];
    int pt = piece_type(piece);
    int captured = b->squares[to];

    b->hash ^= zobrist_castling[b->castling];
    if (b->ep_square != SQ_NONE)
        b->hash ^= zobrist_ep[sq_file(b->ep_square)];

    b->halfmove++;
    if (pt == PAWN || captured != NO_PIECE)
        b->halfmove = 0;

    b->ep_square = SQ_NONE;

    if (flags == MF_CASTLING) {
        int rook_from, rook_to;
        if (to > from) {
            rook_from = to + 1;
            rook_to = to - 1;
        } else {
            rook_from = to - 2;
            rook_to = to + 1;
        }
        int rook = b->squares[rook_from];
        b->piece_bb[ROOK] ^= BIT(rook_from) | BIT(rook_to);
        b->color_bb[us] ^= BIT(rook_from) | BIT(rook_to);
        b->squares[rook_from] = NO_PIECE;
        b->squares[rook_to] = rook;
        b->hash ^= zobrist_pieces[us][ROOK][rook_from];
        b->hash ^= zobrist_pieces[us][ROOK][rook_to];

        b->piece_bb[KING] ^= BIT(from) | BIT(to);
        b->color_bb[us] ^= BIT(from) | BIT(to);
        b->squares[from] = NO_PIECE;
        b->squares[to] = piece;
        b->hash ^= zobrist_pieces[us][KING][from];
        b->hash ^= zobrist_pieces[us][KING][to];
    } else if (flags == MF_EP) {
        int cap_sq = make_sq(sq_file(to), sq_rank(from));
        si->captured_piece = captured = make_piece(them, PAWN);
        b->piece_bb[PAWN] ^= BIT(from) | BIT(to) | BIT(cap_sq);
        b->color_bb[us] ^= BIT(from) | BIT(to);
        b->color_bb[them] ^= BIT(cap_sq);
        b->squares[from] = NO_PIECE;
        b->squares[to] = piece;
        b->squares[cap_sq] = NO_PIECE;
        b->hash ^= zobrist_pieces[us][PAWN][from];
        b->hash ^= zobrist_pieces[us][PAWN][to];
        b->hash ^= zobrist_pieces[them][PAWN][cap_sq];
        b->halfmove = 0;
    } else if (flags == MF_PROMO) {
        int promo_type = move_promo(m) + KNIGHT;
        int promo_piece = make_piece(us, promo_type);
        si->captured_piece = captured;

        if (captured != NO_PIECE) {
            int cap_type = piece_type(captured);
            int cap_color = piece_color(captured);
            b->piece_bb[cap_type] ^= BIT(to);
            b->color_bb[cap_color] ^= BIT(to);
            b->hash ^= zobrist_pieces[cap_color][cap_type][to];
        }

        b->piece_bb[PAWN] ^= BIT(from);
        b->piece_bb[promo_type] |= BIT(to);
        b->color_bb[us] ^= BIT(from) | BIT(to);
        b->squares[from] = NO_PIECE;
        b->squares[to] = promo_piece;
        b->hash ^= zobrist_pieces[us][PAWN][from];
        b->hash ^= zobrist_pieces[us][promo_type][to];
    } else {
        si->captured_piece = captured;
        if (captured != NO_PIECE) {
            int cap_type = piece_type(captured);
            int cap_color = piece_color(captured);
            b->piece_bb[cap_type] ^= BIT(to);
            b->color_bb[cap_color] ^= BIT(to);
            b->hash ^= zobrist_pieces[cap_color][cap_type][to];
        }

        b->piece_bb[pt] ^= BIT(from) | BIT(to);
        b->color_bb[us] ^= BIT(from) | BIT(to);
        b->squares[from] = NO_PIECE;
        b->squares[to] = piece;
        b->hash ^= zobrist_pieces[us][pt][from];
        b->hash ^= zobrist_pieces[us][pt][to];

        if (pt == PAWN && abs(sq_rank(to) - sq_rank(from)) == 2) {
            b->ep_square = make_sq(sq_file(from), (sq_rank(from) + sq_rank(to)) / 2);
        }
    }

    b->occupied = b->color_bb[WHITE] | b->color_bb[BLACK];
    b->castling &= castling_update[from] & castling_update[to];
    b->hash ^= zobrist_castling[b->castling];
    if (b->ep_square != SQ_NONE)
        b->hash ^= zobrist_ep[sq_file(b->ep_square)];
    b->hash ^= zobrist_side;

    b->side = them;
    if (us == BLACK) b->fullmove++;

    b->repetition_table[b->rep_count] = b->hash;
    b->rep_count++;
}

void board_unmake_move(Board *b, Move m) {
    b->hist_ply--;
    b->rep_count--;
    StateInfo *si = &b->history[b->hist_ply];

    int from = move_from(m);
    int to = move_to(m);
    int flags = move_flags(m);
    int them = b->side;
    int us = them ^ 1;

    b->side = us;
    if (us == BLACK) b->fullmove--;

    if (flags == MF_CASTLING) {
        int rook_from, rook_to;
        if (to > from) {
            rook_from = to + 1;
            rook_to = to - 1;
        } else {
            rook_from = to - 2;
            rook_to = to + 1;
        }
        int rook = b->squares[rook_to];
        b->piece_bb[ROOK] ^= BIT(rook_from) | BIT(rook_to);
        b->color_bb[us] ^= BIT(rook_from) | BIT(rook_to);
        b->squares[rook_to] = NO_PIECE;
        b->squares[rook_from] = rook;

        int king = make_piece(us, KING);
        b->piece_bb[KING] ^= BIT(from) | BIT(to);
        b->color_bb[us] ^= BIT(from) | BIT(to);
        b->squares[to] = NO_PIECE;
        b->squares[from] = king;
    } else if (flags == MF_EP) {
        int cap_sq = make_sq(sq_file(to), sq_rank(from));
        int pawn = make_piece(us, PAWN);
        int cap_pawn = make_piece(them, PAWN);
        b->piece_bb[PAWN] ^= BIT(from) | BIT(to) | BIT(cap_sq);
        b->color_bb[us] ^= BIT(from) | BIT(to);
        b->color_bb[them] ^= BIT(cap_sq);
        b->squares[to] = NO_PIECE;
        b->squares[from] = pawn;
        b->squares[cap_sq] = cap_pawn;
    } else if (flags == MF_PROMO) {
        int promo_type = move_promo(m) + KNIGHT;
        int pawn = make_piece(us, PAWN);
        b->piece_bb[PAWN] |= BIT(from);
        b->piece_bb[promo_type] ^= BIT(to);
        b->color_bb[us] ^= BIT(from) | BIT(to);
        b->squares[to] = NO_PIECE;
        b->squares[from] = pawn;

        int captured = si->captured_piece;
        if (captured != NO_PIECE) {
            int cap_type = piece_type(captured);
            int cap_color = piece_color(captured);
            b->piece_bb[cap_type] |= BIT(to);
            b->color_bb[cap_color] |= BIT(to);
            b->squares[to] = captured;
        }
    } else {
        int piece = b->squares[to];
        int pt = piece_type(piece);
        b->piece_bb[pt] ^= BIT(from) | BIT(to);
        b->color_bb[us] ^= BIT(from) | BIT(to);
        b->squares[from] = piece;
        b->squares[to] = NO_PIECE;

        int captured = si->captured_piece;
        if (captured != NO_PIECE) {
            int cap_type = piece_type(captured);
            int cap_color = piece_color(captured);
            b->piece_bb[cap_type] |= BIT(to);
            b->color_bb[cap_color] |= BIT(to);
            b->squares[to] = captured;
        }
    }

    b->occupied = b->color_bb[WHITE] | b->color_bb[BLACK];
    b->castling = si->castling;
    b->ep_square = si->ep_square;
    b->halfmove = si->halfmove;
    b->hash = si->hash;
    b->pawn_hash = si->pawn_hash;
}

void board_make_null(Board *b) {
    StateInfo *si = &b->history[b->hist_ply];
    si->castling = b->castling;
    si->ep_square = b->ep_square;
    si->halfmove = b->halfmove;
    si->hash = b->hash;
    si->pawn_hash = b->pawn_hash;
    si->rule50 = b->halfmove;
    b->hist_ply++;

    if (b->ep_square != SQ_NONE) {
        b->hash ^= zobrist_ep[sq_file(b->ep_square)];
        b->ep_square = SQ_NONE;
    }
    b->hash ^= zobrist_side;
    b->side ^= 1;

    b->repetition_table[b->rep_count] = b->hash;
    b->rep_count++;
}

void board_unmake_null(Board *b) {
    b->hist_ply--;
    b->rep_count--;
    StateInfo *si = &b->history[b->hist_ply];

    b->side ^= 1;
    b->castling = si->castling;
    b->ep_square = si->ep_square;
    b->halfmove = si->halfmove;
    b->hash = si->hash;
    b->pawn_hash = si->pawn_hash;
}

bool board_is_attacked(const Board *b, int sq, int by_color) {
    if (pawn_attacks[by_color ^ 1][sq] & b->piece_bb[PAWN] & b->color_bb[by_color])
        return true;
    if (knight_attacks[sq] & b->piece_bb[KNIGHT] & b->color_bb[by_color])
        return true;
    if (king_attacks[sq] & b->piece_bb[KING] & b->color_bb[by_color])
        return true;
    if (bishop_attacks(sq, b->occupied) & (b->piece_bb[BISHOP] | b->piece_bb[QUEEN]) & b->color_bb[by_color])
        return true;
    if (rook_attacks(sq, b->occupied) & (b->piece_bb[ROOK] | b->piece_bb[QUEEN]) & b->color_bb[by_color])
        return true;
    return false;
}

int board_king_square(const Board *b, int color) {
    Bitboard king_bb = b->piece_bb[KING] & b->color_bb[color];
    if (!king_bb) return SQ_NONE;
    return __builtin_ctzll(king_bb);
}

bool board_in_check(const Board *b) {
    int ksq = board_king_square(b, b->side);
    if (ksq == SQ_NONE) return false;
    return board_is_attacked(b, ksq, b->side ^ 1);
}

bool board_is_repetition(const Board *b) {
    int start = b->rep_count - b->halfmove - 1;
    if (start < 0) start = 0;
    for (int i = b->rep_count - 3; i >= start; i -= 2) {
        if (b->repetition_table[i] == b->hash)
            return true;
    }
    return false;
}

bool board_is_draw(const Board *b) {
    if (b->halfmove >= 100)
        return true;
    if (board_is_repetition(b))
        return true;

    int w_non_pawn = __builtin_popcountll(b->color_bb[WHITE] & ~b->piece_bb[PAWN] & ~b->piece_bb[KING]);
    int b_non_pawn = __builtin_popcountll(b->color_bb[BLACK] & ~b->piece_bb[PAWN] & ~b->piece_bb[KING]);
    int w_pawns = __builtin_popcountll(b->color_bb[WHITE] & b->piece_bb[PAWN]);
    int b_pawns = __builtin_popcountll(b->color_bb[BLACK] & b->piece_bb[PAWN]);

    if (w_pawns == 0 && b_pawns == 0) {
        int w_total = __builtin_popcountll(b->color_bb[WHITE]);
        int b_total = __builtin_popcountll(b->color_bb[BLACK]);
        if (w_total <= 2 && b_total <= 2) {
            if (w_non_pawn <= 1 && b_non_pawn <= 1)
                return true;
        }
    }
    return false;
}

bool board_has_non_pawn_material(const Board *b, int color) {
    Bitboard non_pawn = b->color_bb[color] & ~(b->piece_bb[PAWN] | b->piece_bb[KING]);
    return non_pawn != 0;
}

void move_to_str(Move m, char *str) {
    if (m == MOVE_NONE) {
        strcpy(str, "0000");
        return;
    }
    int from = move_from(m);
    int to = move_to(m);
    str[0] = 'a' + sq_file(from);
    str[1] = '1' + sq_rank(from);
    str[2] = 'a' + sq_file(to);
    str[3] = '1' + sq_rank(to);
    if (move_flags(m) == MF_PROMO) {
        const char promo_chars[] = "nbrq";
        str[4] = promo_chars[move_promo(m)];
        str[5] = '\0';
    } else {
        str[4] = '\0';
    }
}

Move str_to_move(const Board *b, const char *str) {
    int from = (str[0] - 'a') + (str[1] - '1') * 8;
    int to = (str[2] - 'a') + (str[3] - '1') * 8;

    MoveList list;
    movegen_generate_legal(b, &list);

    for (int i = 0; i < list.count; i++) {
        Move m = list.moves[i];
        if (move_from(m) == from && move_to(m) == to) {
            if (move_flags(m) == MF_PROMO) {
                int promo = KNIGHT;
                if (str[4]) {
                    switch (str[4]) {
                        case 'n': promo = KNIGHT; break;
                        case 'b': promo = BISHOP; break;
                        case 'r': promo = ROOK; break;
                        case 'q': promo = QUEEN; break;
                    }
                }
                if (move_promo(m) == promo - KNIGHT)
                    return m;
            } else {
                return m;
            }
        }
    }
    return MOVE_NONE;
}
