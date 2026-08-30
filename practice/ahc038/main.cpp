#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace std;

// Directions are right, down, left, up.  This is also the initial direction
// used by the official judge.
constexpr array<int, 4> DR = {0, 1, 0, -1};
constexpr array<int, 4> DC = {1, 0, -1, 0};

struct Cell {
    int row = 0;
    int column = 0;
};

struct NextEvent {
    int turns = numeric_limits<int>::max();
    int leaf = -1;
    int direction = 0;
    Cell root;
    Cell cell;
};

class Solver {
  public:
    void run() {
        cin >> n_ >> m_ >> vertex_limit_;
        board_.resize(n_);
        target_.resize(n_);
        for (string& row : board_) cin >> row;
        for (string& row : target_) cin >> row;

        make_arm();
        choose_initial_root();
        make_operations();
        print_answer();
    }

  private:
    int n_ = 0;
    int m_ = 0;
    int vertex_limit_ = 0;
    int root_row_ = 0;
    int root_column_ = 0;
    int initial_root_row_ = 0;
    int initial_root_column_ = 0;

    vector<string> board_;
    vector<string> target_;
    vector<int> length_;     // length_[i] is the length of leaf i + 1.
    vector<int> direction_;  // 0:right, 1:down, 2:left, 3:up
    vector<bool> holding_;
    vector<string> operations_;

    bool inside(int row, int column) const {
        return 0 <= row && row < n_ && 0 <= column && column < n_;
    }

    bool is_supply(int row, int column) const {
        return inside(row, column) && board_[row][column] == '1' &&
               target_[row][column] == '0';
    }

    bool is_demand(int row, int column) const {
        return inside(row, column) && board_[row][column] == '0' &&
               target_[row][column] == '1';
    }

    bool finished() const {
        for (int row = 0; row < n_; ++row) {
            for (int column = 0; column < n_; ++column) {
                if (board_[row][column] != target_[row][column]) return false;
            }
        }
        return true;
    }

    void make_arm() {
#ifdef AHC038_BASELINE
        const int leaf_count = 1;
#else
        // A length up to ceil((N-1)/2) can reach every square from at least
        // one of the four directions while the root stays on the board.
        const int always_reachable_length = n_ / 2;
        const int leaf_count = min(vertex_limit_ - 1, always_reachable_length);
#endif
        length_.resize(leaf_count);
        for (int i = 0; i < leaf_count; ++i) length_[i] = i + 1;
        direction_.assign(leaf_count, 0);
        holding_.assign(leaf_count, false);
    }

    void choose_initial_root() {
        // All leaves initially point right.  Prefer a root from which several
        // misplaced takoyaki can be picked immediately.
        int best_count = -1;
        int best_distance = numeric_limits<int>::max();
        for (int row = 0; row < n_; ++row) {
            for (int column = 0; column < n_; ++column) {
                int count = 0;
                int nearest = n_ * 2;
                for (int length : length_) {
                    if (is_supply(row, column + length)) ++count;
                }
                for (int r = 0; r < n_; ++r) {
                    for (int c = 0; c < n_; ++c) {
                        if (is_supply(r, c)) {
                            nearest = min(nearest, abs(row - r) + abs(column - c));
                        }
                    }
                }
                if (count > best_count ||
                    (count == best_count && nearest < best_distance)) {
                    best_count = count;
                    best_distance = nearest;
                    root_row_ = row;
                    root_column_ = column;
                }
            }
        }
        initial_root_row_ = root_row_;
        initial_root_column_ = root_column_;
    }

    static int rotation_distance(int from, int to) {
        const int clockwise = (to - from + 4) % 4;
        return min(clockwise, 4 - clockwise);
    }

    Cell leaf_position(int leaf, int root_row, int root_column,
                       int direction) const {
        const int len = length_[leaf];
        return {root_row + DR[direction] * len,
                root_column + DC[direction] * len};
    }

    NextEvent find_next_event() const {
        NextEvent best;
        for (int leaf = 0; leaf < static_cast<int>(length_.size()); ++leaf) {
            for (int row = 0; row < n_; ++row) {
                for (int column = 0; column < n_; ++column) {
                    const bool usable = holding_[leaf] ? is_demand(row, column)
                                                       : is_supply(row, column);
                    if (!usable) continue;

                    for (int direction = 0; direction < 4; ++direction) {
                        const int goal_root_row =
                            row - DR[direction] * length_[leaf];
                        const int goal_root_column =
                            column - DC[direction] * length_[leaf];
                        if (!inside(goal_root_row, goal_root_column)) continue;

                        const int move_turns = abs(root_row_ - goal_root_row) +
                                               abs(root_column_ - goal_root_column);
                        const int rotate_turns =
                            rotation_distance(direction_[leaf], direction);
                        const int turns = max(move_turns, rotate_turns);

                        // Loading an unused leaf is slightly preferred on a tie:
                        // it increases the number of items transported together.
                        const int load_priority = holding_[leaf] ? 1 : 0;
                        const auto key = tuple(turns, load_priority, move_turns,
                                               row, column, leaf, direction);
                        const auto best_key =
                            tuple(best.turns,
                                  best.leaf < 0 || holding_[best.leaf] ? 1 : 0,
                                  best.leaf < 0
                                      ? numeric_limits<int>::max()
                                      : abs(root_row_ - best.root.row) +
                                            abs(root_column_ - best.root.column),
                                  best.cell.row, best.cell.column, best.leaf,
                                  best.direction);
                        if (best.leaf < 0 || key < best_key) {
                            best.turns = turns;
                            best.leaf = leaf;
                            best.direction = direction;
                            best.root = {goal_root_row, goal_root_column};
                            best.cell = {row, column};
                        }
                    }
                }
            }
        }
        return best;
    }

    int nearest_needed_distance(int leaf, int direction, int next_root_row,
                                int next_root_column) const {
        const Cell position =
            leaf_position(leaf, next_root_row, next_root_column, direction);
        int best = n_ * 4;
        for (int row = 0; row < n_; ++row) {
            for (int column = 0; column < n_; ++column) {
                const bool usable = holding_[leaf] ? is_demand(row, column)
                                                   : is_supply(row, column);
                if (usable) {
                    best = min(best, abs(position.row - row) +
                                         abs(position.column - column));
                }
            }
        }
        return best;
    }

    int greedy_rotation(int leaf, int next_root_row, int next_root_column,
                        const Cell& reserved) const {
        int best_delta = 0;
        int best_immediate = -1;
        int best_distance = numeric_limits<int>::max();

        // Try no rotation first on exact ties, which keeps the output calmer.
        constexpr array<int, 3> DELTA = {0, -1, 1};
        for (int delta : DELTA) {
            const int next_direction = (direction_[leaf] + delta + 4) % 4;
            const Cell position = leaf_position(
                leaf, next_root_row, next_root_column, next_direction);
            const bool is_reserved = position.row == reserved.row &&
                                     position.column == reserved.column;
            const bool immediate =
                !is_reserved &&
                (holding_[leaf] ? is_demand(position.row, position.column)
                                : is_supply(position.row, position.column));
            const int distance = nearest_needed_distance(
                leaf, next_direction, next_root_row, next_root_column);
            if (static_cast<int>(immediate) > best_immediate ||
                (static_cast<int>(immediate) == best_immediate &&
                 distance < best_distance)) {
                best_immediate = static_cast<int>(immediate);
                best_distance = distance;
                best_delta = delta;
            }
        }
        return best_delta;
    }

    void apply_turn(char movement, const vector<int>& rotation_delta,
                    int selected_leaf, const Cell& reserved,
                    bool selected_must_act) {
        if (movement == 'U') --root_row_;
        if (movement == 'D') ++root_row_;
        if (movement == 'L') --root_column_;
        if (movement == 'R') ++root_column_;

        const int vertex_count = static_cast<int>(length_.size()) + 1;
        string command(2 * vertex_count, '.');
        command[0] = movement;

        for (int leaf = 0; leaf < static_cast<int>(length_.size()); ++leaf) {
            const int delta = rotation_delta[leaf];
            if (delta == -1) command[leaf + 1] = 'L';
            if (delta == 1) command[leaf + 1] = 'R';
            direction_[leaf] = (direction_[leaf] + delta + 4) % 4;
        }

        // The official judge processes fingertip actions by vertex number.
        // Distinct arm lengths make their positions distinct, but updating in
        // the same order also makes this local simulation exactly match it.
        for (int leaf = 0; leaf < static_cast<int>(length_.size()); ++leaf) {
            const Cell position = leaf_position(
                leaf, root_row_, root_column_, direction_[leaf]);
            const bool selected = leaf == selected_leaf && selected_must_act;
            const bool reserved_for_other =
                leaf != selected_leaf && position.row == reserved.row &&
                position.column == reserved.column;
            const bool can_act =
                leaf != selected_leaf &&
                inside(position.row, position.column) && !reserved_for_other &&
                (holding_[leaf] ? is_demand(position.row, position.column)
                                : is_supply(position.row, position.column));
            if (selected || can_act) {
                // selected is guaranteed to be at a still-reserved valid cell.
                command[vertex_count + leaf + 1] = 'P';
                if (holding_[leaf]) {
                    board_[position.row][position.column] = '1';
                    holding_[leaf] = false;
                } else {
                    board_[position.row][position.column] = '0';
                    holding_[leaf] = true;
                }
            }
        }
        operations_.push_back(command);
    }

    void travel_to_event(const NextEvent& event) {
        const int step_count = max(
            abs(root_row_ - event.root.row) +
                abs(root_column_ - event.root.column),
            rotation_distance(direction_[event.leaf], event.direction));

        // If the fingertip is already at the right square, grabbing/releasing
        // still consumes one turn.
        const int actual_steps = max(1, step_count);
        for (int step = 0; step < actual_steps; ++step) {
            char movement = '.';
            int next_root_row = root_row_;
            int next_root_column = root_column_;
            if (next_root_row < event.root.row) {
                ++next_root_row;
                movement = 'D';
            } else if (next_root_row > event.root.row) {
                --next_root_row;
                movement = 'U';
            } else if (next_root_column < event.root.column) {
                ++next_root_column;
                movement = 'R';
            } else if (next_root_column > event.root.column) {
                --next_root_column;
                movement = 'L';
            }

            vector<int> rotation_delta(length_.size(), 0);
            const int clockwise =
                (event.direction - direction_[event.leaf] + 4) % 4;
            if (clockwise == 1 || clockwise == 2) {
                rotation_delta[event.leaf] = 1;
            } else if (clockwise == 3) {
                rotation_delta[event.leaf] = -1;
            }

            for (int leaf = 0; leaf < static_cast<int>(length_.size()); ++leaf) {
                if (leaf == event.leaf) continue;
                rotation_delta[leaf] = greedy_rotation(
                    leaf, next_root_row, next_root_column, event.cell);
            }

            const bool is_last = step + 1 == actual_steps;
            apply_turn(movement, rotation_delta, event.leaf, event.cell, is_last);
        }
    }

    void make_operations() {
        while (!finished() && operations_.size() < 100000U) {
            const NextEvent event = find_next_event();
            if (event.leaf < 0) break;  // This should be unreachable.
            travel_to_event(event);
        }
    }

    void print_answer() const {
        const int vertex_count = static_cast<int>(length_.size()) + 1;
        cout << vertex_count << '\n';
        for (int length : length_) cout << 0 << ' ' << length << '\n';
        cout << initial_root_row_ << ' ' << initial_root_column_ << '\n';
        for (const string& operation : operations_) cout << operation << '\n';
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Solver solver;
    solver.run();
    return 0;
}
