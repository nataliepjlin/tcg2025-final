#include <iostream>
#include <vector>
#include <algorithm>
#include <climits>
#include <fstream>
#include <set>
#include <iomanip>

// --- 定義常數 ---
constexpr int S_Soldier  = 1;
constexpr int S_Cannon   = 6;
constexpr int S_Horse    = 6 * 3;
constexpr int S_Chariot  = 6 * 3 * 3;
constexpr int S_Elephant = 6 * 3 * 3 * 3;
constexpr int S_Advisor  = 6 * 3 * 3 * 3 * 3;
constexpr int S_General  = 6 * 3 * 3 * 3 * 3 * 3;

constexpr int TABLE_SIZE = S_General * 2; // 2916


const int WIN_TIER_SCORES[6] = {
    0,
    150,
    185,
    220,
    255,
    290
};
const int ELIMINATION_SCORE = 300;

constexpr int I_SOLDIER  = 0;
constexpr int I_CANNON   = 1;
constexpr int I_HORSE    = 2;
constexpr int I_CHARIOT  = 3;
constexpr int I_ELEPHANT = 4;
constexpr int I_ADVISOR  = 5;
constexpr int I_GENERAL  = 6;
constexpr int P_COUNT    = 7;

const int MAX_CNTS[P_COUNT] = {5, 2, 2, 2, 2, 2, 1};
const int BASE_SCORES[P_COUNT] = {1, 10, 3, 5, 10, 15, 30};

int eval_table[TABLE_SIZE][TABLE_SIZE] = {{0}};

int score_base(const int my_cnts[7], const int op_cnts[7]){
    int score = 0;
    for(int i = 0; i < P_COUNT; i++){
        score += my_cnts[i] * BASE_SCORES[i];
        score -= op_cnts[i] * BASE_SCORES[i];
    }
    return score;
}

bool can_capture(int attacker_type, int victim_type) {
    if (attacker_type == I_CANNON) return false; // Cannon handled separately logic usually, or depends on rules
    
    // 1. Soldier Case
    if (attacker_type == I_SOLDIER) {
        // Soldier eats General AND Soldier
        return (victim_type == I_GENERAL || victim_type == I_SOLDIER);
    }
    
    // 2. General Case (The Fix)
    if (attacker_type == I_GENERAL && victim_type == I_SOLDIER) {
        return false; // General CANNOT eat Soldier
    }

    // 3. Standard Rank Logic
    return attacker_type >= victim_type; 
}

int forced_win(const int my_cnts[7], const int op_cnts[7]){
    int total_my = 0;
    int total_op = 0;
    for(int i = 0; i < P_COUNT; i++){
        total_my += my_cnts[i];
        total_op += op_cnts[i];
    }
    if(op_cnts[I_CANNON] > 0){
        if(total_op == 1){
            int num_strong = 0;
            for(int i = 0; i < P_COUNT; i++){
                if(my_cnts[i] > 0 && can_capture(i, I_CANNON)){
                    num_strong++;
                }
            }
            return num_strong;
        }
        return 0;// cannot force win if opponent has cannon + other pieces
    }

    int total_forced = 0;
    for(int i = 0; i < P_COUNT; i++){
        bool cap_all = true;
        for(int j = 0; j < P_COUNT; j++){
            if(op_cnts[j] > 0){
                if(!can_capture(i, j)){
                    cap_all = false;
                }
                else if(i == j && op_cnts[i] <= op_cnts[j]){
                    cap_all = false;
                }
            }
        }
        total_forced += cap_all ? my_cnts[i] : 0;
    }
    return total_forced == 1 ? (total_op == 1 ? 2 : 1) : total_forced;
}

void idx_to_counts(int idx, int cnts[]) {
    for (int i = 0; i < P_COUNT; ++i) {
        cnts[i] = idx % (MAX_CNTS[i] + 1);
        idx /= (MAX_CNTS[i] + 1);
    }
}


void generate_eval_table() {
    int my_cnts[P_COUNT];
    int op_cnts[P_COUNT];

    for(int i = 0; i < TABLE_SIZE; i++){
        idx_to_counts(i, my_cnts);
        for(int j = 0; j < TABLE_SIZE; j++){
            idx_to_counts(j, op_cnts);
            int idx = forced_win(my_cnts, op_cnts);
            int idx2 = forced_win(op_cnts, my_cnts);
            if(idx != 0){
                eval_table[i][j] = WIN_TIER_SCORES[idx];
            }
            else if(idx2 != 0){
                eval_table[i][j] = -WIN_TIER_SCORES[idx2];
            }
            else{
                eval_table[i][j] = score_base(my_cnts, op_cnts);
            }
            
        }
    }

    for(int i = 0; i < TABLE_SIZE; i++){
        eval_table[i][i] = 0;
        eval_table[i][0] = ELIMINATION_SCORE;
        eval_table[0][i] = -ELIMINATION_SCORE;
    }
}

void save_to_binary() {
    std::ofstream file("material_scores.bin", std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not create material_scores.bin" << std::endl;
        return;
    }
    file.write(reinterpret_cast<const char*>(eval_table), sizeof(eval_table));
    file.close();
    std::cout << "Saved material_scores.bin (" << sizeof(eval_table) << " bytes)" << std::endl;
}

int main() {
    std::cout << "Generating Eval Table..." << std::endl;
    generate_eval_table();
    
    save_to_binary();
    return 0;
}