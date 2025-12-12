#ifndef TRANSPOSITION_TABLE_H
#define TRANSPOSITION_TABLE_H

#include <unordered_map>
#include <vector>
#include "zobrist.h"

enum TT_Flag{
    TT_EXACT,
    TT_ALPHA,// failed low, upper bound
    TT_BETA // failed high, lower bound
};

struct TT_Entry{
    uint64_t hash; // 8 bytes
    int depth; // 4 bytes
    long double score; // 16 bytes ?
    uint8_t flag; // 1 byte (uint8_t)
    Move best_move; // 2 bytes
    TT_Entry()
      : hash(0), depth(0), score(0.0), flag(TT_EXACT)
    {}
};


class TranspositionTable{
public:
    static constexpr size_t TT_SIZE = 1 << 20; 
    static constexpr size_t TT_MASK = TT_SIZE - 1;
    TranspositionTable() : table(TT_SIZE) {}

    bool probe(uint64_t hash, long double alpha, long double beta, const int depth, long double &ret_score, Move &tt_move);
    void store(uint64_t hash, const long double score, const int depth, const TT_Flag flag, const Move &best_move);

private:
    std::vector<TT_Entry> table;
};
#endif