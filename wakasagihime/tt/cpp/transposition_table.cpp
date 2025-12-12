#include "../h/transposition_table.h"
#include <algorithm>

bool TranspositionTable::probe(uint64_t hash, long double alpha, long double beta, const int depth, long double &ret_score, Move &tt_move){
    size_t index = hash & TT_MASK; // hash % TT_SIZE
    TT_Entry &entry = table[index];
    if(entry.hash == hash){
        if(entry.depth >= depth){
            tt_move = entry.best_move;
            if(entry.flag == TT_EXACT){
                ret_score = entry.score;
                return true;
            }
            else if(entry.flag == TT_ALPHA){
                // failed low
                beta = std::min(beta, entry.score);
            }
            else if(entry.flag == TT_BETA){
                // failed high
                alpha = std::max(alpha, entry.score);
            }

            if(alpha >= beta){
                ret_score = entry.score;
                return true;
            }
        }
    }
    return false;
}

void TranspositionTable::store(uint64_t hash, const long double score, const int depth, const TT_Flag flag, const Move &best_move){
    size_t index = hash & TT_MASK; // hash % TT_SIZE
    TT_Entry &entry = table[index];
    if (entry.hash != hash || depth >= entry.depth) {
        entry.hash = hash;
        entry.score = score;
        entry.depth = depth;
        entry.flag = flag;
        entry.best_move = best_move;
    }
}