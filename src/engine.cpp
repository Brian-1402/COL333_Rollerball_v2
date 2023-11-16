#include <algorithm>
#include <random>
#include <iostream>
#include <thread>

#include "board.hpp"
#include "engine.hpp"
#include "butils.hpp"
#include "bdata.hpp"

#include <algorithm>
#include <random>
#include <iostream>
#include <limits>
#include <chrono>
#include <bitset>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <math.h>

typedef uint8_t U8;
typedef uint16_t U16;

int max_ids_store_depth = 5;
std::unordered_map<std::string, std::tuple<std::unordered_set<U16>, std::vector<BoardData>, float>> move_histories; // dictionary storing the present state (board_0 in its string format) and its corresponding moves, board states after moves, and the score of the board state
std::vector<std::string> moves_taken;
std::vector<U8> last_killed_pieces;
std::vector<int> last_killed_pieces_idx;

void do_move(Board *b, U16 move)

{

    U8 p0 = getp0(move);
    U8 p1 = getp1(move);
    U8 promo = getpromo(move);
    U8 piecetype = b->data.board_0[p0];
    last_killed_pieces.push_back(0);
    last_killed_pieces_idx.push_back(-1);

    // scan and get piece from coord
    U8 *pieces = (U8 *)b;
    for (int i = 0; i < 2 * b->data.n_pieces; i++)
    {
        if (pieces[i] == p1)
        {
            pieces[i] = DEAD;
            last_killed_pieces.back() = b->data.board_0[p1];
            last_killed_pieces_idx.back() = i;
        }
        if (pieces[i] == p0)
        {
            pieces[i] = p1;
        }
    }

    if (promo == PAWN_ROOK)
    {
        piecetype = (piecetype & (WHITE | BLACK)) | ROOK;
    }
    else if (promo == PAWN_BISHOP)
    {
        piecetype = (piecetype & (WHITE | BLACK)) | BISHOP;
    }

    b->data.board_0[b->data.transform_array[0][p1]] = piecetype;
    b->data.board_90[b->data.transform_array[1][p1]] = piecetype;
    b->data.board_180[b->data.transform_array[2][p1]] = piecetype;
    b->data.board_270[b->data.transform_array[3][p1]] = piecetype;

    b->data.board_0[b->data.transform_array[0][p0]] = 0;
    b->data.board_90[b->data.transform_array[1][p0]] = 0;
    b->data.board_180[b->data.transform_array[2][p0]] = 0;
    b->data.board_270[b->data.transform_array[3][p0]] = 0;

    b->data.player_to_play = (PlayerColor)(b->data.player_to_play ^ (WHITE | BLACK)); // flipping player
    // std::cout << "Did last move\n";
    // std::cout << all_boards_to_str(*this);
}

void undo_last_move(Board *b, U16 move)
{

    U8 p0 = getp0(move);
    U8 p1 = getp1(move);
    U8 promo = getpromo(move);

    U8 piecetype = b->data.board_0[p1];
    U8 deadpiece = 0;

    // scan and get piece from coord
    U8 *pieces = (U8 *)(&(b->data));
    for (int i = 0; i < 2 * b->data.n_pieces; i++) //* might have to make it i < 20
    {
        if (pieces[i] == p1)
        {
            pieces[i] = p0;
            break;
        }
    }

    if (last_killed_pieces_idx.back() != -1)
    {
        deadpiece = last_killed_pieces.back(); // updating deadpiece if there is something in vector
        pieces[last_killed_pieces_idx.back()] = p1;
    }

    last_killed_pieces.pop_back();
    last_killed_pieces_idx.pop_back();

    if (promo == PAWN_ROOK)
    {
        piecetype = ((piecetype & (WHITE | BLACK)) ^ ROOK) | PAWN;
    }
    else if (promo == PAWN_BISHOP)
    {
        piecetype = ((piecetype & (WHITE | BLACK)) ^ BISHOP) | PAWN;
    }

    b->data.board_0[b->data.transform_array[0][p1]] = deadpiece;
    b->data.board_90[b->data.transform_array[1][p1]] = deadpiece;
    b->data.board_180[b->data.transform_array[2][p1]] = deadpiece;
    b->data.board_270[b->data.transform_array[3][p1]] = deadpiece;

    b->data.board_0[b->data.transform_array[0][p0]] = piecetype;
    b->data.board_90[b->data.transform_array[1][p0]] = piecetype;
    b->data.board_180[b->data.transform_array[2][p0]] = piecetype;
    b->data.board_270[b->data.transform_array[3][p0]] = piecetype;

    b->data.player_to_play = (PlayerColor)(b->data.player_to_play ^ (WHITE | BLACK)); // flipping player again

    // std::cout << "Undid last move\n";
    // std::cout << all_boards_to_str(*this);
}
// No of white - No of black
float material_check(Board *b)
{
    float val = 0;
    // float weight_arr[12] = {8, 8, 0, 4, 2, 2, 8, 8, 0, 4, 2, 2};
    float p = 2, bi = 4, k = 4, r = 8;
    U8 *pieces = (U8 *)(&(b->data));
    for (int i = 0; i < 2 * b->data.n_pieces; i++)
    {
        int temp = 0;
        if (pieces[i] != DEAD)
        {
            // val += (((int(i >= 6) - int(i < 6))) * weight_arr[i])* (pieces[i] != DEAD);

            U8 piecetype = b->data.board_0[pieces[i]];
            temp += ((piecetype & PAWN) == PAWN) * p;
            temp += ((piecetype & BISHOP) == BISHOP) * bi;
            temp += ((piecetype & ROOK) == ROOK) * r;
            temp += ((piecetype & KNIGHT) == KNIGHT) * k;
            temp *= std::pow(-1, (piecetype & BLACK) == BLACK);
        }
        val += temp;
    }
    return val;
}

float check_condition(Board *b)
{
    //
    float val = 0;
    bool player = (b->data.player_to_play == WHITE); // current player
    if (b->in_check())
    {
        // if white is in check, bad, negative
        val = 10 * std::pow(-1, int(player));
        if (b->get_legal_moves().size() == 0) // if you're checkmated
        {
            val += 500 * std::pow(-1, int(player));
        }
    }
    return val;
}

// Calculates how ranges covered + threats given
float range_check(Board *b)
{
    bool player = (b->data.player_to_play == WHITE);
    float val = 0;
    // float p = 2, bi = 4, r = 6;
    // U8 *pieces = (U8 *)(&(b->data));

    // b->data.player_to_play = (PlayerColor)(b->data.player_to_play ^ (WHITE | BLACK)); // flipping player
    std::unordered_set<U16> moves = b->get_legal_moves();
    // b->data.player_to_play = (PlayerColor)(b->data.player_to_play ^ (WHITE | BLACK)); // flipping player back so as to not interfere with remaining processes

    // moves = (player == 1) ? moves : moves_flip; //White's moves
    // moves_flip = (player == 0) ? moves : moves_flip; //Black's moves

    val += (moves.size()) * pow(-1, player); // Calculates range (always white - black)

    /*Taking union -> Note: we dont need to care about coinciding positions because if there existed coinciding
    positions it implies there was no oponent piece in that square in first place */
    // moves.insert(moves_flip.begin(), moves_flip.end());

    // // Calculating threats
    // for (auto m : moves)
    // {
    //     U8 p1 = getp1(m);
    //     for (int i = 0; i < 12; i++)
    //     {
    //         int temp = 0;
    //         if (pieces[i] == p1)
    //         {
    //             // val += (((int(i >= 6) - int(i < 6))) * weight_arr[i])* (pieces[i] != DEAD);
    //             U8 piecetype = b->data.board_0[pieces[i]];
    //             temp += ((piecetype & PAWN) == PAWN) * p;
    //             temp += ((piecetype & BISHOP) == BISHOP) * bi;
    //             temp += ((piecetype & ROOK) == ROOK) * r;
    //             temp *= std::pow(-1, (piecetype & BLACK) == BLACK);
    //         }
    //         val += temp * threat_weight;
    //     }
    // }
    return val;
}

float pawn_distance(Board *b)
{
    float val = 0;
    // the below are the proper non-overlapping sector regions.
    std::unordered_set<U8> bottom({1, 2, 3, 4, 5, 6, 10, 11, 12, 13});
    std::unordered_set<U8> left({0, 8, 16, 24, 32, 40, 9, 17, 25, 33});
    std::unordered_set<U8> top({48, 49, 50, 51, 52, 53, 41, 42, 43, 44});
    std::unordered_set<U8> right({54, 46, 38, 30, 22, 14, 45, 37, 29, 21});
    U8 corners[4] = {pos(5, 1), pos(1, 1), pos(1, 5), pos(5, 5)};
    U8 *pieces = (U8 *)(&(b->data));

    int ids[4] = {4, 5, 10, 11};
    for (int i = 0; i < 4; i++) // for each piece
    {
        std::bitset<4> piece_arr;
        std::bitset<4> king_arr;
        U8 piece_pos = pieces[ids[i]];
        U8 king_pos = pieces[(i > 5) * 2 + (i <= 5) * 8];
        U8 piecetype = b->data.board_0[piece_pos];

        if (piece_pos == DEAD || (piecetype & PAWN) != PAWN)
            continue;
        piece_arr[0] = bottom.count(piece_pos);
        piece_arr[1] = left.count(piece_pos);
        piece_arr[2] = top.count(piece_pos);
        piece_arr[3] = right.count(piece_pos);
        king_arr[0] = bottom.count(king_pos);
        king_arr[1] = left.count(king_pos);
        king_arr[2] = top.count(king_pos);
        king_arr[3] = right.count(king_pos);
        int sector_dist = (int)std::log2((piece_arr.to_ulong() << 4) / king_arr.to_ulong()) % 4;
        int piece_c = 0;
        int king_c = 0;
        for (int i = 0; i < 4; i++)
        {
            piece_c += (int)piece_arr[i] * corners[i];
            king_c += (int)king_arr[i] * corners[i];
        }
        int piece_dist = std::abs(getx(piece_pos) - getx(piece_c)) + std::abs(gety(piece_pos) - gety(piece_c));
        int king_dist = std::abs(getx(king_pos) - getx(king_c)) + std::abs(gety(king_pos) - gety(king_c));
        int diff = king_dist - piece_dist;
        if (diff < 0)
            diff += 40;
        int total_dist = diff + 4 * sector_dist;
        val -= total_dist * std::pow(-1, (piecetype & BLACK) == BLACK);
    }
    return val;
}

float rook_distance(Board *b)
{
    float val = 0;
    // std::unordered_set<U8> bottom({1, 2, 3, 4, 5, 6, 10, 11, 12, 13});
    // std::unordered_set<U8> left({0, 8, 16, 24, 32, 40, 9, 17, 25, 33});
    // std::unordered_set<U8> top({48, 49, 50, 51, 52, 53, 41, 42, 43, 44});
    // std::unordered_set<U8> right({54, 46, 38, 30, 22, 14, 45, 37, 29, 21});

    std::unordered_set<U8> bottom_r({2, 3, 4, 5, 6, 10, 11, 12, 13, 14});
    std::unordered_set<U8> left_r({0, 1, 8, 9, 16, 24, 32, 17, 25, 33});
    std::unordered_set<U8> top_r({50, 51, 52, 42, 43, 44, 40, 41, 48, 49});
    std::unordered_set<U8> right_r({54, 46, 38, 30, 22, 14, 45, 37, 29, 21});

    std::unordered_set<U8> bottom_l({0, 1, 8, 9, 2, 3, 4, 10, 11, 12});
    std::unordered_set<U8> left_l({40, 41, 48, 49, 16, 24, 32, 17, 25, 33});
    std::unordered_set<U8> top_l({45, 46, 53, 54, 50, 51, 52, 42, 43, 44});
    std::unordered_set<U8> right_l({5, 6, 13, 14, 38, 30, 22, 37, 29, 21});
    U8 *pieces = (U8 *)(&(b->data));

    int ids[8] = {0, 1, 4, 5, 6, 7, 10, 11}; // iterate through rooks and pawns(maybe upgraded)
    for (int i = 0; i < 8; i++)
    {
        std::bitset<4> piece_arr;
        std::bitset<4> king_arr;
        U8 piece_pos = pieces[ids[i]];
        U8 king_pos = pieces[(i > 5) * 2 + (i <= 5) * 8];
        U8 piecetype = b->data.board_0[piece_pos];

        if (piece_pos == DEAD || (piecetype & ROOK) != ROOK)
            continue;
        piece_arr[0] = bottom_r.count(piece_pos);
        piece_arr[1] = left_r.count(piece_pos);
        piece_arr[2] = top_r.count(piece_pos);
        piece_arr[3] = right_r.count(piece_pos);
        king_arr[0] = bottom_l.count(king_pos);
        king_arr[1] = left_l.count(king_pos);
        king_arr[2] = top_l.count(king_pos);
        king_arr[3] = right_l.count(king_pos);
        int sector_dist = 0;
        if ((piece_arr.to_ulong() & piece_arr.to_ulong()) > 0)
            sector_dist = 5;
        else
            sector_dist = (int)std::log2((piece_arr.to_ulong() << 4) / king_arr.to_ulong()) % 4; // 0 to 3

        // std::cout << "rook:" << rook_arr.to_ulong() << "king:" << king_arr.to_ulong() << "\n";

        val -= 8 * sector_dist * std::pow(-1, (piecetype & BLACK) == BLACK);
    }
    return val;
}

float eval_fn(Board *b)
{
    float final_val = 0;
    final_val += 1 * material_check(b);
    final_val += 1 * check_condition(b);
    // final_val += 0.1 * range_check(b);
    // final_val += 0.01 * pawn_distance(b);
    // final_val += 0.02 * rook_distance(b);
    return final_val;
}

void print_state(Board *b, U16 move, int cutoff, int global_cutoff)
{
    std::cout << "Present board state:" << std::endl;
    std::cout << all_boards_to_str(*b) << std::endl;
    std::cout << "Moves taken till now: ";
    for (auto m : moves_taken)
    {
        std::cout << m << " ";
    }
    std::cout << std::endl;
    if (move != 0)
    {
        std::cout << "Next Move to take: " << move_to_str(move) << std::endl;
        std::cout << "Present Depth:" << global_cutoff - cutoff << std::endl;
        std::cout << "\n";
    }
}

void print_moveset(std::unordered_set<U16> moveset)
{
    std::cout << "Moves that can be taken from this node: ";
    for (auto m : moveset)
    {
        std::cout << move_to_str(m) << " ";
    }
    std::cout << std::endl;
}

U16 best_move_obtained = 0;
int minimax_count;
std::chrono::milliseconds get_legal_moves_duration(0);
std::chrono::milliseconds eval_duration(0);

float minimax(Board *b, int cutoff, float alpha, float beta, bool Maximizing, int global_cutoff)
{
    minimax_count++;
    // std::cout << cutoff << std::endl;
    // bool is_sorted = false;
    if (cutoff == 0)
    {
        auto eval_start = std::chrono::high_resolution_clock::now();
        float eval = eval_fn(b);
        auto eval_end = std::chrono::high_resolution_clock::now();
        eval_duration += std::chrono::duration_cast<std::chrono::milliseconds>(eval_end - eval_start);
        return eval;
    }

    std::unordered_set<U16> moveset;
    // bool in_move_hist = false;
    std::string board_string = board_to_str(&(b->data));
    b->data.player_to_play == WHITE ? board_string += "W" : board_string += "B";

    // NOTE: global_cutoff - cutoff gives the present depth of search
    if (move_histories.find(board_string) != move_histories.end())
    { // * Board state already in dictionary
        moveset = std::get<0>(move_histories[board_string]);
        // in_move_hist = true;
        // std::cout << "Board state already in dictionary" << std::endl;
    }
    else
    {
        auto legal_start = std::chrono::high_resolution_clock::now();
        moveset = b->get_legal_moves();
        auto legal_end = std::chrono::high_resolution_clock::now();
        get_legal_moves_duration += std::chrono::duration_cast<std::chrono::milliseconds>(legal_end - legal_start);
        if (global_cutoff - cutoff < max_ids_store_depth)
        { // * Only store the moveset if the depth is less than the max_ids_store_depth (to avoid overflow of memory)
            std::vector<BoardData> board_states;
            move_histories[board_string] = std::make_tuple(moveset, board_states, 0.0f);
        }
    }

    if (moveset.size() == 0)
    {
        return eval_fn(b);
    }

    float best_eval = std::numeric_limits<float>::lowest() * Maximizing + std::numeric_limits<float>::max() * (!Maximizing);
    for (auto m : moveset)
    {
        // BoardData present_data;
        // if (in_move_hist){
        //     present_data = b->data;
        //     b->data = std::get<1>(move_histories[board_string])[ind];
        //     ind++;
        // }
        // else{
        do_move(b, m);
        // if (global_cutoff - cutoff < max_ids_store_depth)
        // {
        //     std::get<1>(move_histories[board_string]).push_back(b->data);
        // }
        // }
        float eval = minimax(b, cutoff - 1, alpha, beta, !Maximizing, global_cutoff);
        // if (in_move_hist){
        //     b->data = present_data;
        // }
        // else{
        undo_last_move(b, m);
        // }
        // if (Maximizing)
        //     best_eval = std::max(best_eval, eval);
        // else
        //     best_eval = std::min(best_eval, eval);
        best_eval = std::max(best_eval, eval) * Maximizing + std::min(best_eval, eval) * (!Maximizing);
        bool prune = ((eval > alpha) && Maximizing) || ((eval < beta) && !Maximizing);
        if (cutoff == global_cutoff && prune)
        {
            best_move_obtained = m;
            std::cout << "New best move chosen:" << move_to_str(m) << ", score: " << eval << std::endl;
        }
        if (Maximizing)
            alpha = std::max(alpha, eval);
        else
            beta = std::min(beta, eval);

        if (alpha >= beta)
            break;
    }
    // if (global_cutoff - cutoff < max_ids_store_depth)
    // {
    //     std::get<2>(move_histories[board_string]) = best_eval;
    // }
    return best_eval;
}

void Engine::find_best_move(const Board &b)
{

    // pick a random move

    auto moveset = b.get_legal_moves();
    if (moveset.size() == 0)
    {
        this->best_move = 0;
    }
    else
    {
        // std::cout << "\n\n\n\n\n\n\nStart find_best_move()\nBoard state:\n";
        // std::cout << all_boards_to_str(b) << "\n Legal moves:" << std::endl;
        // for (auto m : moveset)
        // {
        //     std::cout << move_to_str(m) << " ";
        // }
        // std::cout << std::endl;
        Board *b_copy = new Board(b);
        print_moveset(moveset);

        std::chrono::milliseconds per_move(0);
        BoardType btype = b_copy->data.board_type;
        if (btype == SEVEN_THREE)
        {
            per_move = std::chrono::milliseconds(2000);
        }
        else if (btype == EIGHT_FOUR)
        {
            per_move = std::chrono::milliseconds(3000);
        }
        else
        {
            per_move = std::chrono::milliseconds(4000);
        }
        auto total_start = std::chrono::high_resolution_clock::now();
        std::chrono::milliseconds total_duration(0);
        std::chrono::milliseconds minimax_duration(0);
        std::chrono::milliseconds time_for_next_depth(0);
        int prev_minimax_count = 0;

        move_histories.clear();
        int depth = 0;
        int avg_branch_factor = 0;
        float alpha = 0.8;
        while (true)
        {
            prev_minimax_count = minimax_count;
            minimax_count = 0;
            std::cout << "iterative depth:" << depth << std::endl;
            auto start = std::chrono::high_resolution_clock::now();
            minimax(b_copy, depth, std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max(), b.data.player_to_play == WHITE, depth);
            auto end = std::chrono::high_resolution_clock::now();
            minimax_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - total_start);
            std::cout << "Minimax calls in iterative depth " << depth << ": " << minimax_count << std::endl;
            // std::cout << "Minimax calls per ms: " << (minimax_count) / minimax_duration.count() << std::endl;
            if (depth >= 2){
                avg_branch_factor = (int)(alpha * (minimax_count/prev_minimax_count) + (1-alpha) * avg_branch_factor); // * exponential moving average
            }
            time_for_next_depth = avg_branch_factor * minimax_duration;
            if (total_duration + time_for_next_depth > per_move) // * if the time taken for the next depth is more than the time left for the move, break
            {
                break;
            }
            depth++;
        }
        // minimax(b_copy, global_cutoff, std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max(), b.data.player_to_play == WHITE);
        // auto total_end = std::chrono::high_resolution_clock::now();

        std::cout << "\n\nTotal time taken: " << total_duration.count() << "ms" << std::endl;
        std::cout << "Depth covered: " << depth << std::endl;
        // std::cout << "get_legal_moves() time taken: " << get_legal_moves_duration.count() << "ms" << std::endl;
        // std::cout << "eval_fn() time taken: " << eval_duration.count() << "ms" << std::endl;

        // if (duration.count() < 2000)
        this->best_move = best_move_obtained;
        std::cout << "Best move chosen:" << move_to_str(best_move_obtained) << std::endl;
        delete b_copy;
    }
}
