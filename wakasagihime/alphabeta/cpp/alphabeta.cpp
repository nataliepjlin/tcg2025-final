#ifndef ALPHABETA_CPP
#define ALPHABETA_CPP
#include "../h/alphabeta.h"
#include <fstream>

int AlphaBetaEngine::material_table[MAT_SIZE][MAT_SIZE];
bool AlphaBetaEngine::table_loaded = false;
static const double DISTANCE_TABLE_SCALED[11] = { 
    0.0, 0.5, 0.3, 0.2, 0.1, 0.05, 0.0, 0.0, 0.0, 0.0, 0.0 
};

static std::ofstream f3_dumper;
#define RET_LOG(score_expr) { \
    double _val = (score_expr); \
    if (!time_out_) { \
        if (!f3_dumper.is_open()) f3_dumper.open("f3_scores.csv", std::ios::app); \
        f3_dumper << _val << "\n"; \
    } \
    return _val; \
}

void log_position(int depth, Move best, bool last, bool start) {
    std::ofstream fout("./log_negascout.log", std::ios::app); // append mode
    if (!fout.is_open()) {
        std::cerr << "Failed to open log file\n";
        return;
    }
    if(start){
        fout << "A new game!\n";
        fout << "------------------------\n";}
    else if(last){
        fout << "------------------------\n";
    } else {
    fout << "Now depth: " << depth << "\n";
    fout << "Now best: " << best;}

    
    fout.close();
}

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
    int cur_total_count = pos.count();
    for(int c = 0; c < SIDE_NB; c++){
        for(int pt = General; pt <= Soldier; pt++){
            int unrevealed = init_counts[pt] - pos.count(Color(c), PieceType(pt));
            if(unrevealed_count[c][pt] != unrevealed){
                unrevealed_count[c][pt] = unrevealed;
                changed = true;
            }
        }
    }
    if(!changed && cur_total_count == prev_total_count){
        no_eat_flip++;
    }
    else{
        no_eat_flip = 0;
    }
    prev_total_count = cur_total_count;
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

double AlphaBetaEngine::star0(const Move &mv, const Position &pos, double alpha, double beta, int depth, uint64_t key, Move &dummy_ref){
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
            vsum += count * -f3(copy, -INF, INF, depth, child_key, dummy_ref);
            unrevealed_count[c][pt]++;// restore count
            D += count;
        }
    }

    if(D == 0) return 0; // should not happen

    return vsum / D;
}

double AlphaBetaEngine::try_move(const Position &pos, const Move &mv, double alpha, double beta, int depth, uint64_t key, Move &dummy_ref){
    if(mv.type() == Flipping){
        return star0(mv, pos, alpha, beta, depth - 1, key, dummy_ref);
    }
    else{
        Position copy(pos);
        copy.do_move(mv);
        uint64_t child_key = zobrist_.update_zobrist_hash(key, mv, pos, Piece());
        return -f3(copy, -beta, -alpha, depth - 1, child_key, dummy_ref, Move());
    }
}


double AlphaBetaEngine::f3(Position &pos, double alpha, double beta, int depth, uint64_t key, Move &best_move_ref, const Move pv_hint){
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
        best_move_ref = tt_move; 
        return tt_value;
    }

    // Terminal check
    if(pos.winner() != NO_COLOR){
        int diff = pos_score(pos, pos.due_up());
        if(pos.winner() == pos.due_up()) return AB_WIN_SCORE + depth + diff;
        else if(pos.winner() == Mystery) return diff; 
        else return -(AB_WIN_SCORE + depth) + diff;
    }

    // Depth Cutoff
    if(depth == 0) {
        return pos_score(pos, pos.due_up());
    }

    Move sort_move = (pv_hint != Move()) ? pv_hint : tt_move;
    auto moves = get_ordered_moves(pos, sort_move);
    
    if (moves.empty()) return -(AB_WIN_SCORE + depth);

    double m = -INF;
    double n = beta;
    Move best_move_this_node = moves[0].mv;
    best_move_ref = best_move_this_node; 

    Move dummy_ref;
    for(int i = 0; i < moves.size(); i++){        
        double t = try_move(pos, moves[i].mv, std::max(alpha, m), n, depth, key, dummy_ref);
        
        if(time_out_) return 0;

        if(t > m){
            if(n == beta || depth < 3 || t >= beta || moves[i].mv.type() == Flipping){
                m = t;
            }
            else{
                // Re-search
                m = try_move(pos, moves[i].mv, t, beta, depth, key, dummy_ref);
                if(time_out_) return 0;
            }
            best_move_this_node = moves[i].mv;
            best_move_ref = best_move_this_node; 
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
        n = std::max(alpha, m) + 0.001;
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
    prev_total_count = SQUARE_NB;
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
            score = SCORE_TT_MOVE; // Highest priority
        } 
        else if(mv.type() == Flipping){
            score = SCORE_FLIP_BASE + flip_score; // Higher than History
        }
        else{
            PieceType from = pos.peek_piece_at(mv.from()).type;
            PieceType to = pos.peek_piece_at(mv.to()).type;

            if(to != NO_PIECE){
                // Eating: Higher than Flips and History
                score = SCORE_CAPTURE_BASE + yummy_table[from][to];
            } 
            else{
                if(no_eat_flip >= MAX_NO_EAT_FLIP){
                    score = 1; // Avoid draw
                }
                else {
                    // History: Lowest priority (0 to 1,000,000)
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
        log_position(0, Move(), false, true);
        init_game();
        return Move(SQ_D2, SQ_D2); // flip center piece
    }
    this->ply_count_++;
    update_unrevealed(pos);// may be eaten by opponent in last turn

    Move best_move_root = Move();
    uint64_t key = zobrist_.compute_zobrist_hash(pos);

    Move tt_move = Move();
    double tt_val;
    double dummy_alpha = -INF, dummy_beta = INF;
    tt_.probe(key, dummy_alpha, dummy_beta, 0, tt_val, tt_move);
    auto moves = get_ordered_moves(pos, best_move_root);
    if(best_move_root == Move()){
        best_move_root = moves[0].mv;
    }
    else if(moves.empty()){
        error << "NO AVAILABLE MOVE\n";
    }

    for(int depth = 1; depth <= 50; depth++){
        
        Move best_move_this_iter = Move();
        
        f3(pos, -INF, INF, depth, key, best_move_this_iter, best_move_root);
        
        if(time_out_){
            if(best_move_this_iter != Move()){
                best_move_root = best_move_this_iter;
            }
            log_position(depth, best_move_root, true, false);
            break;
        }
        
        best_move_root = best_move_this_iter;
        log_position(depth, best_move_root, false, false);
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
                score += DISTANCE_TABLE_SCALED[min_dist_to_this_enemy];
            }
        }
    }

    return score;
}


#endif
