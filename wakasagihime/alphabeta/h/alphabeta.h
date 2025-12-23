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
#include <cstring>

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

const double INF = 1e6;
const int AB_WIN_SCORE = 20000;
const int FORCE_WIN_THRESHOLD = AB_WIN_SCORE / 2;
const double WIN = 1.0;
const double LOSS = 0.0;
constexpr int MAT_SIZE = 2916;
const double SCALING_FACTOR = 200.0;
const int MAX_NO_EAT_FLIP = 20;
const double MAX_TIME_MS = 15000.0;
const double MIN_TIME_MS = 100.0;

const int EXPECTED_PLYS = 80;
const int EXPECTED_PLY_LONG = 120;

class AlphaBetaEngine{
public:
    AlphaBetaEngine();
    Move search(Position &pos);

    static int material_table[MAT_SIZE][MAT_SIZE]; 
    static bool table_loaded;
    void update_unrevealed(const Position &pos);
    void init_game();
private:
    struct ScoredMove{
        Move mv;
        int score;
    };
    std::vector<ScoredMove> get_ordered_moves(const Position &pos, Move tt_move);
    int history_table_[SQUARE_NB][SQUARE_NB];
    void age_history_table();

    double star0(const Move &mv, const Position &pos, double alpha, double beta, int depth, uint64_t key);
    double f3(Position &pos, double alpha, double beta, int depth, uint64_t key);
    double eval(const Position &pos, const int depth);
    double pos_score(const Position &pos, Color cur_color);

    double estimatePlyTime(const Position& pos);

    bool time_out_ = false;
    int node_count_ = 0;
    int no_eat_flip = 0;
    int ply_count_ = 0;// total ply count in the game
    std::chrono::time_point<std::chrono::steady_clock> start_time_{};
    int time_limit_ms_ = 4500;
    TranspositionTable tt_;
    ZobristHash zobrist_;

    void load_material_table();
    int get_material_index(const Position &pos, Color cur_color) const;
    double try_move(const Position &pos, const Move &mv, double alpha, double beta, int depth, uint64_t key);

    const int init_counts[7] = {1, 2, 2, 2, 2, 2, 5};
    int unrevealed_count[2][7];// need track both!
};

#endif