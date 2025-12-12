#include "../h/zobrist.h"

void ZobristHash::init_zobrist(){
    rng64.seed(42);
    for(int t = 0; t < TOTAL_TYPE; t++){
        for(Square sq = SQ_A1; is_okay(sq); sq += 1){
            zob[t][sq] = rng64();
        }
    }
    sideToMove_hash = rng64();
}

uint64_t ZobristHash::compute_zobrist_hash(const Position &pos){
    uint64_t h = 0;
    for(Square sq: BoardView(pos.pieces())){
        Piece p = pos.peek_piece_at(sq);
        int type_index = p.type;
        if(p.type == Hidden){
            type_index = HIDDEN_TYPE_INDEX;
        }
        else if(p.side == Black){
            type_index += BLACK_OFFSET;
        }
        h ^= zob[type_index][sq];
    }
    if(pos.due_up() == Black){
        h ^= sideToMove_hash;
    }
    return h;
}

uint64_t ZobristHash::update_zobrist_hash(uint64_t hash, const Move &mv, const Position &pos, Piece flip_piece){
    hash ^= sideToMove_hash;
    if(mv.type() == Flipping){
        Square sq = mv.from();
        hash ^= zob[HIDDEN_TYPE_INDEX][sq];
        int type_index = flip_piece.type;
        if(flip_piece.side == Black){
            type_index += BLACK_OFFSET;
        }
        hash ^= zob[type_index][sq];
    }
    else{
        Square from = mv.from();
        Square to = mv.to();
        Piece target = pos.peek_piece_at(to);
        if(target.type != NO_PIECE){
            int target_index = target.type;
            if (target.side == Black) target_index += BLACK_OFFSET;
            hash ^= zob[target_index][to];
        }
        Piece mover = pos.peek_piece_at(from);
        int mover_index = mover.type;
        if(mover.side == Black) mover_index += BLACK_OFFSET;

        hash ^= zob[mover_index][from];
        hash ^= zob[mover_index][to];
    }
    return hash;
}