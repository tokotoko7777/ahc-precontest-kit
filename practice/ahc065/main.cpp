#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <utility>
#include <vector>

using namespace std;

// AHC065 is fixed to a 20 x 20 board.
constexpr int N = 20;
constexpr int BLOCK_N = 10;
constexpr int CELL_COUNT = N * N;
constexpr int RAIL_LENGTH = CELL_COUNT / 2;
constexpr int OPERATION_LIMIT = 100000;

struct Cell {
    int row = 0;
    int column = 0;

    bool operator==(const Cell& other) const {
        return row == other.row && column == other.column;
    }
};

struct Layout {
    // rail[i] is on the large conveyor. pocket[i] is connected to it
    // by the i-th two-cell swap conveyor.
    array<Cell, RAIL_LENGTH> rail{};
    array<Cell, RAIL_LENGTH> pocket{};
};

struct Result {
    bool finished = false;
    vector<pair<int, int>> operations;
};

struct RandomNumber {
    uint64_t state = 1;

    explicit RandomNumber(uint64_t seed) : state(seed) {}

    uint64_t next() {
        state ^= state << 7;
        state ^= state >> 9;
        return state;
    }

    int next_int(int upper_bound) {
        return static_cast<int>(next() % static_cast<uint64_t>(upper_bound));
    }
};

array<array<int, N>, N> initial_box{};

int cell_id(const Cell& cell, int side) {
    return cell.row * side + cell.column;
}

Cell cell_from_id(int id, int side) {
    return Cell{id / side, id % side};
}

bool adjacent(const Cell& first, const Cell& second) {
    return abs(first.row - second.row) + abs(first.column - second.column) == 1;
}

// Makes a Hamiltonian cycle on an even side x side grid. It first walks
// along the top row, snakes through columns 1..side-1, and returns through
// column 0.
vector<int> make_snake_cycle(int side) {
    vector<Cell> cells;
    cells.reserve(side * side);

    for (int column = 0; column < side; ++column) {
        cells.push_back(Cell{0, column});
    }

    int current_row = 0;
    for (int column = side - 1; column >= 1; --column) {
        if (column != side - 1) {
            cells.push_back(Cell{current_row, column});
        }
        if (current_row == 0 || current_row == 1) {
            const int first_row = (current_row == 0 ? 1 : 2);
            for (int row = first_row; row < side; ++row) {
                cells.push_back(Cell{row, column});
            }
            current_row = side - 1;
        } else {
            for (int row = side - 2; row >= 1; --row) {
                cells.push_back(Cell{row, column});
            }
            current_row = 1;
        }
    }

    cells.push_back(Cell{side - 1, 0});
    for (int row = side - 2; row >= 1; --row) {
        cells.push_back(Cell{row, 0});
    }

    vector<int> cycle;
    cycle.reserve(cells.size());
    for (const Cell& cell : cells) {
        cycle.push_back(cell_id(cell, side));
    }
    return cycle;
}

Cell transform_cell(Cell cell, int type, int side) {
    const int last = side - 1;
    switch (type) {
        case 0: return cell;
        case 1: return Cell{cell.row, last - cell.column};
        case 2: return Cell{last - cell.row, cell.column};
        case 3: return Cell{last - cell.row, last - cell.column};
        case 4: return Cell{cell.column, cell.row};
        case 5: return Cell{cell.column, last - cell.row};
        case 6: return Cell{last - cell.column, cell.row};
        default: return Cell{last - cell.column, last - cell.row};
    }
}

vector<int> transform_cycle(const vector<int>& source, int type, int side) {
    vector<int> result;
    result.reserve(source.size());
    for (int id : source) {
        result.push_back(cell_id(transform_cell(cell_from_id(id, side), type, side), side));
    }
    return result;
}

bool valid_cycle(const vector<int>& cycle, int side) {
    if (static_cast<int>(cycle.size()) != side * side) return false;
    vector<char> used(side * side, false);
    for (int index = 0; index < side * side; ++index) {
        const int id = cycle[index];
        if (id < 0 || id >= side * side || used[id]) return false;
        used[id] = true;
        if (!adjacent(cell_from_id(id, side),
                      cell_from_id(cycle[(index + 1) % (side * side)], side))) {
            return false;
        }
    }
    return true;
}

void add_undirected_edge(vector<vector<int>>& graph, int first, int second) {
    graph[first].push_back(second);
    graph[second].push_back(first);
}

void remove_undirected_edge(vector<vector<int>>& graph, int first, int second) {
    auto& first_list = graph[first];
    first_list.erase(find(first_list.begin(), first_list.end(), second));
    auto& second_list = graph[second];
    second_list.erase(find(second_list.begin(), second_list.end(), first));
}

// A random spanning tree on a 5 x 5 coarse grid can be converted into a
// Hamiltonian cycle on the 10 x 10 block grid. Start with one four-cell cycle
// per coarse cell, then merge two cycles along every tree edge.
vector<int> make_tree_cycle(RandomNumber& random) {
    constexpr int COARSE_N = BLOCK_N / 2;
    constexpr int COARSE_COUNT = COARSE_N * COARSE_N;

    struct CoarseEdge {
        int first;
        int second;
    };

    vector<CoarseEdge> edges;
    for (int row = 0; row < COARSE_N; ++row) {
        for (int column = 0; column < COARSE_N; ++column) {
            const int id = row * COARSE_N + column;
            if (row + 1 < COARSE_N) edges.push_back({id, id + COARSE_N});
            if (column + 1 < COARSE_N) edges.push_back({id, id + 1});
        }
    }
    for (int index = static_cast<int>(edges.size()) - 1; index > 0; --index) {
        swap(edges[index], edges[random.next_int(index + 1)]);
    }

    array<int, COARSE_COUNT> parent{};
    for (int id = 0; id < COARSE_COUNT; ++id) parent[id] = id;
    const auto root = [&](int start) {
        int vertex = start;
        while (parent[vertex] != vertex) vertex = parent[vertex];
        return vertex;
    };

    vector<CoarseEdge> tree;
    for (const CoarseEdge edge : edges) {
        const int first_root = root(edge.first);
        const int second_root = root(edge.second);
        if (first_root == second_root) continue;
        parent[first_root] = second_root;
        tree.push_back(edge);
    }

    vector<vector<int>> graph(BLOCK_N * BLOCK_N);
    for (int coarse_row = 0; coarse_row < COARSE_N; ++coarse_row) {
        for (int coarse_column = 0; coarse_column < COARSE_N; ++coarse_column) {
            const int row = 2 * coarse_row;
            const int column = 2 * coarse_column;
            const int top_left = row * BLOCK_N + column;
            const int top_right = top_left + 1;
            const int bottom_left = top_left + BLOCK_N;
            const int bottom_right = bottom_left + 1;
            add_undirected_edge(graph, top_left, top_right);
            add_undirected_edge(graph, top_right, bottom_right);
            add_undirected_edge(graph, bottom_right, bottom_left);
            add_undirected_edge(graph, bottom_left, top_left);
        }
    }

    for (const CoarseEdge edge : tree) {
        const int first_row = edge.first / COARSE_N;
        const int first_column = edge.first % COARSE_N;
        const int second_row = edge.second / COARSE_N;
        const int second_column = edge.second % COARSE_N;

        if (first_row == second_row) {
            const int left_column = min(first_column, second_column);
            const int row = 2 * first_row;
            const int left_top = row * BLOCK_N + 2 * left_column + 1;
            const int left_bottom = left_top + BLOCK_N;
            const int right_top = left_top + 1;
            const int right_bottom = left_bottom + 1;
            remove_undirected_edge(graph, left_top, left_bottom);
            remove_undirected_edge(graph, right_top, right_bottom);
            add_undirected_edge(graph, left_top, right_top);
            add_undirected_edge(graph, left_bottom, right_bottom);
        } else {
            const int top_row = min(first_row, second_row);
            const int column = 2 * first_column;
            const int top_left = (2 * top_row + 1) * BLOCK_N + column;
            const int top_right = top_left + 1;
            const int bottom_left = top_left + BLOCK_N;
            const int bottom_right = bottom_left + 1;
            remove_undirected_edge(graph, top_left, top_right);
            remove_undirected_edge(graph, bottom_left, bottom_right);
            add_undirected_edge(graph, top_left, bottom_left);
            add_undirected_edge(graph, top_right, bottom_right);
        }
    }

    vector<int> cycle;
    cycle.reserve(BLOCK_N * BLOCK_N);
    int previous = -1;
    int current = 0;
    for (int step = 0; step < BLOCK_N * BLOCK_N; ++step) {
        if (graph[current].size() != 2) return {};
        cycle.push_back(current);
        const int next = (graph[current][0] == previous
                              ? graph[current][1]
                              : graph[current][0]);
        previous = current;
        current = next;
    }
    if (current != 0 || !valid_cycle(cycle, BLOCK_N)) return {};
    return cycle;
}

// Sides of a 2 x 2 block.
// 0: up, 1: down, 2: left, 3: right.
int side_from_step(int delta_row, int delta_column) {
    if (delta_row == -1) return 0;
    if (delta_row == 1) return 1;
    if (delta_column == -1) return 2;
    return 3;
}

Cell port_in_block(int side, int bit) {
    if (side == 0) return Cell{0, bit};
    if (side == 1) return Cell{1, bit};
    if (side == 2) return Cell{bit, 0};
    return Cell{bit, 1};
}

// Lifts a 10 x 10 block cycle to a 200-cell conveyor on the 20 x 20 board.
// In every 2 x 2 block, two adjacent cells belong to the rail and the other
// two cells become their pockets.
bool make_layout(const vector<int>& block_cycle, int start_bit, Layout& layout) {
    if (!valid_cycle(block_cycle, BLOCK_N)) return false;

    int bit = start_bit;
    int rail_size = 0;
    bool has_exit = false;

    for (int index = 0; index < BLOCK_N * BLOCK_N; ++index) {
        const Cell previous = cell_from_id(
            block_cycle[(index + BLOCK_N * BLOCK_N - 1) % (BLOCK_N * BLOCK_N)], BLOCK_N);
        const Cell current = cell_from_id(block_cycle[index], BLOCK_N);
        const Cell next = cell_from_id(
            block_cycle[(index + 1) % (BLOCK_N * BLOCK_N)], BLOCK_N);

        const int in_side = side_from_step(previous.row - current.row,
                                           previous.column - current.column);
        const int out_side = side_from_step(next.row - current.row,
                                            next.column - current.column);

        const Cell entry = port_in_block(in_side, bit);
        int next_bit = -1;
        Cell leave;
        for (int candidate_bit = 0; candidate_bit < 2; ++candidate_bit) {
            const Cell candidate = port_in_block(out_side, candidate_bit);
            if (!(candidate == entry) && adjacent(entry, candidate)) {
                next_bit = candidate_bit;
                leave = candidate;
                break;
            }
        }
        if (next_bit == -1) return false;

        const auto to_board = [&](const Cell& local) {
            return Cell{2 * current.row + local.row, 2 * current.column + local.column};
        };
        const Cell first = to_board(entry);
        const Cell second = to_board(leave);
        layout.rail[rail_size] = first;
        layout.rail[rail_size + 1] = second;

        if (entry.row == leave.row) {
            layout.pocket[rail_size] =
                to_board(Cell{1 - entry.row, entry.column});
            layout.pocket[rail_size + 1] =
                to_board(Cell{1 - leave.row, leave.column});
        } else {
            layout.pocket[rail_size] =
                to_board(Cell{entry.row, 1 - entry.column});
            layout.pocket[rail_size + 1] =
                to_board(Cell{leave.row, 1 - leave.column});
        }

        if (first.row == 0 && first.column == N / 2) has_exit = true;
        if (second.row == 0 && second.column == N / 2) has_exit = true;
        rail_size += 2;
        bit = next_bit;
    }

    if (bit != start_bit || !has_exit || rail_size != RAIL_LENGTH) return false;

    array<int, CELL_COUNT> count{};
    for (int index = 0; index < RAIL_LENGTH; ++index) {
        ++count[cell_id(layout.rail[index], N)];
        ++count[cell_id(layout.pocket[index], N)];
        if (!adjacent(layout.rail[index], layout.pocket[index])) return false;
        if (!adjacent(layout.rail[index],
                      layout.rail[(index + 1) % RAIL_LENGTH])) return false;
    }
    for (int value : count) {
        if (value != 1) return false;
    }
    return true;
}

vector<int> layout_key(const Layout& layout) {
    vector<int> forward(RAIL_LENGTH);
    for (int index = 0; index < RAIL_LENGTH; ++index) {
        forward[index] = cell_id(layout.rail[index], N) * CELL_COUNT +
                         cell_id(layout.pocket[index], N);
    }
    return forward;
}

Result simulate(const Layout& layout, int output_direction) {
    Result result;
    array<int, RAIL_LENGTH> rail{};
    array<int, RAIL_LENGTH> pocket{};
    int exit_index = -1;

    for (int index = 0; index < RAIL_LENGTH; ++index) {
        rail[index] = initial_box[layout.rail[index].row][layout.rail[index].column];
        pocket[index] = initial_box[layout.pocket[index].row][layout.pocket[index].column];
        if (layout.rail[index].row == 0 && layout.rail[index].column == N / 2) {
            exit_index = index;
        }
    }
    if (exit_index == -1) return result;

    int next_box = 0;
    const auto remove_box = [&]() {
        if (next_box < CELL_COUNT && rail[exit_index] == next_box) {
            rail[exit_index] = -1;
            ++next_box;
        }
    };

    const auto swap_with_pocket = [&](int index) {
        swap(rail[index], pocket[index]);
        result.operations.push_back({index + 1, 1});
        remove_box();
    };

    const auto rotate_rail = [&](int direction) {
        array<int, RAIL_LENGTH> next{};
        if (direction == 1) {
            for (int index = 0; index < RAIL_LENGTH; ++index) {
                next[(index + 1) % RAIL_LENGTH] = rail[index];
            }
        } else {
            for (int index = 0; index < RAIL_LENGTH; ++index) {
                next[(index + RAIL_LENGTH - 1) % RAIL_LENGTH] = rail[index];
            }
        }
        rail = next;
        result.operations.push_back({0, direction});
        remove_box();
    };

    remove_box();

    while (next_box < CELL_COUNT &&
           static_cast<int>(result.operations.size()) <= OPERATION_LIMIT) {
        const int batch_begin = next_box;
        const int batch_end = min(CELL_COUNT, batch_begin + RAIL_LENGTH);
        const auto is_in_batch = [&](int box) {
            return batch_begin <= box && box < batch_end;
        };

        // First put every box of this batch into a pocket. Boxes already in a
        // pocket stay there. The rail may rotate in either direction.
        while (true) {
            vector<pair<int, int>> targets;  // (box number, rail index)
            vector<int> free_pockets;
            for (int index = 0; index < RAIL_LENGTH; ++index) {
                if (is_in_batch(rail[index])) targets.push_back({rail[index], index});
                if (pocket[index] == -1 || !is_in_batch(pocket[index])) {
                    free_pockets.push_back(index);
                }
            }
            if (targets.empty()) break;
            if (free_pockets.size() < targets.size()) return result;

            sort(targets.begin(), targets.end(),
                 [](const auto& left, const auto& right) {
                     return left.second < right.second;
                 });

            int chosen_direction = 1;
            pair<int, int> chosen_cost{numeric_limits<int>::max(),
                                       numeric_limits<int>::max()};
            vector<int> chosen_slot(CELL_COUNT, -1);

            for (int direction : {1, -1}) {
                vector<int> assigned(CELL_COUNT, -1);
                pair<int, int> direction_cost{numeric_limits<int>::max(),
                                              numeric_limits<int>::max()};

                if (free_pockets.size() == targets.size()) {
                    // Pair the two sorted circular lists. Trying every cyclic
                    // offset avoids an arbitrary disadvantage at index 0.
                    const int count = static_cast<int>(targets.size());
                    for (int shift = 0; shift < count; ++shift) {
                        vector<int> trial(CELL_COUNT, -1);
                        int farthest = 0;
                        int distance_sum = 0;
                        for (int index = 0; index < count; ++index) {
                            const auto& [box, start] = targets[index];
                            const int destination =
                                free_pockets[(index + shift) % count];
                            const int distance = (direction == 1)
                                ? (destination - start + RAIL_LENGTH) % RAIL_LENGTH
                                : (start - destination + RAIL_LENGTH) % RAIL_LENGTH;
                            trial[box] = destination;
                            farthest = max(farthest, distance);
                            distance_sum += distance;
                        }
                        const pair<int, int> cost{farthest, distance_sum};
                        if (cost < direction_cost) {
                            direction_cost = cost;
                            assigned = move(trial);
                        }
                    }
                } else {
                    // This only matters after an unusually early delivery.
                    vector<int> remaining = free_pockets;
                    int farthest = 0;
                    int distance_sum = 0;
                    for (const auto& [box, start] : targets) {
                        int best_index = -1;
                        int best_distance = numeric_limits<int>::max();
                        for (int index = 0;
                             index < static_cast<int>(remaining.size()); ++index) {
                            const int destination = remaining[index];
                            const int distance = (direction == 1)
                                ? (destination - start + RAIL_LENGTH) % RAIL_LENGTH
                                : (start - destination + RAIL_LENGTH) % RAIL_LENGTH;
                            if (distance < best_distance) {
                                best_distance = distance;
                                best_index = index;
                            }
                        }
                        assigned[box] = remaining[best_index];
                        remaining.erase(remaining.begin() + best_index);
                        farthest = max(farthest, best_distance);
                        distance_sum += best_distance;
                    }
                    direction_cost = {farthest, distance_sum};
                }

                if (direction_cost < chosen_cost) {
                    chosen_cost = direction_cost;
                    chosen_direction = direction;
                    chosen_slot = move(assigned);
                }
            }

            array<char, CELL_COUNT> waiting{};
            int waiting_count = 0;
            for (const auto& [box, position] : targets) {
                (void)position;
                waiting[box] = true;
                ++waiting_count;
            }

            for (int step = 0; step <= RAIL_LENGTH && waiting_count > 0; ++step) {
                for (int index = 0; index < RAIL_LENGTH; ++index) {
                    const int box = rail[index];
                    if (box >= 0 && waiting[box] && chosen_slot[box] == index) {
                        swap_with_pocket(index);
                        waiting[box] = false;
                        --waiting_count;
                    }
                }

                // A target can leave from the exit before reaching its pocket.
                array<char, CELL_COUNT> still_on_rail{};
                for (int box : rail) {
                    if (box >= 0) still_on_rail[box] = true;
                }
                for (const auto& [box, position] : targets) {
                    (void)position;
                    if (waiting[box] && !still_on_rail[box]) {
                        waiting[box] = false;
                        --waiting_count;
                    }
                }

                if (waiting_count > 0) rotate_rail(chosen_direction);
            }
            if (waiting_count > 0) return result;
        }

        // During one full rotation, insert all batch boxes into consecutive
        // slots in increasing order.
        array<int, CELL_COUNT> target_slot{};
        target_slot.fill(-1);
        for (int box = batch_begin; box < batch_end; ++box) {
            const int offset = box - batch_begin + 1;
            target_slot[box] =
                (exit_index - output_direction * offset + RAIL_LENGTH) % RAIL_LENGTH;
        }

        for (int turn = 0; turn < RAIL_LENGTH; ++turn) {
            for (int index = 0; index < RAIL_LENGTH; ++index) {
                const int box = pocket[index];
                if (!is_in_batch(box)) continue;
                const int final_position =
                    (index + output_direction * (RAIL_LENGTH - turn) +
                     RAIL_LENGTH * 2) % RAIL_LENGTH;
                if (final_position == target_slot[box]) {
                    swap_with_pocket(index);
                }
            }
            rotate_rail(output_direction);
        }

        // The boxes are now consecutive on the rail, so one more rotation
        // sends them through the exit in order.
        int turns = 0;
        while (next_box < batch_end && turns < RAIL_LENGTH * 2) {
            rotate_rail(output_direction);
            ++turns;
        }
        if (next_box < batch_end) return result;
    }

    result.finished = (next_box == CELL_COUNT &&
                       static_cast<int>(result.operations.size()) <= OPERATION_LIMIT);
    return result;
}

Result solve_baseline(const vector<int>& full_cycle) {
    Result result;
    const int length = static_cast<int>(full_cycle.size());
    vector<int> rail(length);
    int exit_index = -1;
    for (int index = 0; index < length; ++index) {
        const Cell cell = cell_from_id(full_cycle[index], N);
        rail[index] = initial_box[cell.row][cell.column];
        if (cell.row == 0 && cell.column == N / 2) exit_index = index;
    }

    int next_box = 0;
    if (rail[exit_index] == 0) {
        rail[exit_index] = -1;
        ++next_box;
    }

    while (next_box < CELL_COUNT) {
        const int position = static_cast<int>(find(rail.begin(), rail.end(), next_box) - rail.begin());
        const int plus_distance = (exit_index - position + length) % length;
        const int minus_distance = (position - exit_index + length) % length;
        const int direction = (plus_distance <= minus_distance ? 1 : -1);
        const int distance = min(plus_distance, minus_distance);

        for (int step = 0; step < distance; ++step) {
            if (direction == 1) {
                rotate(rail.rbegin(), rail.rbegin() + 1, rail.rend());
            } else {
                rotate(rail.begin(), rail.begin() + 1, rail.end());
            }
            result.operations.push_back({0, direction});
            if (rail[exit_index] == next_box) {
                rail[exit_index] = -1;
                ++next_box;
            }
        }
    }
    result.finished = true;
    return result;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int input_n;
    cin >> input_n;
    if (!cin || input_n != N) return 0;
    for (int row = 0; row < N; ++row) {
        for (int column = 0; column < N; ++column) {
            cin >> initial_box[row][column];
        }
    }

#ifdef BASELINE
    const vector<int> full_cycle = make_snake_cycle(N);
    const Result answer = solve_baseline(full_cycle);
    cout << 1 << '\n';
    cout << CELL_COUNT;
    for (int id : full_cycle) {
        const Cell cell = cell_from_id(id, N);
        cout << ' ' << cell.row << ' ' << cell.column;
    }
    cout << '\n' << answer.operations.size() << '\n';
    for (const auto& [belt, direction] : answer.operations) {
        cout << belt << ' ' << direction << '\n';
    }
#else
    const auto search_start = chrono::steady_clock::now();
    constexpr double SEARCH_TIME_LIMIT_SECONDS = 1.50;
    const vector<int> base_cycle = make_snake_cycle(BLOCK_N);
    vector<vector<int>> cycles;
    for (int type = 0; type < 8; ++type) {
        vector<int> cycle = transform_cycle(base_cycle, type, BLOCK_N);
        cycles.push_back(cycle);
        reverse(cycle.begin(), cycle.end());
        cycles.push_back(move(cycle));
    }

    RandomNumber random(0x6a09e667f3bcc909ULL);
    constexpr int RANDOM_CYCLE_COUNT = 1000;
    for (int attempt = 0; attempt < RANDOM_CYCLE_COUNT; ++attempt) {
        vector<int> cycle = make_tree_cycle(random);
        if (cycle.empty()) continue;
        cycles.push_back(cycle);
        reverse(cycle.begin(), cycle.end());
        cycles.push_back(move(cycle));
    }

    set<vector<int>> seen_layouts;
    Layout best_layout;
    Result best_result;
    int layout_count = 0;

    for (const vector<int>& cycle : cycles) {
        if (layout_count >= 16) {
            const double elapsed = chrono::duration<double>(
                chrono::steady_clock::now() - search_start).count();
            if (elapsed >= SEARCH_TIME_LIMIT_SECONDS) break;
        }
        for (int start_bit = 0; start_bit < 2; ++start_bit) {
            Layout layout;
            if (!make_layout(cycle, start_bit, layout)) continue;
            if (!seen_layouts.insert(layout_key(layout)).second) continue;
            ++layout_count;

            Result candidate = simulate(layout, 1);
            if (!candidate.finished) continue;
            if (!best_result.finished ||
                candidate.operations.size() < best_result.operations.size()) {
                best_layout = layout;
                best_result = move(candidate);
            }
        }
    }

#ifdef LOCAL_REPORT
    cerr << "layouts=" << layout_count
         << " operations=" << best_result.operations.size() << '\n';
#endif

    if (!best_result.finished) return 0;
    cout << 1 + RAIL_LENGTH << '\n';
    cout << RAIL_LENGTH;
    for (const Cell& cell : best_layout.rail) {
        cout << ' ' << cell.row << ' ' << cell.column;
    }
    cout << '\n';
    for (int index = 0; index < RAIL_LENGTH; ++index) {
        cout << 2 << ' '
             << best_layout.rail[index].row << ' '
             << best_layout.rail[index].column << ' '
             << best_layout.pocket[index].row << ' '
             << best_layout.pocket[index].column << '\n';
    }
    cout << best_result.operations.size() << '\n';
    for (const auto& [belt, direction] : best_result.operations) {
        cout << belt << ' ' << direction << '\n';
    }
#endif

    return 0;
}
