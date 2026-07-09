#include "search.h"
#include "movegen.h"
#include "eval.h"
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

TTEntry tt_table[TT_SIZE];
int tt_age = 0;

static Move killer_moves[MAX_PLY][2];
static int history_table[COLOR_NB][SQUARE_NB][SQUARE_NB];
static Move pv_table[MAX_PLY][MAX_PLY];
static int pv_length[MAX_PLY];
static int lmr_table[MAX_PLY][MAX_MOVES];

static const int see_values[] = { 0, 100, 320, 330, 500, 900, 20000 };

int64_t get_time_ms(void) {
#ifdef _WIN32
    return (int64_t)GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

void search_init(void) {
    search_clear_tt();
    for (int d = 0; d < MAX_PLY; d++)
        for (int m = 0; m < MAX_MOVES; m++)
            lmr_table[d][m] = (d > 0 && m > 0) ? (int)(0.75 + log(d) * log(m) / 2.25) : 0;
}

void search_clear_tt(void) {
    memset(tt_table, 0, sizeof(tt_table));
    tt_age = 0;
}

static void tt_store(Key hash, Move best, int depth, int score, int flag) {
    int idx = (int)(hash & TT_MASK);
    TTEntry *entry = &tt_table[idx];

    if (entry->hash == hash || depth >= entry->depth || entry->age != tt_age) {
        entry->hash = hash;
        entry->best_move = best;
        entry->depth = depth;
        entry->score = score;
        entry->flag = flag;
        entry->age = tt_age;
    }
}

static TTEntry* tt_probe(Key hash, bool *found) {
    int idx = (int)(hash & TT_MASK);
    TTEntry *entry = &tt_table[idx];
    if (entry->hash == hash) {
        *found = true;
        return entry;
    }
    *found = false;
    return NULL;
}

static Bitboard attackers_to(const Board *b, int sq, Bitboard occupied) {
    Bitboard attackers = 0;
    attackers |= (pawn_attacks[BLACK][sq] & b->piece_bb[PAWN] & b->color_bb[WHITE]);
    attackers |= (pawn_attacks[WHITE][sq] & b->piece_bb[PAWN] & b->color_bb[BLACK]);
    attackers |= (knight_attacks[sq] & b->piece_bb[KNIGHT]);
    attackers |= (bishop_attacks(sq, occupied) & (b->piece_bb[BISHOP] | b->piece_bb[QUEEN]));
    attackers |= (rook_attacks(sq, occupied) & (b->piece_bb[ROOK] | b->piece_bb[QUEEN]));
    attackers |= (king_attacks[sq] & b->piece_bb[KING]);
    return attackers;
}

static int see(const Board *b, Move m) {
    int from = move_from(m);
    int to = move_to(m);
    int flags = move_flags(m);

    if (flags == MF_CASTLING) return 0;

    int gain[32];
    int d = 0;
    Bitboard occupied = b->occupied;
    Bitboard attackers;
    int from_piece = b->squares[from];
    int to_piece = b->squares[to];
    int a_piece = piece_type(from_piece);
    int side = piece_color(from_piece) ^ 1;

    gain[d] = (to_piece != NO_PIECE) ? see_values[piece_type(to_piece)] : 0;
    if (flags == MF_EP) {
        gain[d] = see_values[PAWN];
        int cap_sq = make_sq(sq_file(to), sq_rank(from));
        occupied ^= BIT(cap_sq);
    }
    if (flags == MF_PROMO) {
        a_piece = move_promo(m) + KNIGHT;
        gain[d] += see_values[a_piece] - see_values[PAWN];
    }

    occupied ^= BIT(from) | BIT(to);
    attackers = attackers_to(b, to, occupied) & occupied;

    while (true) {
        d++;
        gain[d] = see_values[a_piece] - gain[d - 1];
        if (-gain[d - 1] < 0) break;

        Bitboard side_attackers = attackers & b->color_bb[side];
        if (!side_attackers) break;

        int pt;
        for (pt = PAWN; pt <= KING; pt++) {
            Bitboard bb = side_attackers & b->piece_bb[pt];
            if (bb) {
                int sq = __builtin_ctzll(bb);
                occupied ^= BIT(sq);
                a_piece = pt;
                if (pt == PAWN || pt == BISHOP || pt == QUEEN)
                    attackers |= bishop_attacks(to, occupied) & (b->piece_bb[BISHOP] | b->piece_bb[QUEEN]);
                if (pt == ROOK || pt == QUEEN)
                    attackers |= rook_attacks(to, occupied) & (b->piece_bb[ROOK] | b->piece_bb[QUEEN]);
                attackers &= occupied;
                side ^= 1;
                break;
            }
        }
        if (pt > KING) break;
    }

    while (--d) {
        gain[d - 1] = -(-gain[d - 1] > gain[d] ? -gain[d - 1] : gain[d]);
    }

    return gain[0];
}

static int score_move(const Board *b, Move m) {
    int from = move_from(m);
    int to = move_to(m);
    int flags = move_flags(m);

    if (b->squares[to] != NO_PIECE || flags == MF_EP) {
        int see_val = see(b, m);
        if (see_val >= 0)
            return 100000 + see_val;
        else
            return 10000 + see_val;
    }
    if (flags == MF_PROMO) {
        return 90000 + move_promo(m) * 100;
    }
    if (killer_moves[0][0] == m) return 70000;
    if (killer_moves[0][1] == m) return 60000;
    return history_table[b->side][from][to];
}

static void sort_moves(MoveList *list, const Board *b, Move tt_move) {
    for (int i = 0; i < list->count; i++) {
        if (list->moves[i] == tt_move)
            list->scores[i] = 200000;
        else
            list->scores[i] = score_move(b, list->moves[i]);
    }
    for (int i = 1; i < list->count; i++) {
        Move m = list->moves[i];
        int s = list->scores[i];
        int j = i - 1;
        while (j >= 0 && list->scores[j] < s) {
            list->moves[j + 1] = list->moves[j];
            list->scores[j + 1] = list->scores[j];
            j--;
        }
        list->moves[j + 1] = m;
        list->scores[j + 1] = s;
    }
}

static int quiesce(Board *b, int alpha, int beta, SearchInfo *info, int ply) {
    info->nodes++;
    if (ply > info->sel_depth) info->sel_depth = ply;

    if (info->nodes % 4096 == 0) {
        int64_t elapsed = get_time_ms() - info->start_time;
        if (info->time_limit > 0 && elapsed >= info->time_limit) {
            info->stopped = true;
            return 0;
        }
    }

    int stand_pat = eval_evaluate(b);
    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;

    MoveList list;
    movegen_generate_captures(b, &list);

    for (int i = 0; i < list.count; i++) {
        int best_idx = i;
        for (int j = i + 1; j < list.count; j++) {
            if (score_move(b, list.moves[j]) > score_move(b, list.moves[best_idx]))
                best_idx = j;
        }
        Move tmp = list.moves[i]; list.moves[i] = list.moves[best_idx]; list.moves[best_idx] = tmp;

        Move m = list.moves[i];
        int to = move_to(m);
        int flags = move_flags(m);

        int captured_value = 0;
        if (b->squares[to] != NO_PIECE)
            captured_value = piece_values[piece_type(b->squares[to])];
        else if (flags == MF_EP)
            captured_value = piece_values[PAWN];

        if (flags == MF_PROMO)
            captured_value += piece_values[move_promo(m) + KNIGHT] - piece_values[PAWN];

        if (stand_pat + captured_value + 200 < alpha && see(b, m) <= 0)
            continue;

        board_make_move(b, m);
        int score = -quiesce(b, -beta, -alpha, info, ply + 1);
        board_unmake_move(b, m);

        if (info->stopped) return 0;

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

static int negamax(Board *b, int alpha, int beta, int depth, int ply, SearchInfo *info, bool do_null) {
    bool pv_node = (beta - alpha > 1);
    pv_length[ply] = ply;

    if (depth <= 0) {
        return quiesce(b, alpha, beta, info, ply);
    }

    info->nodes++;
    if (ply > info->sel_depth) info->sel_depth = ply;

    if (info->nodes % 4096 == 0) {
        int64_t elapsed = get_time_ms() - info->start_time;
        if (info->time_limit > 0 && elapsed >= info->time_limit) {
            info->stopped = true;
            return 0;
        }
    }

    if (ply > 0 && board_is_draw(b)) {
        return VALUE_DRAW;
    }

    bool tt_found;
    TTEntry *tt_entry = tt_probe(b->hash, &tt_found);
    Move tt_move = MOVE_NONE;

    if (tt_found) {
        tt_move = tt_entry->best_move;
        if (!pv_node && tt_entry->depth >= depth && ply > 0) {
            int tt_score = tt_entry->score;
            if (tt_entry->flag == HASH_EXACT) return tt_score;
            if (tt_entry->flag == HASH_ALPHA && tt_score <= alpha) return alpha;
            if (tt_entry->flag == HASH_BETA && tt_score >= beta) return beta;
        }
    }

    bool in_check = board_in_check(b);

    if (in_check) depth++;

    if (do_null && !in_check && depth >= 3 && ply > 0 && !pv_node && board_has_non_pawn_material(b, b->side)) {
        int R = 3 + depth / 4;
        board_make_null(b);
        int null_score = -negamax(b, -beta, -beta + 1, depth - R, ply + 1, info, false);
        board_unmake_null(b);
        if (info->stopped) return 0;
        if (null_score >= beta) return beta;
    }

    bool do_futility = false;
    int static_eval = 0;
    if (!in_check && !pv_node && depth <= 6) {
        static_eval = eval_evaluate(b);
        if (static_eval - 100 * depth >= beta)
            return static_eval;
        if (depth <= 4 && static_eval + 300 * depth < alpha)
            do_futility = true;
    }

    MoveList list;
    movegen_generate_legal(b, &list);

    if (list.count == 0) {
        if (in_check) return -MATE_SCORE + ply;
        return VALUE_DRAW;
    }

    sort_moves(&list, b, tt_move);

    int best_score = -INFINITY_SCORE;
    Move best_move = MOVE_NONE;
    int moves_searched = 0;
    int flag = HASH_ALPHA;

    int lmp_threshold = 3 + depth * depth;

    for (int i = 0; i < list.count; i++) {
        Move m = list.moves[i];
        bool is_capture = (b->squares[move_to(m)] != NO_PIECE);
        bool is_promo = (move_flags(m) == MF_PROMO);
        bool is_quiet = !is_capture && !is_promo;

        if (do_futility && is_quiet && moves_searched > 0)
            continue;

        if (is_quiet && !pv_node && !in_check && depth <= 4 && moves_searched >= lmp_threshold)
            continue;

        board_make_move(b, m);
        int score;

        if (moves_searched == 0) {
            score = -negamax(b, -beta, -alpha, depth - 1, ply + 1, info, true);
        } else {
            int new_depth = depth - 1;

            if (is_quiet && depth >= 3 && moves_searched >= 3 && !in_check && !pv_node) {
                int R = lmr_table[depth][moves_searched];
                if (R < 1) R = 1;
                if (new_depth - R < 1) R = new_depth - 1;
                if (R > 0) {
                    score = -negamax(b, -alpha - 1, -alpha, new_depth - R, ply + 1, info, true);
                } else {
                    score = alpha + 1;
                }
            } else {
                score = alpha + 1;
            }

            if (score > alpha) {
                score = -negamax(b, -alpha - 1, -alpha, new_depth, ply + 1, info, true);
            }
            if (score > alpha && score < beta) {
                score = -negamax(b, -beta, -alpha, new_depth, ply + 1, info, true);
            }
        }

        board_unmake_move(b, m);

        if (info->stopped) return 0;

        moves_searched++;

        if (score > best_score) {
            best_score = score;
            best_move = m;

            if (score > alpha) {
                alpha = score;
                flag = HASH_EXACT;

                pv_table[ply][ply] = m;
                for (int j = ply + 1; j < pv_length[ply + 1]; j++)
                    pv_table[ply][j] = pv_table[ply + 1][j];
                pv_length[ply] = pv_length[ply + 1];

                if (score >= beta) {
                    flag = HASH_BETA;
                    if (is_quiet) {
                        if (killer_moves[ply][0] != m) {
                            killer_moves[ply][1] = killer_moves[ply][0];
                            killer_moves[ply][0] = m;
                        }
                        history_table[b->side][move_from(m)][move_to(m)] += depth * depth;
                        if (history_table[b->side][move_from(m)][move_to(m)] > 1000000) {
                            for (int c = 0; c < COLOR_NB; c++)
                                for (int f = 0; f < SQUARE_NB; f++)
                                    for (int t = 0; t < SQUARE_NB; t++)
                                        history_table[c][f][t] /= 2;
                        }
                    }
                    break;
                }
            }
        }
    }

    tt_store(b->hash, best_move, depth, best_score, flag);

    return best_score;
}

void search_set_time(SearchInfo *info, int time_ms, int inc_ms) {
    info->time_limit = time_ms;
    info->time_inc = inc_ms;
}

Move search_best_move(Board *b, SearchInfo *info) {
    info->start_time = get_time_ms();
    info->nodes = 0;
    info->stopped = false;
    info->best_move = MOVE_NONE;
    info->sel_depth = 0;

    memset(killer_moves, 0, sizeof(killer_moves));
    memset(history_table, 0, sizeof(history_table));
    memset(pv_table, 0, sizeof(pv_table));
    memset(pv_length, 0, sizeof(pv_length));

    tt_age++;

    MoveList list;
    movegen_generate_legal(b, &list);

    if (list.count == 0) return MOVE_NONE;
    if (list.count == 1) return list.moves[0];

    Move best_move = list.moves[0];
    int prev_score = 0;

    for (int depth = 1; depth <= info->depth; depth++) {
        info->stopped = false;
        info->sel_depth = 0;

        int alpha = prev_score - 25;
        int beta = prev_score + 25;
        if (depth <= 3) {
            alpha = -INFINITY_SCORE;
            beta = INFINITY_SCORE;
        }

        int score = 0;
        int aspiration_fails = 0;

        while (true) {
            pv_length[0] = 0;
            sort_moves(&list, b, best_move);

            Move prev_best = best_move;
            best_move = list.moves[0];
            int moves_searched = 0;
            int local_alpha = alpha;

            for (int i = 0; i < list.count; i++) {
                Move m = list.moves[i];
                board_make_move(b, m);

                int s;
                if (moves_searched == 0) {
                    s = -negamax(b, -beta, -local_alpha, depth - 1, 1, info, true);
                } else {
                    s = -negamax(b, -local_alpha - 1, -local_alpha, depth - 1, 1, info, true);
                    if (s > local_alpha && s < beta) {
                        s = -negamax(b, -beta, -local_alpha, depth - 1, 1, info, true);
                    }
                }

                board_unmake_move(b, m);

                if (info->stopped) {
                    best_move = prev_best;
                    goto done;
                }

                moves_searched++;

                if (s > local_alpha) {
                    local_alpha = s;
                    best_move = m;

                    pv_table[0][0] = m;
                    for (int j = 1; j < pv_length[1]; j++)
                        pv_table[0][j] = pv_table[1][j];
                    pv_length[0] = pv_length[1];
                }
            }

            score = local_alpha;

            if (score <= alpha || score >= beta) {
                aspiration_fails++;
                if (aspiration_fails >= 4) {
                    alpha = -INFINITY_SCORE;
                    beta = INFINITY_SCORE;
                } else {
                    int delta = 25 * aspiration_fails * aspiration_fails;
                    if (score <= alpha) alpha = score - delta;
                    if (score >= beta) beta = score + delta;
                }
                continue;
            }

            break;
        }

        prev_score = score;

        int64_t elapsed = get_time_ms() - info->start_time;
        if (elapsed < 1) elapsed = 1;
        int64_t nps = info->nodes * 1000 / elapsed;

        char move_str[6];
        move_to_str(best_move, move_str);

        if (score > MATE_IN_MAX) {
            int mate_in = (MATE_SCORE - score + 1) / 2;
            printf("info depth %d seldepth %d score mate %d nodes %lld time %lld nps %lld pv",
                   depth, info->sel_depth, mate_in,
                   (long long)info->nodes, (long long)elapsed, (long long)nps);
        } else if (score < -MATE_IN_MAX) {
            int mate_in = -(MATE_SCORE + score + 1) / 2;
            printf("info depth %d seldepth %d score mate %d nodes %lld time %lld nps %lld pv",
                   depth, info->sel_depth, mate_in,
                   (long long)info->nodes, (long long)elapsed, (long long)nps);
        } else {
            printf("info depth %d seldepth %d score cp %d nodes %lld time %lld nps %lld pv",
                   depth, info->sel_depth, score,
                   (long long)info->nodes, (long long)elapsed, (long long)nps);
        }

        for (int i = 0; i < pv_length[0]; i++) {
            char pv_str[6];
            move_to_str(pv_table[0][i], pv_str);
            printf(" %s", pv_str);
        }
        printf("\n");
        fflush(stdout);

        if (info->time_limit > 0) {
            int64_t time_used = get_time_ms() - info->start_time;
            if (time_used > info->time_limit * 3 / 4) {
                break;
            }
        }

        if (score > MATE_IN_MAX || score < -MATE_IN_MAX)
            break;
    }

done:
    info->best_move = best_move;
    return best_move;
}
