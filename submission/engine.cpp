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

int max_ids_store_depth = 8;
std::unordered_map<std::string, std::unordered_set<U16>> move_histories; // dictionary storing the present state (board_0 in its string format) and its corresponding moves, board states after moves, and the score of the board state
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

float check_condition(Board *b, int cutoff)
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
            val += 500 * std::pow(-1, int(player)) * (1 + cutoff);
        }
    }
    return val;
}

float rook_distance(Board *b)
{
    float val = 0;
    U8 *pieces = (U8 *)(&(b->data));
    int ids[12] = {0, 1, 10, 11, 6, 7, 8, 9, 16, 17, 18, 19}; // iterate through rooks and pawns(maybe upgraded)
    for (int i = 0; i < 12; i++)
    {
        U8 piece_pos = pieces[ids[i]];
        U8 piecetype = b->data.board_0[piece_pos]; // whether it is a rook or a pawn
        if (piece_pos == DEAD)                     // || (piecetype & ROOK) != ROOK) // if it is not a rook, skip
            continue;
        U8 opp_king_pos = pieces[(i > 9) * 2 + (i <= 9) * 12]; // pos of opp king according to the rook color

        int piece_sector = b->data.board_mask[piece_pos] - 2; // find rook's orientation in the board.
        // 0 for bottom, 1 for left, 2 for top, 3 for right
        const U8 *inv_transform = b->data.inverse_transform_array[piece_sector];

        piece_pos = inv_transform[piece_pos];       // transform positions to make it to bottom row
        opp_king_pos = inv_transform[opp_king_pos]; // corresponding transform for opp_king
        piece_sector = (b->data.board_mask)[piece_pos];
        int king_sector = (b->data.board_mask)[opp_king_pos];

        int sector_dist = king_sector - piece_sector;                 // values 0-3. higher means king is further to the rook. higher -> bad for white rook, good for black rook
        if (sector_dist == 0 && getx(piece_pos) < getx(opp_king_pos)) // if they're in the same sector but king is behind the piece, then bad for white rook, and equivalent to having even more distance.
            sector_dist = 4;
        val += 8 * (4 - sector_dist) * std::pow(-1, (piecetype & BLACK) == BLACK);
    }
    return val;
}

float eval_fn(Board *b, int cutoff)
{
    float final_val = 0;
    final_val += 1 * material_check(b);
    final_val += 1 * check_condition(b, cutoff);
    // final_val += 0.1 * range_check(b);
    // final_val += 0.01 * pawn_distance(b);
    final_val += 0.02 * rook_distance(b);
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
        float eval = eval_fn(b, cutoff);
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
        moveset = move_histories[board_string];
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
            move_histories[board_string] = moveset;
        }
    }

    // * Under the assumption that this if statement is entered only in a game over move
    if (moveset.size() == 0)
    {
        return eval_fn(b, cutoff);
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
            if (depth >= 6)
            {
                avg_branch_factor = (int)(alpha * (minimax_count / prev_minimax_count) + (1 - alpha) * avg_branch_factor); // * exponential moving average
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
