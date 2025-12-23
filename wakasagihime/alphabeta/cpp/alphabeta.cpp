#ifndef ALPHABETA_CPP
#define ALPHABETA_CPP
#include "../h/alphabeta.h"

int AlphaBetaEngine::material_table[MAT_SIZE][MAT_SIZE];
bool AlphaBetaEngine::table_loaded = false;
static const int DISTANCE_TABLE[11] = { 
    0, 
    512, 256, 128, 64, 32, 16, 8, 4, 2, 1 
};

AlphaBetaEngine::AlphaBetaEngine(){
    zobrist_.init_zobrist();

    if(!table_loaded){
        load_material_table();
        table_loaded = true;
    }
}

void AlphaBetaEngine::load_material_table(){
    std::ifstream in("material_scores.bin", std::ios::binary);
    if(!in){
        error << "Error: Could not open material_scores.bin\n";
        return;
    }
    in.read(reinterpret_cast<char*>(material_table), sizeof(material_table));
    in.close();
    debug << "Material table loaded.\n";
}

void AlphaBetaEngine::update_unrevealed(const Position &pos){
    bool changed = false;
    for(int c = 0; c < SIDE_NB; c++){
        for(int pt = General; pt <= Soldier; pt++){
            int unrevealed = init_counts[pt] - pos.count(Color(c), PieceType(pt));
            if(unrevealed_count[c][pt] != unrevealed){
                unrevealed_count[c][pt] = unrevealed;
                changed = true;
            }
        }
    }
    if(!changed){
        no_eat_flip++;
    }
    else{
        no_eat_flip = 0;
    }
}

double AlphaBetaEngine::eval(const Position &pos, const int depth){
    if(pos.winner() != NO_COLOR){
        if(pos.winner() == pos.due_up()){
            return std::min(1.0, 0.98 + (0.001 * depth));
        }
        else if(pos.winner() == Mystery){// opponent wins
            return 0.2;
        }
        else{
            return std::max(0.0, 0.02 - (0.001 * depth));
        }
    }
    return pos_score(pos, pos.due_up());
}

double AlphaBetaEngine::star0(const Move &mv, const Position &pos, double alpha, double beta, int depth, uint64_t key){
    (void)alpha;
    (void)beta;
    double vsum = 0;
    int D = 0;// total number of unrevealed pieces
    for(Color c: {Red, Black}){
        for(int pt = General; pt <= Soldier; pt++){
            if(unrevealed_count[c][pt] <= 0) continue;

            Piece p(c, PieceType(pt));
            int count = unrevealed_count[c][pt];

            Position copy(pos);
            copy.clear_collection();
            Piece force_set[1] = {p};
            copy.add_collection(force_set, 1);
            copy.do_move(mv);
            uint64_t child_key = zobrist_.update_zobrist_hash(key, mv, pos, p);
            unrevealed_count[c][pt]--;// temporarily decrease count
            vsum += count * -f3(copy, -INF, INF, depth, child_key);
            unrevealed_count[c][pt]++;// restore count
            D += count;
        }
    }

    if(D == 0) return 0; // should not happen

    return vsum / D;
}

double AlphaBetaEngine::try_move(const Position &pos, const Move &mv, double alpha, double beta, int depth, uint64_t key){
    if(mv.type() == Flipping){
        return star0(mv, pos, alpha, beta, depth - 1, key);
    }
    else{
        Position copy(pos);
        copy.do_move(mv);
        uint64_t child_key = zobrist_.update_zobrist_hash(key, mv, pos, Piece());
        return -f3(copy, -beta, -alpha, depth - 1, child_key);
    }
}


double AlphaBetaEngine::f3(Position &pos, double alpha, double beta, int depth, uint64_t key){
    // Check time every 256 nodes for more frequent timeout checks
    if((++node_count_ & 255) == 0){
        auto now = std::chrono::steady_clock::now();
        if(std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count() > time_limit_ms_){
            time_out_ = true;
            return 0;
        }
    }
    if(time_out_) return 0;
    
    Move tt_move = Move();
    double tt_value;
    if(tt_.probe(key, alpha, beta, depth, tt_value, tt_move)){
        return tt_value;
    }

    // Depth limit check
    if(pos.winner() != NO_COLOR){
        int diff = pos_score(pos, pos.due_up());
        if(pos.winner() == pos.due_up())
            return AB_WIN_SCORE + depth + diff;
        else if(pos.winner() == Mystery)
            return diff; // Draw
        else
            return -(AB_WIN_SCORE + depth) + diff;
    }

    // Depth Cutoff
    if(depth == 0) {
        return pos_score(pos, pos.due_up());
    }

    auto moves = get_ordered_moves(pos, tt_move);
    double m = -INF;
    Move best_move_this_node = moves[0].mv;
    for(int i = 0; i < moves.size(); i++){        
        double t = try_move(pos, moves[i].mv, std::max(alpha, m), beta, depth, key);
        if(time_out_){
            return 0;
        }
        if(t > m){
            m = t;
            best_move_this_node = moves[i].mv;
        }
        if(m >= beta){
            tt_.store(key, m, depth, TT_BETA, best_move_this_node);

            if (moves[i].mv.type() != Flipping) {
                // Weight = 2^depth. Clamp depth to avoid overflow
                int weight = 1 << std::min(depth, 14);
                history_table_[moves[i].mv.from()][moves[i].mv.to()] += weight;
                
                // Aging check: Prevent overflow (e.g. if > 1M)
                if(history_table_[moves[i].mv.from()][moves[i].mv.to()] > 1000000){
                    age_history_table();
                }
            }

            return m; // Beta cutoff
        }
    }

    if(m > alpha){
        tt_.store(key, m, depth, TT_EXACT, best_move_this_node);
        if(best_move_this_node.type() != Flipping){
            int weight = 1 << std::min(depth, 14);
            history_table_[best_move_this_node.from()][best_move_this_node.to()] += weight;
        }
    }
    else{
        tt_.store(key, m, depth, TT_ALPHA, best_move_this_node);
    }

    return m;
}

void AlphaBetaEngine::init_game(){
    ply_count_ = 0;
    no_eat_flip = 0;
    std::memset(history_table_, 0, sizeof(history_table_));
    for(int pt = General; pt <= Soldier; pt++){
        unrevealed_count[0][pt] = init_counts[pt];
        unrevealed_count[1][pt] = init_counts[pt];
    }
}

void AlphaBetaEngine::age_history_table() {
    // Halve all history scores to prioritize recent info and prevent overflow
    for (int i = 0; i < SQUARE_NB; i++) {
        for (int j = 0; j < SQUARE_NB; j++) {
            history_table_[i][j] >>= 1; 
        }
    }
}
std::vector<AlphaBetaEngine::ScoredMove> AlphaBetaEngine::get_ordered_moves(const Position &pos, Move tt_move) {
    MoveList<> raw_moves(pos);
    std::vector<ScoredMove> moves;
    moves.reserve(raw_moves.size());

    for(Move mv : raw_moves){
        int score = 0;

        if(mv == tt_move){
            score = 2000000;
        } 
        else if(mv.type() == Flipping){
            score = 100000 + flip_score;
        }
        else{
            PieceType from = pos.peek_piece_at(mv.from()).type;
            PieceType to = pos.peek_piece_at(mv.to()).type;

            if(to != NO_PIECE){
                score = 100000 + yummy_table[from][to];
            } 
            else{
                if(no_eat_flip >= MAX_NO_EAT_FLIP){
                    score = 1;// avoid draw
                }
                else {
                    score = history_table_[mv.from()][mv.to()];
                }
            }
        }
        moves.push_back({mv, score});
    }

    // Sort descending
    std::sort(moves.begin(), moves.end(), [](const ScoredMove& a, const ScoredMove& b) {
        return a.score > b.score;
    });

    return moves;
}

double AlphaBetaEngine::estimatePlyTime(const Position& pos) {
    double exp_ply = this->ply_count_ + pos.count() + this->no_eat_flip;
    if(exp_ply < EXPECTED_PLYS){
        exp_ply = EXPECTED_PLYS;
    }
    else if(exp_ply < EXPECTED_PLY_LONG){
        exp_ply = EXPECTED_PLY_LONG;
    }

    return std::max(MIN_TIME_MS, std::min(MAX_TIME_MS, pos.time_left() / (exp_ply-this->ply_count_+1) ));
}

Move AlphaBetaEngine::search(Position &pos){
    start_time_ = std::chrono::steady_clock::now();
    time_out_ = false;
    node_count_ = 0;
    
    // handle new game start
    if(pos.count(Hidden) == SQUARE_NB){
        // restore unrevealed pieces
        init_game();
        return Move(SQ_D2, SQ_D2); // flip center piece
    }
    this->ply_count_++;
    update_unrevealed(pos);// may be eaten by opponent in last turn

    double alloc_ms = estimatePlyTime(pos);
    time_limit_ms_ = static_cast<int>(alloc_ms);
    double time_limit_sec = alloc_ms / 1000.0; 

    Move best_move_root = Move();
    uint64_t key = zobrist_.compute_zobrist_hash(pos);

    Move tt_move = Move();
    double tt_val;
    double dummy_alpha = -INF, dummy_beta = INF;
    if(tt_.probe(key, dummy_alpha, dummy_beta, 0, tt_val, tt_move)){
        best_move_root = tt_move; // Seed the best move!
    }
    auto moves = get_ordered_moves(pos, best_move_root);
    if(best_move_root == Move()){
        best_move_root = moves[0].mv;
    }

    double duration_prev_depth = 0.0;

    for(int depth = 1; depth <= 50;){
        auto iter_start = std::chrono::steady_clock::now();
        double mx = -INF;
        double alpha = -INF;
        double beta = INF;
        
        Move best_move_this_iter = moves[0].mv;
        if(best_move_root == Move()){// safe-guard for first iter
            best_move_root = best_move_this_iter;
        }

        // Search root children manually to track best_move
        for(int i = 0; i < moves.size(); i++){
            double t = try_move(pos, moves[i].mv, std::max(alpha, mx), beta, depth, key);
            if(time_out_){
                if(i > 0){
                    // partial pv
                    best_move_root = best_move_this_iter;
                }
                return best_move_root;
            }
            if(t > mx){
                mx = t;
                best_move_this_iter = moves[i].mv;
            }
        }
        if(time_out_){
            break;
        }
        if(mx > alpha){
            tt_.store(key, mx, depth, TT_EXACT, best_move_this_iter);
        }
        else{
            tt_.store(key, mx, depth, TT_ALPHA, best_move_this_iter);
        }
        
        best_move_root = best_move_this_iter;

        // Measure duration of THIS iteration
        auto iter_end = std::chrono::steady_clock::now();
        std::chrono::duration<double> duration = iter_end - iter_start;
        duration_prev_depth = duration.count();

        if(depth == 1) depth = 2;
        else depth += 2;

        moves = get_ordered_moves(pos, best_move_root);// reorder for next iter
    }
    return best_move_root;
}

int AlphaBetaEngine::get_material_index(const Position &pos, Color c) const{
    return pos.count(c, Soldier) +
           pos.count(c, Cannon) * 6 +
           pos.count(c, Horse) * 18 +
           pos.count(c, Chariot) * 54 +
           pos.count(c, Elephant) * 162 +
           pos.count(c, Advisor) * 486 +
           pos.count(c, General) * 1458;
}

double AlphaBetaEngine::pos_score(const Position &pos, const Color cur_color){
    double score = 0;
    Color opp_color = Color(cur_color ^ 1);
    
    for(Square sq = SQ_A1; sq < SQUARE_NB; sq = Square(sq + 1)){
        Piece p = pos.peek_piece_at(sq);
        if(p.side == cur_color){
            score += Piece_Value[p.type];
        }
        else if(p.side == opp_color){
            score -= Piece_Value[p.type];
        }
    }

    Board my_board = pos.pieces(cur_color);
    Board opp_board = pos.pieces(opp_color);
    if(pos.count(cur_color) && pos.count(opp_color)){
        for(Square opp_sq : BoardView(opp_board)){
            PieceType opp_pc = pos.peek_piece_at(opp_sq).type;
            
            int min_dist_to_this_enemy = 1000;
            bool can_be_killed = false;

            for(Square my_sq : BoardView(my_board)){
                PieceType my_pc = pos.peek_piece_at(my_sq).type;
                
                // Only measure distance if I can actually hurt them
                if(my_pc > opp_pc && !(opp_pc > my_pc)){
                    int d = SquareDistance[my_sq][opp_sq];
                    if(d < min_dist_to_this_enemy){
                        min_dist_to_this_enemy = d;
                        can_be_killed = true;
                    }
                }
            }
            if(can_be_killed){
                score -= min_dist_to_this_enemy;
            }
        }
    }

    return score;
}


#endif
