#include <iostream>
#include <cmath>
#include <cstdio>
#define SIZE 2916

static int scores[SIZE][SIZE]; // static storage keeps the table off the stack
static inline int round_score(double base, double exponent) {
    // Round the exponential score to the nearest integer instead of truncating it.
    return static_cast<int>(std::lround(std::pow(base, exponent)));
}
int get_material_index(const int g, const int a, const int e, const int c, const int h, const int n, const int s){
    return g * 3 * 3 * 3 * 3 * 3 * 6 +
           a * 3 * 3 * 3 * 3 * 6 +
           e * 3 * 3 * 3 * 6 +
           c * 3 * 3 * 6 +
           h * 3 * 6 +
           n * 6 +
           s;
}
int main(){

    // calculate single piece scores first
    for(int g = 0; g <= 1; g++){
    for(int a = 0; a <= 2; a++){
    for(int e = 0; e <= 2; e++){
    for(int c = 0; c <= 2; c++){
    for(int h = 0; h <= 2; h++){
    for(int n = 0; n <= 2; n++){// cannons
    for(int s = 0; s <= 5; s++){
        int opp_index = get_material_index(g, a, e, c, h, n, s);
        int my_index = 1;
        // soldier
        int predator = a + e + c + h + n;
        scores[my_index][opp_index] = round_score(1.5, 16-predator-s/2.0);
        std::cout << "Soldier score for my_index " << my_index << " and opp_index " << opp_index << " is " << scores[my_index][opp_index] << "\n";
        
        predator += g;
        // cannon
        my_index *= 6;
        scores[my_index][opp_index] = round_score(1.5, 16-predator/2.0);
        std::cout << "Cannon score for my_index " << my_index << " and opp_index " << opp_index << " is " << scores[my_index][opp_index] << "\n";

        // horse
        my_index *= 3;
        predator = predator - n - h;
        scores[my_index][opp_index] = round_score(1.5, 16-predator-(h+n)/2.0);
        std::cout << "Horse score for my_index " << my_index << " and opp_index " << opp_index << " is " << scores[my_index][opp_index] << "\n";
        
        // chariot
        my_index *= 3;
        predator -= c;
        scores[my_index][opp_index] = round_score(1.5, 16-predator-(c+n)/2.0);
        std::cout << "Chariot score for my_index " << my_index << " and opp_index " << opp_index << " is " << scores[my_index][opp_index] << "\n";

        // elephant
        my_index *= 3;
        predator -= e;
        scores[my_index][opp_index] = round_score(1.5, 16-predator-(e+n)/2.0);
        std::cout << "Elephant score for my_index " << my_index << " and opp_index " << opp_index << " is " << scores[my_index][opp_index] << "\n";

        // advisor
        my_index *= 3;
        predator -= a;
        scores[my_index][opp_index] = round_score(1.5, 16-predator-(a+n)/2.0);
        std::cout << "Advisor score for my_index " << my_index << " and opp_index " << opp_index << " is " << scores[my_index][opp_index] << "\n";

        // general
        my_index *= 2;
        predator = s;
        scores[my_index][opp_index] = round_score(1.5, 16-predator-(g+n)/2.0);
        std::cout << "General score for my_index " << my_index << " and opp_index " << opp_index << " is " << scores[my_index][opp_index] << "\n";
    }
    }
    }
    }
    }
    }
    }

    // combine piece scores
    for(int g = 0; g <= 1; g++){
    for(int a = 0; a <= 2; a++){
    for(int e = 0; e <= 2; e++){
    for(int c = 0; c <= 2; c++){
    for(int h = 0; h <= 2; h++){
    for(int n = 0; n <= 2; n++){// cannons
    for(int s = 0; s <= 5; s++){
        int my_index = get_material_index(g, a, e, c, h, n, s);
        for(int i = 0; i < SIZE; i++){
            scores[my_index][i] += s * scores[1][i];
            scores[my_index][i] += n * scores[6][i];
            scores[my_index][i] += h * scores[18][i];
            scores[my_index][i] += c * scores[54][i];
            scores[my_index][i] += e * scores[162][i];
            scores[my_index][i] += a * scores[486][i];
            scores[my_index][i] += g * scores[1458][i];
        }
    }
    }
    }
    }
    }
    }
    }

    FILE* fp = fopen("material_scores.bin", "wb");
    if(fp){
        size_t written = fwrite(scores, sizeof(int), SIZE * SIZE, fp);
        if(written != SIZE * SIZE){
            std::cerr << "Error writing to file, only wrote " << written << " elements\n";
        }
        fclose(fp);
    }
    else{
        std::cerr << "Error opening file for writing\n";
    }
    return 0;
}