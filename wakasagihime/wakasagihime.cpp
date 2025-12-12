// Wakasagihime
// Plays Chinese Dark Chess (Banqi)!

#include "lib/chess.h"
#include "lib/marisa.h"
#include "lib/types.h"
#include "lib/helper.h"

// Girls are preparing...
__attribute__((constructor)) void prepare()
{
    // Prepare the distance table
    for (Square i = SQ_A1; i < SQUARE_NB; i += 1) {
        for (Square j = SQ_A1; j < SQUARE_NB; j += 1) {
            SquareDistance[i][j] = distance<Rank>(i, j) + distance<File>(i, j);
        }
    }

    // Prepare the attack table (regular)
    Direction dirs[4] = { NORTH, SOUTH, EAST, WEST };
    for (Square sq = SQ_A1; is_okay(sq); sq += 1) {
        Board a = 0;
        for (Direction d : dirs) {
            a |= safe_destination(sq, d);
        }
        PseudoAttacks[sq] = a;
    }

    // Prepare magic
    init_magic<Cannon>(cannonTable, cannonMagics);
}

// le fishe
int main()
{
    /*
     * This is a simple Monte Carlo agent, it does
     *     - move generation
     *     - simulation
     *
     * To make it good MCTS, you still need:
     *     - a tree
     *     - Some UCB math
     *     - other enhancements
     *
     * You SHOULD create new files instead of cramming everything in this one,
     * it MAY affect your readability score.
     */
    std::string line;
    /* read input board state */
    while (std::getline(std::cin, line)) {
        Position pos(line);
        MoveList moves(pos);

        // NEW
        if (pos.time_left() < -1.0) {
            continue;
        }

        int min_score = 100;
        int chosen = 0;

        for (int i = 0; i < moves.size(); i += 1) {
            Position copy(pos);
            copy.do_move(moves[i]);
            int local_score = 0;
            for (int j = 0; j < 20; j += 1) {
                /* Run some (20) simulations. */
                local_score += copy.simulate(strategy_random);
            }
            /*
             * The simulations started from the opponent's perspective,
             * so we choose the move that led to the MINIMUM score here.
             */
            if (local_score < min_score) {
                chosen = i;
                min_score = local_score;
            }
            debug << "This is a debug message " << local_score << "\n";
            std::fflush(stderr);
        }
        /* output the move */
        info << moves[chosen];
    }
}
