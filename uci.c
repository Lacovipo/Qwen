#include "uci.h"
#include "board.h"
#include "movegen.h"
#include "eval.h"
#include "search.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static Board board;
static SearchInfo search_info;

static void uci_command(void) {
    printf("id name %s %s\n", ENGINE_NAME, ENGINE_VERSION);
    printf("id author %s\n", ENGINE_AUTHOR);
    printf("uciok\n");
    fflush(stdout);
}

static void isready_command(void) {
    printf("readyok\n");
    fflush(stdout);
}

static void position_command(char *line) {
    char *ptr = line;

    while (*ptr && *ptr != ' ') ptr++;
    if (*ptr == ' ') ptr++;

    if (strncmp(ptr, "startpos", 8) == 0) {
        board_from_fen(&board, START_FEN);
        ptr += 8;
    } else if (strncmp(ptr, "fen", 3) == 0) {
        ptr += 3;
        while (*ptr == ' ') ptr++;
        board_from_fen(&board, ptr);
        while (*ptr && *ptr != '\n') ptr++;
    }

    while (*ptr && *ptr != ' ') ptr++;
    if (*ptr == ' ' && strncmp(ptr + 1, "moves", 5) == 0) {
        ptr += 6;
        while (*ptr == ' ') ptr++;

        while (*ptr && *ptr != '\n') {
            char move_str[6] = {0};
            int i = 0;
            while (*ptr && *ptr != ' ' && *ptr != '\n' && i < 5) {
                move_str[i++] = *ptr++;
            }
            move_str[i] = '\0';

            Move m = str_to_move(&board, move_str);
            if (m != MOVE_NONE) {
                board_make_move(&board, m);
            }

            while (*ptr == ' ') ptr++;
        }
    }
}

static void go_command(char *line) {
    search_info.depth = MAX_PLY;
    search_info.time_limit = 0;
    search_info.time_inc = 0;
    search_info.nodes_limit = 0;
    search_info.infinite = false;

    int wtime = 0, btime = 0, winc = 0, binc = 0, movestogo = 0, movetime = 0;

    char *ptr = line;
    while (*ptr) {
        if (strncmp(ptr, "wtime", 5) == 0) {
            ptr += 5;
            while (*ptr == ' ') ptr++;
            wtime = atoi(ptr);
        } else if (strncmp(ptr, "btime", 5) == 0) {
            ptr += 5;
            while (*ptr == ' ') ptr++;
            btime = atoi(ptr);
        } else if (strncmp(ptr, "winc", 4) == 0) {
            ptr += 4;
            while (*ptr == ' ') ptr++;
            winc = atoi(ptr);
        } else if (strncmp(ptr, "binc", 4) == 0) {
            ptr += 4;
            while (*ptr == ' ') ptr++;
            binc = atoi(ptr);
        } else if (strncmp(ptr, "movestogo", 9) == 0) {
            ptr += 9;
            while (*ptr == ' ') ptr++;
            movestogo = atoi(ptr);
        } else if (strncmp(ptr, "depth", 5) == 0) {
            ptr += 5;
            while (*ptr == ' ') ptr++;
            search_info.depth = atoi(ptr);
        } else if (strncmp(ptr, "nodes", 5) == 0) {
            ptr += 5;
            while (*ptr == ' ') ptr++;
            search_info.nodes_limit = atoll(ptr);
        } else if (strncmp(ptr, "infinite", 8) == 0) {
            search_info.infinite = true;
            search_info.time_limit = 0;
        } else if (strncmp(ptr, "movetime", 8) == 0) {
            ptr += 8;
            while (*ptr == ' ') ptr++;
            movetime = atoi(ptr);
        }
        while (*ptr && *ptr != ' ') ptr++;
        if (*ptr == ' ') ptr++;
    }

    if (movetime > 0) {
        search_info.time_limit = movetime - 10;
    } else if (wtime > 0 || btime > 0) {
        int time_left = (board.side == WHITE) ? wtime : btime;
        int inc = (board.side == WHITE) ? winc : binc;

        if (movestogo > 0) {
            search_info.time_limit = time_left / movestogo + inc / 2;
        } else {
            int moves_left = 30;
            if (board.fullmove > 40) moves_left = 20;
            if (board.fullmove > 60) moves_left = 15;
            search_info.time_limit = time_left / moves_left + inc * 3 / 4;
        }

        if (search_info.time_limit > time_left / 3)
            search_info.time_limit = time_left / 3;
        if (search_info.time_limit < 10)
            search_info.time_limit = 10;
    }

    Move best = search_best_move(&board, &search_info);

    char move_str[6];
    move_to_str(best, move_str);
    printf("bestmove %s\n", move_str);
    fflush(stdout);
}

static int64_t perft(Board *b, int depth) {
    if (depth == 0) return 1;

    MoveList list;
    movegen_generate_legal(b, &list);

    if (depth == 1) return list.count;

    int64_t nodes = 0;
    for (int i = 0; i < list.count; i++) {
        board_make_move(b, list.moves[i]);
        nodes += perft(b, depth - 1);
        board_unmake_move(b, list.moves[i]);
    }
    return nodes;
}

static int64_t perft_slow(Board *b, int depth) {
    if (depth == 0) return 1;

    MoveList pseudo;
    pseudo.count = 0;
    int us = b->side;

    Bitboard pawns = b->piece_bb[PAWN] & b->color_bb[us];
    Bitboard enemies = b->color_bb[us ^ 1];
    int push_dir = us == WHITE ? 8 : -8;
    Bitboard rank7 = us == WHITE ? 0x00FF000000000000ULL : 0x000000000000FF00ULL;
    Bitboard promoting = pawns & rank7;
    Bitboard non_promoting = pawns & ~rank7;

    while (non_promoting) {
        int from = __builtin_ctzll(non_promoting);
        non_promoting &= non_promoting - 1;
        int to = from + push_dir;
        if (!(BIT(to) & b->occupied)) {
            pseudo.moves[pseudo.count++] = make_move(from, to, MF_NORMAL, 0);
            if ((us == WHITE && sq_rank(from) == 1) || (us == BLACK && sq_rank(from) == 6)) {
                int to2 = from + 2 * push_dir;
                if (!(BIT(to2) & b->occupied))
                    pseudo.moves[pseudo.count++] = make_move(from, to2, MF_NORMAL, 0);
            }
        }
        Bitboard atks = pawn_attacks[us][from] & enemies;
        while (atks) {
            to = __builtin_ctzll(atks);
            atks &= atks - 1;
            pseudo.moves[pseudo.count++] = make_move(from, to, MF_NORMAL, 0);
        }
    }
    while (promoting) {
        int from = __builtin_ctzll(promoting);
        promoting &= promoting - 1;
        int to = from + push_dir;
        if (!(BIT(to) & b->occupied)) {
            for (int p = 0; p < 4; p++)
                pseudo.moves[pseudo.count++] = make_move(from, to, MF_PROMO, p);
        }
        Bitboard atks = pawn_attacks[us][from] & enemies;
        while (atks) {
            to = __builtin_ctzll(atks);
            atks &= atks - 1;
            for (int p = 0; p < 4; p++)
                pseudo.moves[pseudo.count++] = make_move(from, to, MF_PROMO, p);
        }
    }
    if (b->ep_square != SQ_NONE) {
        Bitboard ep_pawns = pawns & pawn_attacks[us ^ 1][b->ep_square];
        while (ep_pawns) {
            int from = __builtin_ctzll(ep_pawns);
            ep_pawns &= ep_pawns - 1;
            pseudo.moves[pseudo.count++] = make_move(from, b->ep_square, MF_EP, 0);
        }
    }

    Bitboard knights = b->piece_bb[KNIGHT] & b->color_bb[us];
    while (knights) {
        int from = __builtin_ctzll(knights);
        knights &= knights - 1;
        Bitboard atks = knight_attacks[from] & ~b->color_bb[us];
        while (atks) {
            int to = __builtin_ctzll(atks);
            atks &= atks - 1;
            pseudo.moves[pseudo.count++] = make_move(from, to, MF_NORMAL, 0);
        }
    }
    Bitboard bishops = b->piece_bb[BISHOP] & b->color_bb[us];
    while (bishops) {
        int from = __builtin_ctzll(bishops);
        bishops &= bishops - 1;
        Bitboard atks = bishop_attacks(from, b->occupied) & ~b->color_bb[us];
        while (atks) {
            int to = __builtin_ctzll(atks);
            atks &= atks - 1;
            pseudo.moves[pseudo.count++] = make_move(from, to, MF_NORMAL, 0);
        }
    }
    Bitboard rooks = b->piece_bb[ROOK] & b->color_bb[us];
    while (rooks) {
        int from = __builtin_ctzll(rooks);
        rooks &= rooks - 1;
        Bitboard atks = rook_attacks(from, b->occupied) & ~b->color_bb[us];
        while (atks) {
            int to = __builtin_ctzll(atks);
            atks &= atks - 1;
            pseudo.moves[pseudo.count++] = make_move(from, to, MF_NORMAL, 0);
        }
    }
    Bitboard queens = b->piece_bb[QUEEN] & b->color_bb[us];
    while (queens) {
        int from = __builtin_ctzll(queens);
        queens &= queens - 1;
        Bitboard atks = queen_attacks(from, b->occupied) & ~b->color_bb[us];
        while (atks) {
            int to = __builtin_ctzll(atks);
            atks &= atks - 1;
            pseudo.moves[pseudo.count++] = make_move(from, to, MF_NORMAL, 0);
        }
    }
    int ksq = board_king_square(b, us);
    if (ksq != SQ_NONE) {
        Bitboard atks = king_attacks[ksq] & ~b->color_bb[us];
        while (atks) {
            int to = __builtin_ctzll(atks);
            atks &= atks - 1;
            pseudo.moves[pseudo.count++] = make_move(ksq, to, MF_NORMAL, 0);
        }
        int them = us ^ 1;
        if (us == WHITE) {
            if ((b->castling & WHITE_OO) && !(b->occupied & (BIT(SQ_F1)|BIT(SQ_G1))) &&
                !board_is_attacked(b, SQ_E1, them) && !board_is_attacked(b, SQ_F1, them) && !board_is_attacked(b, SQ_G1, them))
                pseudo.moves[pseudo.count++] = make_move(SQ_E1, SQ_G1, MF_CASTLING, 0);
            if ((b->castling & WHITE_OOO) && !(b->occupied & (BIT(SQ_D1)|BIT(SQ_C1)|BIT(SQ_B1))) &&
                !board_is_attacked(b, SQ_E1, them) && !board_is_attacked(b, SQ_D1, them) && !board_is_attacked(b, SQ_C1, them))
                pseudo.moves[pseudo.count++] = make_move(SQ_E1, SQ_C1, MF_CASTLING, 0);
        } else {
            if ((b->castling & BLACK_OO) && !(b->occupied & (BIT(SQ_F8)|BIT(SQ_G8))) &&
                !board_is_attacked(b, SQ_E8, them) && !board_is_attacked(b, SQ_F8, them) && !board_is_attacked(b, SQ_G8, them))
                pseudo.moves[pseudo.count++] = make_move(SQ_E8, SQ_G8, MF_CASTLING, 0);
            if ((b->castling & BLACK_OOO) && !(b->occupied & (BIT(SQ_D8)|BIT(SQ_C8)|BIT(SQ_B8))) &&
                !board_is_attacked(b, SQ_E8, them) && !board_is_attacked(b, SQ_D8, them) && !board_is_attacked(b, SQ_C8, them))
                pseudo.moves[pseudo.count++] = make_move(SQ_E8, SQ_C8, MF_CASTLING, 0);
        }
    }

    int64_t nodes = 0;
    for (int i = 0; i < pseudo.count; i++) {
        board_make_move(b, pseudo.moves[i]);
        int our_king = board_king_square(b, us);
        bool in_check = (our_king != SQ_NONE) && board_is_attacked(b, our_king, us ^ 1);
        if (!in_check) {
            if (depth == 1)
                nodes++;
            else
                nodes += perft_slow(b, depth - 1);
        }
        board_unmake_move(b, pseudo.moves[i]);
    }
    return nodes;
}

static void perft_command(int depth) {
    MoveList list;
    int64_t total = 0;
    int64_t start = get_time_ms();

    movegen_generate_legal(&board, &list);

    for (int i = 0; i < list.count; i++) {
        board_make_move(&board, list.moves[i]);
        int64_t nodes = perft(&board, depth - 1);
        board_unmake_move(&board, list.moves[i]);

        char move_str[6];
        move_to_str(list.moves[i], move_str);
        printf("%s: %lld\n", move_str, (long long)nodes);
        total += nodes;
    }

    int64_t elapsed = get_time_ms() - start;
    if (elapsed < 1) elapsed = 1;
    printf("\nNodes searched: %lld\n", (long long)total);
    printf("Time: %lld ms\n", (long long)elapsed);
    printf("NPS: %lld\n", (long long)(total * 1000 / elapsed));
    fflush(stdout);
}

static void d_command(void) {
    board_print(&board);
    char fen[256];
    board_to_fen(&board, fen);
    printf("Fen: %s\n", fen);
    printf("Key: %016llx\n", (unsigned long long)board.hash);
    fflush(stdout);
}

void uci_loop(void) {
    char line[8192];

    board_from_fen(&board, START_FEN);
    memset(&search_info, 0, sizeof(search_info));
    search_info.depth = MAX_PLY;

    setbuf(stdin, NULL);
    setbuf(stdout, NULL);

    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\r\n")] = '\0';

        if (strncmp(line, "uci", 3) == 0) {
            uci_command();
        } else if (strncmp(line, "isready", 7) == 0) {
            isready_command();
        } else if (strncmp(line, "ucinewgame", 10) == 0) {
            search_clear_tt();
            board_from_fen(&board, START_FEN);
        } else if (strncmp(line, "position", 8) == 0) {
            position_command(line);
        } else if (strncmp(line, "go", 2) == 0) {
            go_command(line);
        } else if (strncmp(line, "perft", 5) == 0) {
            int depth = 1;
            char *ptr = line + 5;
            while (*ptr == ' ') ptr++;
            if (*ptr) depth = atoi(ptr);
            perft_command(depth);
        } else if (strncmp(line, "sperft", 6) == 0) {
            int depth = 1;
            char *ptr = line + 6;
            while (*ptr == ' ') ptr++;
            if (*ptr) depth = atoi(ptr);
            int64_t start = get_time_ms();

            MoveList list;
            movegen_generate_legal(&board, &list);
            int64_t total_fast = 0;
            int64_t total_slow = 0;
            for (int i = 0; i < list.count; i++) {
                board_make_move(&board, list.moves[i]);
                int64_t fast = perft(&board, depth - 1);
                int64_t slow = perft_slow(&board, depth - 1);
                board_unmake_move(&board, list.moves[i]);
                total_fast += fast;
                total_slow += slow;
                if (fast != slow) {
                    char ms[6];
                    move_to_str(list.moves[i], ms);
                    printf("MISMATCH %s: fast=%lld slow=%lld diff=%lld\n", ms, (long long)fast, (long long)slow, (long long)(fast - slow));
                }
            }
            int64_t elapsed = get_time_ms() - start;
            printf("Fast: %lld  Slow: %lld  Diff: %lld  Time: %lld ms\n",
                   (long long)total_fast, (long long)total_slow,
                   (long long)(total_fast - total_slow), (long long)elapsed);
            fflush(stdout);
        } else if (strncmp(line, "d", 1) == 0 && (line[1] == '\0' || line[1] == '\n')) {
            d_command();
        } else if (strncmp(line, "quit", 4) == 0) {
            break;
        } else if (strncmp(line, "stop", 4) == 0) {
            search_info.stopped = true;
        }
    }
}
