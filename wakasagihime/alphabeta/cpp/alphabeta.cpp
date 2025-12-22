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

    // if(!table_loaded){
    //     load_material_table();
    //     table_loaded = true;
    // }
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

void AlphaBetaEngine::update_unrevealed(const Position &pos){
    for(int c = 0; c < SIDE_NB; c++){
        for(int pt = General; pt <= Soldier; pt++){
            int revealed = pos.count(Color(c), PieceType(pt));
            unrevealed_count[c][pt] = std::min(init_counts[pt] - revealed, unrevealed_count[c][pt]);
        }
    }
}

int AlphaBetaEngine::star0(const Move &mv, const Position &pos, int alpha, int beta, int depth, uint64_t key){
    (void)alpha;
    (void)beta;
    int vsum = 0;
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


int AlphaBetaEngine::f3(Position &pos, int alpha, int beta, int depth, uint64_t key){
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
    int tt_value;
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

    MoveList<> moves(pos);
    std::sort(moves.begin(), moves.end(), [&](Move a, Move b){
        if(a == tt_move) return true;
        if(b == tt_move) return false;
        
        PieceType afrom = pos.peek_piece_at(a.from()).type;
        PieceType ato = pos.peek_piece_at(a.to()).type;
        int score_a = a.from() == a.to() ? flip_score :
        ato == NO_PIECE ? yummy_table[afrom][7] : yummy_table[afrom][ato];

        PieceType bfrom = pos.peek_piece_at(b.from()).type;
        PieceType bto = pos.peek_piece_at(b.to()).type;
        int score_b = b.from() == b.to() ? flip_score :
        bto == NO_PIECE ? yummy_table[bfrom][7] : yummy_table[bfrom][bto];
        return score_a > score_b;
    });
    Move best_move_this_node = moves[0];

    int mx = -INF;

    for(Move mv : moves){        
        int t;
        if(mv.type() == Flipping){
            t = star0(mv, pos, -beta, -std::max(alpha, mx), depth - 1, key);
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

    MoveList<> moves(pos);
    if(moves.size() == 0) {
        // Should not happen in valid game state, but safety check
        return Move();
    }

    std::sort(moves.begin(), moves.end(), [&](Move a, Move b){
        PieceType afrom = pos.peek_piece_at(a.from()).type;
        PieceType ato = pos.peek_piece_at(a.to()).type;
        int score_a = a.from() == a.to() ? flip_score :
        ato == NO_PIECE ? yummy_table[afrom][7] : yummy_table[afrom][ato];

        PieceType bfrom = pos.peek_piece_at(b.from()).type;
        PieceType bto = pos.peek_piece_at(b.to()).type;
        int score_b = b.from() == b.to() ? flip_score :
        bto == NO_PIECE ? yummy_table[bfrom][7] : yummy_table[bfrom][bto];
        return score_a > score_b;
    });

    // handle new game start
    if(pos.count(Hidden) == SQUARE_NB){
        // restore unrevealed pieces
        for(int pt = General; pt <= Soldier; pt++){
            unrevealed_count[0][pt] = init_counts[pt];
            unrevealed_count[1][pt] = init_counts[pt];
        }
        prev_depth_ = 1;
        return Move(SQ_D2, SQ_D2); // flip center piece
    }
    
    update_unrevealed(pos);// may be eaten by opponent in last turn

    int best_move = 0;
    int best_move_this_iter = 0;
    uint64_t key = zobrist_.compute_zobrist_hash(pos);

    // 3. Iterative Deepening Loop
    // Start at depth 1, increase until time runs out
    for (int depth = prev_depth_; depth <= 50; depth++){

        int mx = -INF;
        int alpha = -INF;
        int beta = INF;

        // Search root children manually to track best_move
        for(int i = 0; i < moves.size(); i++){

            // Call f3 with depth - 1
            int t;
            if(moves[i].type() == Flipping){
                t = star0(moves[i], pos, -beta, -std::max(alpha, mx), depth - 1, key);
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
            break;
        }
        else{
            best_move = best_move_this_iter;
            // Optimization: If we found a forced mate, stop early
            if(mx > FORCE_WIN_THRESHOLD)
                break;
        }
    }

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
    int score = 0;
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
