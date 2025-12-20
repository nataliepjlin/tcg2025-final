#ifndef ALPHABETA_H
#define ALPHABETA_H

#include <cmath>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>
#include "../../lib/helper.h"
#include "../../lib/chess.h"
#include "../../tt/h/transposition_table.h"
#include "../../tt/h/zobrist.h"
#include "../../lib/chess.h"
#include "../../lib/movegen.h"
#include "../../lib/helper.h"
#include <algorithm>
#include <fstream>
#include <chrono>
#include <unordered_map>
#include <vector>

const int Piece_Value[] = {
    810, // General
    270, // Advisor
    90,  // Elephant
    18,  // Chariot
    6,   // Horse
    18,  // Cannon
    1    // Soldier
};

const int yummy_table[7][8] = {
    // Victim:  Gen, Adv, Ele, Cha, Hor, Can, Sol | walk
    // ---------------------------------------------
    
    // general
    { 500, 300, 100,  30,  10,  30,   0, 10}, 

    // advisor
    {   0, 300, 100,  30,  10,  30,   5, 10},

    // elephant
    {   0,   0, 100,  30,  10,  30,   5, 12},

    // chariot
    {   0,   0,   0,  50,  20,  50,   5, 20},

    // horse
    {   0,   0,   0,   0,  20,  50,   5, 15},
    // cannon
    { 900, 300, 100,  50,  20,  50,   5, 20},

    // solider
    { 1000,  0,   0,   0,   0,   0,  10, 15} 
};
const int flip_score = 10;

const double INF = 2e9;
const int AB_WIN_SCORE = 20000;
const int FORCE_WIN_THRESHOLD = AB_WIN_SCORE / 2;
constexpr int MAT_SIZE = 2916;

class AlphaBetaEngine{
public:
    AlphaBetaEngine();
    Move search(Position &pos);

    static int material_table[MAT_SIZE][MAT_SIZE]; 
    static bool table_loaded;
    void update_unrevealed(const Position &pos);
private:
    int star0(const Move &mv, const Position &pos, int alpha, int beta, int depth, uint64_t key);
    int f3(Position &pos, int alpha, int beta, int depth, uint64_t key);
    int pos_score(const Position &pos, Color cur_color) const;

    bool time_out_ = false;
    int node_count_ = 0;
    std::chrono::time_point<std::chrono::steady_clock> start_time_{};
    static constexpr int kTimeLimitMs = 4500;
    TranspositionTable tt_;
    ZobristHash zobrist_;

    void load_material_table();
    int get_material_index(const Position &pos, Color cur_color) const;

    const int init_counts[7] = {1, 2, 2, 2, 2, 2, 5};
    int unrevealed_count[2][7];// need track both!
    
    int prev_depth_ = 1;
};

#endif