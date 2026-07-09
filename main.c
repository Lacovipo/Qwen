#include "types.h"
#include "board.h"
#include "movegen.h"
#include "eval.h"
#include "search.h"
#include "uci.h"

int main(void) {
    zobrist_init();
    movegen_init();
    eval_init();
    search_init();
    uci_loop();
    return 0;
}
