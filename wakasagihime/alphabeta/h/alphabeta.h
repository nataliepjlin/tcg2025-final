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
    30, // General
    15, // Advisor
    10,  // Elephant
    5,  // Chariot
    3,   // Horse
    5,  // Cannon
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
const int AB_WIN_SCORE = 300;
const int MAT_WIN_SCORE = 150;
const double WIN = 1.0;
const double LOSS = 0.0;
constexpr int MAT_SIZE = 2916;
const double SCALING_FACTOR = 200.0;
const int MAX_NO_EAT_FLIP = 20;
const double MAX_TIME_MS = 15000.0;
const double MIN_TIME_MS = 100.0;

const int EXPECTED_PLYS = 80;
const int EXPECTED_PLY_LONG = 120;
const double ASPIRATION_WINDOW = 25.0;

const double EPSILON = 1e-5;
const int SCORE_TT_MOVE = 100000000;
const int SCORE_CAPTURE_BASE = 50000000;
const int SCORE_FLIP_BASE = 40000000;
const int SCORE_HISTORY_MAX = 10000000;

const double V_MAX = 320.0;
const double V_MIN = -320.0;

const int MAX_FLIP_BUDGET = 3;   // Max 2 flips per path
const int FLIP_COOLDOWN_REQ = 1; // Separate flips by 1 move
const int MIN_DEPTH_FOR_FLIP = 0; // Stop flipping near leaf

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

    double star1(const Move &mv, const Position &pos, double alpha, double beta, int depth, uint64_t key, Move &dummy_ref, int flip_budget, int cooldown);
    double f4(Position &pos, double alpha, double beta, int depth, uint64_t key, Move &best_move_ref, const Move pv_hint = Move(), int flip_budget = MAX_FLIP_BUDGET, int cooldown = 0);
    double eval(const Position &pos, const int depth);
    double pos_score(const Position &pos, Color cur_color);

    double estimatePlyTime(const Position& pos);

    void store_choice(uint64_t key, Move mv);
    Move check_previous_choice(uint64_t key);

    double qsearch(Position &pos, double alpha, double beta);

    bool time_out_ = false;
    int node_count_ = 0;
    int no_eat_flip = 0;
    int ply_count_ = 0;// total ply count in the game
    int prev_total_count = SQUARE_NB;
    std::chrono::time_point<std::chrono::steady_clock> start_time_{};
    int time_limit_ms_ = 5000;
    TranspositionTable tt_;
    ZobristHash zobrist_;

    void load_material_table();
    int get_material_index(const Position &pos, Color cur_color) const;
    double try_move(const Position &pos, const Move &mv, double alpha, double beta, int depth, uint64_t key, Move &dummy_ref, int flip_budget, int cooldown);

    const int init_counts[7] = {1, 2, 2, 2, 2, 2, 5};
    int unrevealed_count[2][7];// need track both!
    int prev_revealed_count[2][7];// for chance node
    
    std::unordered_map<uint64_t, Move> prev_choices_;
};

#endif