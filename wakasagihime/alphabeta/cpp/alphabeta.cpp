#ifndef ALPHABETA_CPP
#define ALPHABETA_CPP
#include "../h/alphabeta.h"

int AlphaBetaEngine::material_table[MAT_SIZE][MAT_SIZE];
bool AlphaBetaEngine::table_loaded = false;
static const int DISTANCE_TABLE[11] = { 
    0, 
    1, 256, 128, 64, 32, 16, 8, 4, 2, 1 
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
    std::cout << "Material table loaded.\n";
}

std::vector<std::pair<Piece, int>> AlphaBetaEngine::get_unrevealed(const Position &pos) const{
    static const int init_counts[] = {1, 2, 2, 2, 2, 2, 5};

    std::vector<std::pair<Piece, int>> unrevealed;
    for(Color c : {Red, Black}){
        for(PieceType pt = General; pt <= Soldier; pt = PieceType(pt + 1)){
            int total_count = init_counts[pt];
            int revealed_count = pos.count(c, pt);
            int unrevealed_count = total_count - revealed_count;
            if(unrevealed_count > 0){
                unrevealed.push_back(std::make_pair(Piece(c, pt), unrevealed_count));
            }
        }
    }
    return unrevealed;
}

long double AlphaBetaEngine::star0(const Move &mv, const Position &pos, long double alpha, long double beta, int depth, uint64_t key){
    (void)alpha;
    (void)beta;
    long double vsum = 0;
    // get all possible flip result
    std::vector<std::pair<Piece, int>> unrevealed = get_unrevealed(pos);
    int D = 0;// total number of unrevealed pieces
    for(const auto& [p, count] : unrevealed){
        Position copy(pos);
        copy.clear_collection();
        Piece force_set[1] = {p};
        copy.add_collection(force_set, 1);
        copy.do_move(mv);
        uint64_t child_key = zobrist_.update_zobrist_hash(key, mv, pos, p);
        vsum += count * -f3(copy, -INF, INF, depth, child_key);
        D += count;
    }

    if(D == 0) return 0; // should not happen

    return vsum / D;
}

long double AlphaBetaEngine::f3(Position &pos, long double alpha, long double beta, int depth, uint64_t key){
    // Check time every 256 nodes for more frequent timeout checks
    if ((++node_count_ & 255) == 0){
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count() > kTimeLimitMs){
            time_out_ = true;
            return 0;
        }
    }
    if(time_out_) return 0;
    
    Move tt_move = Move();
    long double tt_value;
    if(tt_.probe(key, alpha, beta, depth, tt_value, tt_move)){
        return tt_value;
    }

    // Depth limit check
    if(pos.winner() != NO_COLOR){
        int diff = pos_score(pos, pos.due_up());
        // FIX: Add depth bonus. Higher depth value = shallower in tree (closer to root) = faster win.
        // Assuming depth counts DOWN from Max to 0.
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

    MoveList<> moves(pos);
    if(tt_move != Move()){
        // Move tt_move to front
        auto it = std::find(moves.begin(), moves.end(), tt_move);
        if(it != moves.end()){
            std::iter_swap(moves.begin(), it);
        }
    }
    Move best_move_this_node = moves[0];

    long double mx = -INF;

    for(Move mv : moves){        
        long double t;
        if(mv.type() == Flipping){
            t = star0(mv, pos, -beta, -std::max(alpha, mx), depth - 2, key);
        }
        else{
            Position copy(pos);
            copy.do_move(mv);
            uint64_t child_key = zobrist_.update_zobrist_hash(key, mv, pos, Piece());
            t = -f3(copy, -beta, -std::max(alpha, mx), depth - 1, child_key);
        }

        if (time_out_) return 0;

        if(t > mx){
            mx = t;
            best_move_this_node = mv;
        }
        if(mx >= beta){
            tt_.store(key, mx, depth, TT_BETA, mv);
            return mx;
        }
    }

    if(mx > alpha){
        tt_.store(key, mx, depth, TT_EXACT, best_move_this_node);
    }
    else{
        tt_.store(key, mx, depth, TT_ALPHA, best_move_this_node);
    }

    return mx;
}

Move AlphaBetaEngine::search(Position &pos){
    start_time_ = std::chrono::steady_clock::now();
    time_out_ = false;
    node_count_ = 0;

    if(pos.pieces(FACE_UP) == 0){ // Opening move
        // Flip center
        return Move(SQ_D2, SQ_D2);
    }

    MoveList<> moves(pos);
    if(moves.size() == 0) {
        // Should not happen in valid game state, but safety check
        return Move();
    }

    int best_move = 0;
    int best_move_this_iter = 0;
    uint64_t key = zobrist_.compute_zobrist_hash(pos);

    // 3. Iterative Deepening Loop
    // Start at depth 1, increase until time runs out
    int depth = 1;
    for (; depth <= 50; depth += 2){

        long double mx = -INF;
        long double alpha = -INF;
        long double beta = INF;

        // Search root children manually to track best_move
        for(int i = 0; i < moves.size(); i++){

            // Call f3 with depth - 1
            long double t;
            if(moves[i].type() == Flipping){
                t = star0(moves[i], pos, -beta, -std::max(alpha, mx), depth - 2, key);
            }
            else{
                Position copy(pos);
                copy.do_move(moves[i]);
                uint64_t child_key = zobrist_.update_zobrist_hash(key, moves[i], pos, Piece());
                t = -f3(copy, -beta, -std::max(alpha, mx), depth - 1, child_key);
            }

            if (time_out_) break; // Break inner loop

            if(t > mx){
                mx = t;
                best_move_this_iter = i;
            }
        }

        if (time_out_){
            // If we timed out during Depth K, the results are incomplete/garbage.
            // We MUST discard Depth K and return the result from Depth K-1.
            break;
        }
        else{
            best_move = best_move_this_iter;
            // Optimization: If we found a forced mate, stop early
            if(mx > FORCE_WIN_THRESHOLD)
                break;
        }
    }
    debug << "AlphaBeta: Search completed with depth = " << depth - 2 << ", " << node_count_ << " nodes evaluated.\n";
    return moves[best_move];
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

int AlphaBetaEngine::pos_score(const Position &pos, const Color cur_color) const{
    Color opp_color = Color(cur_color ^ 1);
    int my_index = get_material_index(pos, cur_color);
    int opp_index = get_material_index(pos, opp_color);
    int my_raw_score = material_table[my_index][opp_index];
    int opp_raw_score = material_table[opp_index][my_index];

    int total_score = my_raw_score + opp_raw_score;
    if(total_score == 0) return 0; // avoid division by zero

    const int SCALE = 10000;
    long long score_calc = (long long)(my_raw_score - opp_raw_score) * SCALE;
    int final_score = static_cast<int>(score_calc / total_score);

    return final_score;
}


#endif
