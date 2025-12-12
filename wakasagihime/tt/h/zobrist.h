#ifndef ZOBRIST_H
#define ZOBRIST_H

#include <algorithm>
#include "../../lib/helper.h"
#include "../../lib/pcg-cpp-0.98/include/pcg_random.hpp"

class ZobristHash{
private:
    static constexpr int TOTAL_TYPE = 15;// 7*2 + 1 (hidden)
    static constexpr int HIDDEN_TYPE_INDEX = 14;
    static constexpr int BLACK_OFFSET = 7;
    uint64_t zob[TOTAL_TYPE][SQUARE_NB];// [piece_type][square], piece_type: 0-6 Red, 7-13 Black, 14 Hidden
    uint64_t sideToMove_hash;
    pcg64 rng64;
public:
    void init_zobrist();
    uint64_t compute_zobrist_hash(const Position &pos);
    uint64_t update_zobrist_hash(uint64_t hash, const Move &mv, const Position &pos, Piece flip_piece);
};

#endif  