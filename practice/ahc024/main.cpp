#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <vector>

using namespace std;

// AHC024: preserve the color-adjacency graph while erasing as many cells as
// possible.  The state always stays legal, so the best board can be printed at
// any time.

struct Timer {
    chrono::steady_clock::time_point start = chrono::steady_clock::now();

    double milliseconds() const {
        return chrono::duration<double, milli>(chrono::steady_clock::now() - start)
            .count();
    }
};

struct Random {
    uint64_t state;

    explicit Random(uint64_t seed) : state(seed) {}

    uint32_t next_u32() {
        state ^= state << 7;
        state ^= state >> 9;
        return static_cast<uint32_t>(state);
    }

    int next_int(int upper) {
        return static_cast<int>(next_u32() % static_cast<uint32_t>(upper));
    }

    double next_double() {
        return (next_u32() + 0.5) / 4294967296.0;
    }
};

struct State {
    static constexpr int MAX_N = 50;
    static constexpr int MAX_COLOR = 100;

    int n = 0;
    int m = 0;
    array<array<int, MAX_N>, MAX_N> board{};
    array<int, MAX_COLOR + 1> cell_count{};
    array<array<int, MAX_COLOR + 1>, MAX_COLOR + 1> edge_count{};
    const array<array<char, MAX_COLOR + 1>, MAX_COLOR + 1>* required = nullptr;

    array<int, MAX_N * MAX_N> seen{};
    int seen_stamp = 0;

    int color_at(int row, int column) const {
        if (row < 0 || row >= n || column < 0 || column >= n) return 0;
        return board[row][column];
    }

    static pair<int, int> ordered_pair(int a, int b) {
        if (a > b) swap(a, b);
        return {a, b};
    }

    void add_edge(int a, int b, int value) {
        if (a == b) return;
        auto [small, large] = ordered_pair(a, b);
        if (small < 0 || large > MAX_COLOR) return;
        edge_count[small][large] += value;
    }

    void initialize(
        int board_size, int color_count,
        const array<array<int, MAX_N>, MAX_N>& initial_board,
        const array<array<char, MAX_COLOR + 1>, MAX_COLOR + 1>& required_pairs) {
        n = board_size;
        m = color_count;
        board = initial_board;
        required = &required_pairs;
        cell_count.fill(0);
        for (auto& row : edge_count) row.fill(0);

        for (int row = 0; row < n; ++row) {
            for (int column = 0; column < n; ++column) {
                const int color = board[row][column];
                ++cell_count[color];
                if (row == 0) add_edge(color, 0, 1);
                if (row + 1 == n) add_edge(color, 0, 1);
                if (column == 0) add_edge(color, 0, 1);
                if (column + 1 == n) add_edge(color, 0, 1);
                if (row + 1 < n) add_edge(color, board[row + 1][column], 1);
                if (column + 1 < n) add_edge(color, board[row][column + 1], 1);
            }
        }
    }

    bool touches_color(int row, int column, int color) const {
        static constexpr int DR[4] = {-1, 1, 0, 0};
        static constexpr int DC[4] = {0, 0, -1, 1};
        for (int direction = 0; direction < 4; ++direction) {
            if (color_at(row + DR[direction], column + DC[direction]) == color) {
                return true;
            }
        }
        return false;
    }

    // Removing one cell keeps its old color connected exactly when all
    // same-colored neighbors can still reach one another without that cell.
    bool old_color_stays_connected(int removed_row, int removed_column) {
        const int old_color = board[removed_row][removed_column];
        if (cell_count[old_color] <= 1) return false;

        static constexpr int DR[4] = {-1, 1, 0, 0};
        static constexpr int DC[4] = {0, 0, -1, 1};
        array<int, 4> neighbor_ids{};
        int neighbor_count = 0;
        for (int direction = 0; direction < 4; ++direction) {
            const int next_row = removed_row + DR[direction];
            const int next_column = removed_column + DC[direction];
            if (color_at(next_row, next_column) == old_color) {
                neighbor_ids[neighbor_count++] = next_row * n + next_column;
            }
        }
        if (neighbor_count <= 1) return true;

        ++seen_stamp;
        if (seen_stamp == 0) {
            seen.fill(0);
            ++seen_stamp;
        }
        array<int, MAX_N * MAX_N> stack{};
        int stack_size = 0;
        stack[stack_size++] = neighbor_ids[0];
        seen[neighbor_ids[0]] = seen_stamp;

        while (stack_size > 0) {
            const int id = stack[--stack_size];
            const int row = id / n;
            const int column = id % n;
            for (int direction = 0; direction < 4; ++direction) {
                const int next_row = row + DR[direction];
                const int next_column = column + DC[direction];
                if (next_row < 0 || next_row >= n || next_column < 0 ||
                    next_column >= n) {
                    continue;
                }
                if (next_row == removed_row && next_column == removed_column) continue;
                if (board[next_row][next_column] != old_color) continue;
                const int next_id = next_row * n + next_column;
                if (seen[next_id] == seen_stamp) continue;
                seen[next_id] = seen_stamp;
                stack[stack_size++] = next_id;
            }
        }

        for (int index = 1; index < neighbor_count; ++index) {
            if (seen[neighbor_ids[index]] != seen_stamp) return false;
        }
        return true;
    }

    struct Change {
        array<pair<int, int>, 8> pairs{};
        array<int, 8> differences{};
        int pair_count = 0;
        int different_edge_delta = 0;
    };

    void add_difference(Change& change, int a, int b, int difference) const {
        if (a == b) return;
        const auto pair = ordered_pair(a, b);
        change.different_edge_delta += difference;
        for (int index = 0; index < change.pair_count; ++index) {
            if (change.pairs[index] == pair) {
                change.differences[index] += difference;
                return;
            }
        }
        change.pairs[change.pair_count] = pair;
        change.differences[change.pair_count] = difference;
        ++change.pair_count;
    }

    Change describe_change(int row, int column, int new_color) const {
        static constexpr int DR[4] = {-1, 1, 0, 0};
        static constexpr int DC[4] = {0, 0, -1, 1};
        Change change;
        const int old_color = board[row][column];
        for (int direction = 0; direction < 4; ++direction) {
            const int neighbor = color_at(row + DR[direction], column + DC[direction]);
            add_difference(change, old_color, neighbor, -1);
            add_difference(change, new_color, neighbor, 1);
        }
        return change;
    }

    bool can_change(int row, int column, int new_color, Change& change) {
        const int old_color = board[row][column];
        if (old_color == 0 || old_color == new_color) return false;
        if (!touches_color(row, column, new_color)) return false;
        if (!old_color_stays_connected(row, column)) return false;

        change = describe_change(row, column, new_color);
        for (int index = 0; index < change.pair_count; ++index) {
            const auto [small, large] = change.pairs[index];
            const int after = edge_count[small][large] + change.differences[index];
            if (after < 0) return false;
            if (((*required)[small][large] != 0) != (after > 0)) return false;
        }
        return true;
    }

    void apply_change(int row, int column, int new_color, const Change& change) {
        const int old_color = board[row][column];
        for (int index = 0; index < change.pair_count; ++index) {
            const auto [small, large] = change.pairs[index];
            edge_count[small][large] += change.differences[index];
        }
        --cell_count[old_color];
        ++cell_count[new_color];
        board[row][column] = new_color;
    }

    int zero_count() const {
        return cell_count[0];
    }
};

bool erase_everything_possible(State& state, Random& random, const Timer& timer,
                               double deadline_ms) {
    vector<int> order(state.n * state.n);
    iota(order.begin(), order.end(), 0);
    bool erased_anywhere = false;

    for (int pass = 0;; ++pass) {
        for (int index = static_cast<int>(order.size()) - 1; index > 0; --index) {
            swap(order[index], order[random.next_int(index + 1)]);
        }

        bool erased_this_pass = false;
        for (int index = 0; index < static_cast<int>(order.size()); ++index) {
            if ((index & 255) == 0 && timer.milliseconds() >= deadline_ms) {
                return erased_anywhere;
            }
            const int id = order[index];
            const int row = id / state.n;
            const int column = id % state.n;
            if (state.board[row][column] == 0 ||
                !state.touches_color(row, column, 0)) {
                continue;
            }
            State::Change change;
            if (state.can_change(row, column, 0, change)) {
                state.apply_change(row, column, 0, change);
                erased_this_pass = true;
                erased_anywhere = true;
            }
        }
        if (!erased_this_pass) break;
    }
    return erased_anywhere;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, m;
    cin >> n >> m;
    array<array<int, State::MAX_N>, State::MAX_N> input_board{};
    uint64_t input_hash = 1469598103934665603ULL;
    for (int row = 0; row < n; ++row) {
        for (int column = 0; column < n; ++column) {
            cin >> input_board[row][column];
            input_hash ^= static_cast<uint64_t>(input_board[row][column] +
                                                row * 131 + column * 8191);
            input_hash *= 1099511628211ULL;
        }
    }

    array<array<char, State::MAX_COLOR + 1>, State::MAX_COLOR + 1> required{};
    auto require_pair = [&](int a, int b) {
        if (a == b) return;
        required[a][b] = true;
        required[b][a] = true;
    };
    for (int row = 0; row < n; ++row) {
        for (int column = 0; column < n; ++column) {
            const int color = input_board[row][column];
            if (row == 0 || row + 1 == n || column == 0 || column + 1 == n) {
                require_pair(color, 0);
            }
            if (row + 1 < n) require_pair(color, input_board[row + 1][column]);
            if (column + 1 < n) require_pair(color, input_board[row][column + 1]);
        }
    }

#ifdef LOCAL_SHORT_TIME
    constexpr double DEADLINE_MS = 70.0;
#else
    constexpr double DEADLINE_MS = 1850.0;
#endif
#ifndef AHC024_ERASE_ONLY
    constexpr int PROTECTED_WEIGHT = 7;
    constexpr int ERASE_INTERVAL = 1536;
    constexpr double FIRST_RESTART_MS = 180.0;
    constexpr double RESTART_MS = 180.0;
#endif

    Timer timer;
    Random random(input_hash ^ 0x9e3779b97f4a7c15ULL);
    array<array<int, State::MAX_N>, State::MAX_N> best_board = input_board;
    int best_zeros = 0;

#ifndef AHC024_ERASE_ONLY
    int restart = 0;
#endif
    while (timer.milliseconds() < DEADLINE_MS) {
        State state;
        state.initialize(n, m, input_board, required);
        erase_everything_possible(state, random, timer, DEADLINE_MS);
        if (state.zero_count() > best_zeros) {
            best_zeros = state.zero_count();
            best_board = state.board;
        }

#ifdef AHC024_ERASE_ONLY
        break;
#else
        const double start_ms = timer.milliseconds();
        const double restart_budget = (restart == 0 ? FIRST_RESTART_MS : RESTART_MS);
        const double restart_end = min(DEADLINE_MS, start_ms + restart_budget);
        int attempts_since_erase = 0;
        int search_iterations = 0;
        double current_ms = start_ms;

        while (current_ms < restart_end) {
            if ((search_iterations & 255) == 0) current_ms = timer.milliseconds();
            ++search_iterations;
            int id;
            do {
                id = random.next_int(n * n);
            } while (state.board[id / n][id % n] == 0);
            const int row = id / n;
            const int column = id % n;
            const int old_color = state.board[row][column];

            static constexpr int DR[4] = {-1, 1, 0, 0};
            static constexpr int DC[4] = {0, 0, -1, 1};
            array<int, 4> candidates{};
            int candidate_count = 0;
            for (int direction = 0; direction < 4; ++direction) {
                const int color =
                    state.color_at(row + DR[direction], column + DC[direction]);
                if (color == 0 || color == old_color) continue;
                bool duplicate = false;
                for (int index = 0; index < candidate_count; ++index) {
                    duplicate |= candidates[index] == color;
                }
                if (!duplicate) candidates[candidate_count++] = color;
            }
            if (candidate_count == 0) continue;

            // A non-boundary color can never be erased directly.  Prefer moves
            // that transfer its cells to an originally boundary-adjacent color.
            const int old_is_protected = !required[0][old_color];
            int new_color = -1;
            int gain = 0;
            int best_selection_value = -1000000;
            State::Change change;
            for (int candidate_index = 0; candidate_index < candidate_count;
                 ++candidate_index) {
                const int candidate = candidates[candidate_index];
                State::Change candidate_change;
                if (!state.can_change(row, column, candidate, candidate_change)) continue;
                const int new_is_protected = !required[0][candidate];
                const int candidate_gain =
                    PROTECTED_WEIGHT *
                        (old_is_protected - new_is_protected) -
                    candidate_change.different_edge_delta;
                // Small noise keeps equal-looking borders from following the
                // same deterministic path in every restart.
                const int selection_value =
                    candidate_gain * 16 + random.next_int(16);
                if (selection_value > best_selection_value) {
                    best_selection_value = selection_value;
                    new_color = candidate;
                    gain = candidate_gain;
                    change = candidate_change;
                }
            }
            if (new_color == -1) continue;

            const double progress =
                max(0.0, min(1.0, (current_ms - start_ms) /
                                      max(1.0, restart_end - start_ms)));
            const double temperature = 4.0 * (1.0 - progress) + 0.18 * progress;
            if (gain >= 0 || random.next_double() < exp(gain / temperature)) {
                state.apply_change(row, column, new_color, change);
            }

            ++attempts_since_erase;
            if (attempts_since_erase >= ERASE_INTERVAL) {
                erase_everything_possible(state, random, timer, restart_end);
                attempts_since_erase = 0;
                if (state.zero_count() > best_zeros) {
                    best_zeros = state.zero_count();
                    best_board = state.board;
                }
            }
        }
        erase_everything_possible(state, random, timer, DEADLINE_MS);
        if (state.zero_count() > best_zeros) {
            best_zeros = state.zero_count();
            best_board = state.board;
        }
        ++restart;
#endif
    }

    for (int row = 0; row < n; ++row) {
        for (int column = 0; column < n; ++column) {
            if (column > 0) cout << ' ';
            cout << best_board[row][column];
        }
        cout << '\n';
    }
    return 0;
}
