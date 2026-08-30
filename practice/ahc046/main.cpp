#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std;

// AHC046: Skating with Blocks
//
// The program always keeps a very safe answer made only of one-cell moves.
// In addition, it searches plans which put a block next to a destination and
// use that block as a reusable stopping point for later slides.

constexpr int MAX_CELLS = 400;
constexpr int BLOCK_WORDS = (MAX_CELLS + 63) / 64;
constexpr int INF = 1'000'000'000;
constexpr int BEAM_WIDTH = 128;
constexpr int LOOK_AHEAD = 40;

const array<int, 4> DR = {-1, 1, 0, 0};
const array<int, 4> DC = {0, 0, -1, 1};
const array<char, 4> DIR_CHAR = {'U', 'D', 'L', 'R'};

struct Point {
    int row;
    int col;
};

struct Action {
    char type;
    char direction;
};

struct Blocks {
    array<uint64_t, BLOCK_WORDS> word{};

    bool contains(int cell) const {
        return ((word[cell / 64] >> (cell % 64)) & 1ULL) != 0ULL;
    }

    void add(int cell) {
        word[cell / 64] |= 1ULL << (cell % 64);
    }

    bool operator==(const Blocks& other) const {
        return word == other.word;
    }
};

struct BlocksHash {
    size_t operator()(const Blocks& blocks) const noexcept {
        uint64_t value = 0x9e3779b97f4a7c15ULL;
        for (uint64_t x : blocks.word) {
            x += 0x9e3779b97f4a7c15ULL;
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
            x ^= x >> 31;
            value ^= x + 0x9e3779b97f4a7c15ULL + (value << 6) + (value >> 2);
        }
        return static_cast<size_t>(value);
    }
};

using StopTable = array<array<int, 4>, MAX_CELLS>;

struct BfsResult {
    array<int, MAX_CELLS> distance;
    array<int, MAX_CELLS> parent;
    array<Action, MAX_CELLS> action;
};

struct SearchState {
    Blocks blocks;
    int cost = 0;
    int rank = 0;
    vector<Action> actions;
};

int board_size;
int target_count;
vector<Point> target;
array<bool, MAX_CELLS> is_target{};

int cell_id(int row, int col) {
    return row * board_size + col;
}

bool inside(int row, int col) {
    return 0 <= row && row < board_size && 0 <= col && col < board_size;
}

Point point_of(int cell) {
    return {cell / board_size, cell % board_size};
}

// For each free cell and each direction, precompute where one slide stops.
StopTable make_stop_table(const Blocks& blocks) {
    StopTable stop{};

    for (int row = 0; row < board_size; ++row) {
        int first_free = 0;
        for (int col = 0; col < board_size; ++col) {
            const int cell = cell_id(row, col);
            if (blocks.contains(cell)) {
                first_free = col + 1;
            } else {
                stop[cell][2] = cell_id(row, first_free);
            }
        }

        int last_free = board_size - 1;
        for (int col = board_size - 1; col >= 0; --col) {
            const int cell = cell_id(row, col);
            if (blocks.contains(cell)) {
                last_free = col - 1;
            } else {
                stop[cell][3] = cell_id(row, last_free);
            }
        }
    }

    for (int col = 0; col < board_size; ++col) {
        int first_free = 0;
        for (int row = 0; row < board_size; ++row) {
            const int cell = cell_id(row, col);
            if (blocks.contains(cell)) {
                first_free = row + 1;
            } else {
                stop[cell][0] = cell_id(first_free, col);
            }
        }

        int last_free = board_size - 1;
        for (int row = board_size - 1; row >= 0; --row) {
            const int cell = cell_id(row, col);
            if (blocks.contains(cell)) {
                last_free = row - 1;
            } else {
                stop[cell][1] = cell_id(last_free, col);
            }
        }
    }

    return stop;
}

// BFS on the current board. Each M and each S costs one action.
// A forbidden cell is useful while taking a detour to place a block: landing
// on the next destination too early would advance the judge's target counter.
BfsResult run_bfs(const Blocks& blocks, const StopTable& stop, int start,
                  int forbidden = -1) {
    BfsResult result;
    result.distance.fill(INF);
    result.parent.fill(-1);

    array<int, MAX_CELLS> queue{};
    int head = 0;
    int tail = 0;
    result.distance[start] = 0;
    queue[tail++] = start;

    auto visit = [&](int from, int to, Action action) {
        if (to == forbidden || result.distance[to] != INF) {
            return;
        }
        result.distance[to] = result.distance[from] + 1;
        result.parent[to] = from;
        result.action[to] = action;
        queue[tail++] = to;
    };

    while (head < tail) {
        const int from = queue[head++];
        const Point p = point_of(from);

        for (int direction = 0; direction < 4; ++direction) {
            const int slide_to = stop[from][direction];
            if (slide_to != from) {
                visit(from, slide_to, {'S', DIR_CHAR[direction]});
            }

            const int next_row = p.row + DR[direction];
            const int next_col = p.col + DC[direction];
            if (!inside(next_row, next_col)) {
                continue;
            }
            const int move_to = cell_id(next_row, next_col);
            if (!blocks.contains(move_to)) {
                visit(from, move_to, {'M', DIR_CHAR[direction]});
            }
        }
    }

    return result;
}

// The look-ahead needs only a distance, not the actual path. This lighter BFS
// avoids filling parent arrays and stops as soon as the goal is discovered.
int shortest_distance(const Blocks& blocks, const StopTable& stop, int start,
                      int goal) {
    if (start == goal) {
        return 0;
    }

    array<int, MAX_CELLS> distance;
    distance.fill(-1);
    array<int, MAX_CELLS> queue{};
    int head = 0;
    int tail = 0;
    distance[start] = 0;
    queue[tail++] = start;

    auto visit = [&](int from, int to) {
        if (distance[to] != -1) {
            return false;
        }
        distance[to] = distance[from] + 1;
        queue[tail++] = to;
        return to == goal;
    };

    while (head < tail) {
        const int from = queue[head++];
        const Point p = point_of(from);
        for (int direction = 0; direction < 4; ++direction) {
            const int slide_to = stop[from][direction];
            if (slide_to != from && visit(from, slide_to)) {
                return distance[goal];
            }

            const int next_row = p.row + DR[direction];
            const int next_col = p.col + DC[direction];
            if (!inside(next_row, next_col)) {
                continue;
            }
            const int move_to = cell_id(next_row, next_col);
            if (!blocks.contains(move_to) && visit(from, move_to)) {
                return distance[goal];
            }
        }
    }
    return INF;
}

vector<Action> restore_path(const BfsResult& bfs, int start, int goal) {
    vector<Action> path;
    if (bfs.distance[goal] == INF) {
        return path;
    }

    for (int cell = goal; cell != start; cell = bfs.parent[cell]) {
        path.push_back(bfs.action[cell]);
    }
    reverse(path.begin(), path.end());
    return path;
}

int direction_between(int from, int to) {
    const Point a = point_of(from);
    const Point b = point_of(to);
    for (int direction = 0; direction < 4; ++direction) {
        if (a.row + DR[direction] == b.row &&
            a.col + DC[direction] == b.col) {
            return direction;
        }
    }
    return -1;
}

// Estimate a state's near future assuming that no more blocks are added.
// A short exact look-ahead is cheap on a 20 x 20 board and gives the beam a
// reason to retain blocks which will be reused a few destinations later.
int future_cost(const Blocks& blocks, int current_target) {
    const StopTable stop = make_stop_table(blocks);
    int total = 0;
    const int last = min(target_count - 1, current_target + LOOK_AHEAD);

    for (int index = current_target; index < last; ++index) {
        const int start = cell_id(target[index].row, target[index].col);
        const int goal = cell_id(target[index + 1].row, target[index + 1].col);
        const int distance = shortest_distance(blocks, stop, start, goal);
        if (distance == INF) {
            return INF / 2;
        }
        total += distance;
    }
    return total;
}

vector<Action> move_only_answer() {
    vector<Action> answer;
    Point now = target[0];

    for (int index = 1; index < target_count; ++index) {
        const Point goal = target[index];
        while (now.row < goal.row) {
            answer.push_back({'M', 'D'});
            ++now.row;
        }
        while (now.row > goal.row) {
            answer.push_back({'M', 'U'});
            --now.row;
        }
        while (now.col < goal.col) {
            answer.push_back({'M', 'R'});
            ++now.col;
        }
        while (now.col > goal.col) {
            answer.push_back({'M', 'L'});
            --now.col;
        }
    }
    return answer;
}

vector<Action> beam_search_answer() {
    vector<SearchState> beam(1);

    for (int index = 0; index + 1 < target_count; ++index) {
        const int start = cell_id(target[index].row, target[index].col);
        const int goal = cell_id(target[index + 1].row, target[index + 1].col);
        vector<SearchState> candidates;

        for (const SearchState& state : beam) {
            const StopTable old_stop = make_stop_table(state.blocks);

            // Choice 1: reach the next target without changing the board.
            const BfsResult direct_bfs = run_bfs(state.blocks, old_stop, start);
            if (direct_bfs.distance[goal] != INF) {
                SearchState next = state;
                vector<Action> path = restore_path(direct_bfs, start, goal);
                next.cost += static_cast<int>(path.size());
                next.actions.insert(next.actions.end(), path.begin(), path.end());
                candidates.push_back(move(next));
            }

            // Choice 2: put one block beside the next target first. We must
            // avoid landing on that target during the placement detour.
            const BfsResult prefix_bfs =
                run_bfs(state.blocks, old_stop, start, goal);

            for (int block_direction = 0; block_direction < 4;
                 ++block_direction) {
                const int block_row = target[index + 1].row + DR[block_direction];
                const int block_col = target[index + 1].col + DC[block_direction];
                if (!inside(block_row, block_col)) {
                    continue;
                }
                const int block_cell = cell_id(block_row, block_col);
                if (state.blocks.contains(block_cell) || is_target[block_cell]) {
                    continue;
                }

                Blocks new_blocks = state.blocks;
                new_blocks.add(block_cell);
                const StopTable new_stop = make_stop_table(new_blocks);

                int best_length = INF;
                vector<Action> best_route;

                // Stand on any free neighbor of the new block, alter it, and
                // then take the shortest route to the target.
                for (int side = 0; side < 4; ++side) {
                    const int stand_row = block_row + DR[side];
                    const int stand_col = block_col + DC[side];
                    if (!inside(stand_row, stand_col)) {
                        continue;
                    }
                    const int stand = cell_id(stand_row, stand_col);
                    if (state.blocks.contains(stand) ||
                        prefix_bfs.distance[stand] == INF) {
                        continue;
                    }

                    const int alter_direction = direction_between(stand, block_cell);
                    if (alter_direction == -1) {
                        continue;
                    }

                    const BfsResult suffix_bfs =
                        run_bfs(new_blocks, new_stop, stand);
                    if (suffix_bfs.distance[goal] == INF) {
                        continue;
                    }

                    const int length = prefix_bfs.distance[stand] + 1 +
                                       suffix_bfs.distance[goal];
                    if (length >= best_length) {
                        continue;
                    }

                    best_length = length;
                    best_route = restore_path(prefix_bfs, start, stand);
                    best_route.push_back({'A', DIR_CHAR[alter_direction]});
                    vector<Action> suffix = restore_path(suffix_bfs, stand, goal);
                    best_route.insert(best_route.end(), suffix.begin(), suffix.end());
                }

                if (best_length != INF) {
                    SearchState next = state;
                    next.blocks = new_blocks;
                    next.cost += best_length;
                    next.actions.insert(next.actions.end(), best_route.begin(),
                                        best_route.end());
                    candidates.push_back(move(next));
                }
            }
        }

        // Different histories can produce the same block set. At the same
        // target only the cheapest such history can ever be useful.
        vector<SearchState> unique;
        unordered_map<Blocks, int, BlocksHash> position;
        position.reserve(candidates.size() * 2 + 1);
        for (SearchState& state : candidates) {
            auto [it, inserted] =
                position.emplace(state.blocks, static_cast<int>(unique.size()));
            if (inserted) {
                unique.push_back(move(state));
            } else if (state.cost < unique[it->second].cost) {
                unique[it->second] = move(state);
            }
        }

        for (SearchState& state : unique) {
            const int estimate = future_cost(state.blocks, index + 1);
            state.rank = (estimate >= INF / 2) ? INF : state.cost + estimate;
        }

        sort(unique.begin(), unique.end(), [](const SearchState& a,
                                               const SearchState& b) {
            if (a.rank != b.rank) {
                return a.rank < b.rank;
            }
            return a.cost < b.cost;
        });
        if (static_cast<int>(unique.size()) > BEAM_WIDTH) {
            unique.resize(BEAM_WIDTH);
        }
        beam = move(unique);
    }

    if (beam.empty()) {
        return move_only_answer();
    }
    const auto best = min_element(beam.begin(), beam.end(),
                                  [](const SearchState& a, const SearchState& b) {
                                      return a.cost < b.cost;
                                  });
    return best->actions;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cin >> board_size >> target_count;
    target.resize(target_count);
    for (Point& p : target) {
        cin >> p.row >> p.col;
        is_target[cell_id(p.row, p.col)] = true;
    }

    vector<Action> answer = move_only_answer();

#ifndef MOVE_ONLY_BASELINE
    vector<Action> searched = beam_search_answer();
    if (searched.size() < answer.size()) {
        answer = move(searched);
    }
#endif

    // 39 Manhattan paths use at most 39 * 38 = 1482 actions, so the fallback
    // also proves that this assertion is safe for the official constraints.
    if (answer.size() > static_cast<size_t>(2 * board_size * target_count)) {
        answer = move_only_answer();
    }

    for (const Action action : answer) {
        cout << action.type << ' ' << action.direction << '\n';
    }
    return 0;
}
