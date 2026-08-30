#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <string>
#include <vector>

using namespace std;

constexpr int ROBOT_LIMIT = 10;
constexpr int BUTTON_LIMIT = 10;
constexpr int CELL_LIMIT = 900;

#ifndef ATTEMPTS
#define ATTEMPTS 128
#endif

const array<char, 5> DIRECTION_CHAR{'U', 'D', 'L', 'R', 'S'};
const array<int, 4> OPPOSITE{1, 0, 3, 2};

struct Controller {
    array<array<int, ROBOT_LIMIT>, BUTTON_LIMIT> direction{};
    vector<int> buttons;
    int covered = 0;
    int score = 0;
};

int grid_size;
int robot_count;
int button_count;
int cell_count;
array<int, ROBOT_LIMIT> initial_position{};
array<array<int, 5>, CELL_LIMIT> next_cell{};

struct RandomNumber {
    uint64_t state;

    explicit RandomNumber(uint64_t seed) : state(seed) {}

    uint32_t next() {
        state ^= state << 7;
        state ^= state >> 9;
        return static_cast<uint32_t>(state);
    }

    int next_int(int upper_bound) {
        return static_cast<int>(next() % static_cast<uint32_t>(upper_bound));
    }
};

void evaluate(Controller& controller) {
    array<int, ROBOT_LIMIT> position = initial_position;
    array<char, CELL_LIMIT> visited{};
    int covered = 0;
    for (int robot = 0; robot < robot_count; ++robot) {
        if (!visited[position[robot]]) {
            visited[position[robot]] = true;
            ++covered;
        }
    }

    for (int button : controller.buttons) {
        for (int robot = 0; robot < robot_count; ++robot) {
            const int direction = controller.direction[button][robot];
            position[robot] = next_cell[position[robot]][direction];
            if (!visited[position[robot]]) {
                visited[position[robot]] = true;
                ++covered;
            }
        }
    }

    controller.covered = covered;
    if (covered == cell_count) {
        controller.score = 3 * cell_count - static_cast<int>(controller.buttons.size());
    } else {
        controller.score = covered;
    }
}

Controller make_dfs_baseline() {
    Controller result;
    for (int button = 0; button < button_count; ++button) {
        for (int robot = 0; robot < robot_count; ++robot) {
            result.direction[button][robot] = (button < 4 ? button : 4);
        }
    }

    array<char, CELL_LIMIT> visited{};
    const auto dfs = [&](const auto& self, int cell) -> void {
        visited[cell] = true;
        for (int direction = 0; direction < 4; ++direction) {
            const int to = next_cell[cell][direction];
            if (to == cell || visited[to]) continue;
            result.buttons.push_back(direction);
            self(self, to);
            result.buttons.push_back(OPPOSITE[direction]);
        }
    };
    dfs(dfs, initial_position[0]);
    evaluate(result);
    return result;
}

array<array<int, ROBOT_LIMIT>, BUTTON_LIMIT> make_balanced_mapping(uint64_t seed) {
    array<array<int, ROBOT_LIMIT>, BUTTON_LIMIT> mapping{};
    RandomNumber random(seed);

    for (int robot = 0; robot < robot_count; ++robot) {
        // Every robot gets every direction at least twice among the 10 buttons.
        array<int, BUTTON_LIMIT> choices{};
        for (int i = 0; i < 8; ++i) choices[i] = i % 4;
        choices[8] = random.next_int(4);
        choices[9] = random.next_int(4);
        for (int i = button_count - 1; i > 0; --i) {
            swap(choices[i], choices[random.next_int(i + 1)]);
        }
        for (int button = 0; button < button_count; ++button) {
            mapping[button][robot] = choices[button];
        }
    }
    return mapping;
}

struct SequenceSearch {
    const array<array<int, ROBOT_LIMIT>, BUTTON_LIMIT>& mapping;
    const array<char, CELL_LIMIT>& visited;
    const array<int, CELL_LIMIT>& distance_to_unvisited;
    int horizon;
    array<int, 3> current_sequence{};
    array<int, 3> best_sequence{};
    array<char, CELL_LIMIT> locally_seen{};
    array<int, CELL_LIMIT> uniqueness_stamp{};
    int current_stamp = 0;
    int best_new_cells = -1;
    int best_early_gain = -1;
    int best_distance_sum = numeric_limits<int>::max();
    int best_unique_positions = -1;

    void search(int depth, const array<int, ROBOT_LIMIT>& position,
                int new_cells, int early_gain) {
        if (depth == horizon) {
            int distance_sum = 0;
            int unique_positions = 0;
            ++current_stamp;
            for (int robot = 0; robot < robot_count; ++robot) {
                distance_sum += distance_to_unvisited[position[robot]];
                if (uniqueness_stamp[position[robot]] != current_stamp) {
                    uniqueness_stamp[position[robot]] = current_stamp;
                    ++unique_positions;
                }
            }

            bool better = false;
            if (new_cells != best_new_cells) {
                better = new_cells > best_new_cells;
            } else if (early_gain != best_early_gain) {
                better = early_gain > best_early_gain;
            } else if (distance_sum != best_distance_sum) {
                better = distance_sum < best_distance_sum;
            } else if (unique_positions != best_unique_positions) {
                better = unique_positions > best_unique_positions;
            } else {
                better = current_sequence < best_sequence;
            }
            if (better) {
                best_new_cells = new_cells;
                best_early_gain = early_gain;
                best_distance_sum = distance_sum;
                best_unique_positions = unique_positions;
                best_sequence = current_sequence;
            }
            return;
        }

        for (int button = 0; button < button_count; ++button) {
            array<int, ROBOT_LIMIT> next_position{};
            array<int, ROBOT_LIMIT> newly_marked{};
            int marked_count = 0;
            int new_at_this_step = 0;
            ++current_stamp;

            for (int robot = 0; robot < robot_count; ++robot) {
                const int direction = mapping[button][robot];
                const int to = next_cell[position[robot]][direction];
                next_position[robot] = to;
                if (!visited[to] && uniqueness_stamp[to] != current_stamp) {
                    uniqueness_stamp[to] = current_stamp;
                    ++new_at_this_step;
                }
                if (!visited[to] && !locally_seen[to]) {
                    locally_seen[to] = true;
                    newly_marked[marked_count++] = to;
                }
            }

            current_sequence[depth] = button;
            search(depth + 1, next_position, new_cells + marked_count,
                   early_gain + new_at_this_step * (horizon - depth));
            for (int i = 0; i < marked_count; ++i) {
                locally_seen[newly_marked[i]] = false;
            }
        }
    }
};

Controller make_greedy_controller(
    const array<array<int, ROBOT_LIMIT>, BUTTON_LIMIT>& mapping, int horizon,
    int committed_steps) {
    Controller result;
    result.direction = mapping;

    array<int, ROBOT_LIMIT> position = initial_position;
    array<char, CELL_LIMIT> visited{};
    int covered = 0;
    for (int robot = 0; robot < robot_count; ++robot) {
        if (!visited[position[robot]]) {
            visited[position[robot]] = true;
            ++covered;
        }
    }

    const int action_limit = 2 * cell_count;
    while (covered < cell_count && static_cast<int>(result.buttons.size()) < action_limit) {
        array<int, CELL_LIMIT> distance_to_unvisited{};
        distance_to_unvisited.fill(cell_count + 1);
        queue<int> que;
        for (int cell = 0; cell < cell_count; ++cell) {
            if (!visited[cell]) {
                distance_to_unvisited[cell] = 0;
                que.push(cell);
            }
        }
        while (!que.empty()) {
            const int cell = que.front();
            que.pop();
            for (int direction = 0; direction < 4; ++direction) {
                const int to = next_cell[cell][direction];
                if (to == cell || distance_to_unvisited[to] <= distance_to_unvisited[cell] + 1) {
                    continue;
                }
                distance_to_unvisited[to] = distance_to_unvisited[cell] + 1;
                que.push(to);
            }
        }

        const int remaining_turns = action_limit - static_cast<int>(result.buttons.size());
        const int actual_horizon = min(horizon, remaining_turns);
        SequenceSearch searcher{mapping, visited, distance_to_unvisited, actual_horizon};
        searcher.search(0, position, 0, 0);

        const int steps_to_use = min(actual_horizon, committed_steps);
        for (int depth = 0; depth < steps_to_use; ++depth) {
            const int button = searcher.best_sequence[depth];
            result.buttons.push_back(button);
            for (int robot = 0; robot < robot_count; ++robot) {
                const int direction = mapping[button][robot];
                position[robot] = next_cell[position[robot]][direction];
                if (!visited[position[robot]]) {
                    visited[position[robot]] = true;
                    ++covered;
                }
            }
            if (covered == cell_count) break;
        }
    }

    evaluate(result);
    return result;
}

void remove_unnecessary_buttons(Controller& controller) {
    // Removing an action changes every later position, so validate the whole
    // sequence each time.  The sequence is only a few hundred actions long.
    for (int index = static_cast<int>(controller.buttons.size()) - 1; index >= 0; --index) {
        Controller candidate = controller;
        candidate.buttons.erase(candidate.buttons.begin() + index);
        evaluate(candidate);
        if (candidate.covered == cell_count) controller = move(candidate);
    }

    // A direct shortcut can work even when neither action can simply be
    // deleted.  Replace two neighboring presses by every possible button.
    for (int index = static_cast<int>(controller.buttons.size()) - 2; index >= 0; --index) {
        for (int button = 0; button < button_count; ++button) {
            Controller candidate = controller;
            candidate.buttons.erase(candidate.buttons.begin() + index,
                                    candidate.buttons.begin() + index + 2);
            candidate.buttons.insert(candidate.buttons.begin() + index, button);
            evaluate(candidate);
            if (candidate.covered == cell_count) {
                controller = move(candidate);
                break;
            }
        }
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cin >> grid_size >> robot_count >> button_count;
    cell_count = grid_size * grid_size;
    for (int robot = 0; robot < robot_count; ++robot) {
        int row, col;
        cin >> row >> col;
        initial_position[robot] = row * grid_size + col;
    }

    vector<string> vertical_wall(grid_size);
    vector<string> horizontal_wall(grid_size - 1);
    for (string& row : vertical_wall) cin >> row;
    for (string& row : horizontal_wall) cin >> row;

    const array<int, 4> dr{-1, 1, 0, 0};
    const array<int, 4> dc{0, 0, -1, 1};
    for (int row = 0; row < grid_size; ++row) {
        for (int col = 0; col < grid_size; ++col) {
            const int cell = row * grid_size + col;
            for (int direction = 0; direction < 4; ++direction) {
                const int next_row = row + dr[direction];
                const int next_col = col + dc[direction];
                bool movable = 0 <= next_row && next_row < grid_size &&
                               0 <= next_col && next_col < grid_size;
                if (movable && direction >= 2) {
                    movable = vertical_wall[row][min(col, next_col)] == '0';
                }
                if (movable && direction < 2) {
                    movable = horizontal_wall[min(row, next_row)][col] == '0';
                }
                next_cell[cell][direction] = movable ? next_row * grid_size + next_col : cell;
            }
            next_cell[cell][4] = cell;
        }
    }

    Controller best = make_dfs_baseline();

#ifndef BASELINE
    // The mappings are deterministic despite using a small pseudo-random
    // generator.  Trying both short-sighted and three-step construction helps
    // because their strengths differ near the end of the coverage.
    for (int attempt = 0; attempt < ATTEMPTS; ++attempt) {
        const uint64_t seed = 0x9e3779b97f4a7c15ULL * static_cast<uint64_t>(attempt + 1);
        const auto mapping = make_balanced_mapping(seed);
        for (int horizon : {1, 3}) {
            Controller candidate = make_greedy_controller(mapping, horizon, horizon);
            if (candidate.score > best.score) best = move(candidate);
        }
        Controller lookahead_candidate = make_greedy_controller(mapping, 2, 1);
        if (lookahead_candidate.score > best.score) best = move(lookahead_candidate);
    }
    remove_unnecessary_buttons(best);
#endif

    for (int button = 0; button < button_count; ++button) {
        for (int robot = 0; robot < robot_count; ++robot) {
            cout << DIRECTION_CHAR[best.direction[button][robot]];
            cout << (robot + 1 == robot_count ? '\n' : ' ');
        }
    }
    for (int button : best.buttons) cout << button << '\n';
    return 0;
}
