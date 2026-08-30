#include <bits/stdc++.h>
using namespace std;

constexpr int BOARD_SIZE = 10;
constexpr int CELL_COUNT = BOARD_SIZE * BOARD_SIZE;
using Board = array<unsigned char, CELL_COUNT>;

struct Random {
    mt19937_64 engine;

    explicit Random(uint64_t seed) : engine(seed) {}

    int next_int(int left, int right) {
        assert(left < right);
        return uniform_int_distribution<int>(left, right - 1)(engine);
    }
};

int cell_id(int row, int column) {
    return row * BOARD_SIZE + column;
}

void place_by_rank(Board& board, int rank, int flavor) {
    for (int cell = 0; cell < CELL_COUNT; ++cell) {
        if (board[cell] != 0) continue;
        --rank;
        if (rank == 0) {
            board[cell] = static_cast<unsigned char>(flavor);
            return;
        }
    }
    assert(false);
}

// direction: 0=F(上), 1=B(下), 2=L(左), 3=R(右)
Board tilt_board(const Board& board, int direction) {
    Board result{};

    if (direction == 0 || direction == 1) {
        for (int column = 0; column < BOARD_SIZE; ++column) {
            int write_row = direction == 0 ? 0 : BOARD_SIZE - 1;
            const int step = direction == 0 ? 1 : -1;
            for (int k = 0; k < BOARD_SIZE; ++k) {
                const int row = direction == 0 ? k : BOARD_SIZE - 1 - k;
                const unsigned char candy = board[cell_id(row, column)];
                if (candy == 0) continue;
                result[cell_id(write_row, column)] = candy;
                write_row += step;
            }
        }
    } else {
        for (int row = 0; row < BOARD_SIZE; ++row) {
            int write_column = direction == 2 ? 0 : BOARD_SIZE - 1;
            const int step = direction == 2 ? 1 : -1;
            for (int k = 0; k < BOARD_SIZE; ++k) {
                const int column = direction == 2
                    ? k
                    : BOARD_SIZE - 1 - k;
                const unsigned char candy = board[cell_id(row, column)];
                if (candy == 0) continue;
                result[cell_id(row, write_column)] = candy;
                write_column += step;
            }
        }
    }
    return result;
}

int component_square_sum(const Board& board) {
    array<unsigned char, CELL_COUNT> visited{};
    array<int, CELL_COUNT> stack{};
    static constexpr int DR[4] = {-1, 1, 0, 0};
    static constexpr int DC[4] = {0, 0, -1, 1};
    int answer = 0;

    for (int start = 0; start < CELL_COUNT; ++start) {
        if (board[start] == 0 || visited[start]) continue;
        const unsigned char flavor = board[start];
        int stack_size = 1;
        int component_size = 0;
        stack[0] = start;
        visited[start] = 1;

        while (stack_size > 0) {
            const int cell = stack[--stack_size];
            ++component_size;
            const int row = cell / BOARD_SIZE;
            const int column = cell % BOARD_SIZE;
            for (int direction = 0; direction < 4; ++direction) {
                const int next_row = row + DR[direction];
                const int next_column = column + DC[direction];
                if (
                    next_row < 0 || next_row >= BOARD_SIZE ||
                    next_column < 0 || next_column >= BOARD_SIZE
                ) {
                    continue;
                }
                const int next_cell = cell_id(next_row, next_column);
                if (!visited[next_cell] && board[next_cell] == flavor) {
                    visited[next_cell] = 1;
                    stack[stack_size++] = next_cell;
                }
            }
        }
        answer += component_size * component_size;
    }
    return answer;
}

int partial_board_value(const Board& board) {
    // 3色を上・下・左へ緩く分ける。連結成分二乗和を圧倒的に優先する。
    int edge_preference = 0;
    int same_neighbors = 0;
    for (int row = 0; row < BOARD_SIZE; ++row) {
        for (int column = 0; column < BOARD_SIZE; ++column) {
            const int flavor = board[cell_id(row, column)];
            if (flavor == 1) edge_preference += BOARD_SIZE - 1 - row;
            if (flavor == 2) edge_preference += row;
            if (flavor == 3) edge_preference += BOARD_SIZE - 1 - column;
            if (
                column + 1 < BOARD_SIZE && flavor != 0 &&
                board[cell_id(row, column + 1)] == flavor
            ) {
                ++same_neighbors;
            }
            if (
                row + 1 < BOARD_SIZE && flavor != 0 &&
                board[cell_id(row + 1, column)] == flavor
            ) {
                ++same_neighbors;
            }
        }
    }
    return 100 * component_square_sum(board) +
           3 * edge_preference + 8 * same_neighbors;
}

int quick_policy_direction(const Board& board, int new_flavor) {
    static constexpr int HOME_DIRECTION[4] = {0, 0, 1, 2};
    int best_direction = 0;
    int best_value = numeric_limits<int>::min();
    for (int direction = 0; direction < 4; ++direction) {
        const Board candidate = tilt_board(board, direction);
        int value = partial_board_value(candidate);
        if (direction == HOME_DIRECTION[new_flavor]) value += 40;
        if (value > best_value) {
            best_value = value;
            best_direction = direction;
        }
    }
    return best_direction;
}

int choose_by_rollout(
    const Board& current_board,
    int turn,
    const array<int, CELL_COUNT>& flavors,
    Random& random
) {
    const int remaining = CELL_COUNT - 1 - turn;
    if (remaining <= 0) return 0;

    const int horizon = min(30, remaining);
    const int sample_count = remaining > 50 ? 16 : 24;
    vector<vector<int>> scenario_rank(
        sample_count,
        vector<int>(horizon)
    );
    for (int sample = 0; sample < sample_count; ++sample) {
        for (int step = 0; step < horizon; ++step) {
            const int empty_count = CELL_COUNT - 1 - turn - step;
            scenario_rank[sample][step] =
                random.next_int(1, empty_count + 1);
        }
    }

    array<long long, 4> total_value{};
    for (int first_direction = 0; first_direction < 4; ++first_direction) {
        for (int sample = 0; sample < sample_count; ++sample) {
            Board board = tilt_board(current_board, first_direction);
            for (int step = 0; step < horizon; ++step) {
                const int future_turn = turn + 1 + step;
                place_by_rank(
                    board,
                    scenario_rank[sample][step],
                    flavors[future_turn]
                );
                // 100個目を置いた後には、得点へ影響する傾きはない。
                if (future_turn == CELL_COUNT - 1) break;
                const int direction = quick_policy_direction(
                    board, flavors[future_turn]
                );
                board = tilt_board(board, direction);
            }
            total_value[first_direction] += partial_board_value(board);
        }
    }

    return static_cast<int>(
        max_element(total_value.begin(), total_value.end()) -
        total_value.begin()
    );
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    array<int, CELL_COUNT> flavors{};
    uint64_t input_hash = 1469598103934665603ULL;
    for (int& flavor : flavors) {
        cin >> flavor;
        input_hash ^= static_cast<uint64_t>(flavor);
        input_hash *= 1099511628211ULL;
    }

    Random random(input_hash);
    Board board{};
    static constexpr char DIRECTION_CHAR[4] = {'F', 'B', 'L', 'R'};
#ifdef SIMPLE_POLICY
    static constexpr int HOME_DIRECTION[4] = {0, 0, 1, 2};
#endif

    for (int turn = 0; turn < CELL_COUNT; ++turn) {
        int placement_rank;
        cin >> placement_rank;
        place_by_rank(board, placement_rank, flavors[turn]);

        int direction;
#ifdef SIMPLE_POLICY
        direction = HOME_DIRECTION[flavors[turn]];
#else
        direction = choose_by_rollout(board, turn, flavors, random);
#endif
        board = tilt_board(board, direction);
        cout << DIRECTION_CHAR[direction] << endl;
    }
    return 0;
}
