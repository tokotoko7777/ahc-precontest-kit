#include <bits/stdc++.h>
using namespace std;

// Define AHC042_HINT_BASELINE to use the official-hint round trip separately
// for every Oni.

struct Random {
    uint64_t state;

    explicit Random(uint64_t seed) : state(seed) {}

    uint64_t next() {
        state += 0x9e3779b97f4a7c15ULL;
        uint64_t value = state;
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }

    int next_int(int upper) {
        return static_cast<int>(next() % static_cast<uint64_t>(upper));
    }

    double next_double() {
        return static_cast<double>(next() >> 11)
             * (1.0 / 9007199254740992.0);
    }
};

struct Choice {
    int group;
    int depth;
};

struct Move {
    char direction;
    int line;
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int N;
    cin >> N;
    vector<string> initial_board(N);
    for (string& row : initial_board) cin >> row;

    vector<pair<int, int>> oni;
    for (int row = 0; row < N; ++row) {
        for (int column = 0; column < N; ++column) {
            if (initial_board[row][column] == 'x') {
                oni.push_back({row, column});
            }
        }
    }

    const int oni_count = static_cast<int>(oni.size());
    const int group_count = 4 * N;
    vector<vector<Choice>> choices(oni_count);
    vector<vector<int>> depth(oni_count, vector<int>(group_count, -1));

    auto add_choice = [&](int index, int group, int move_count) {
        choices[index].push_back({group, move_count});
        depth[index][group] = move_count;
    };

    for (int index = 0; index < oni_count; ++index) {
        const auto [row, column] = oni[index];

        bool safe = true;
        for (int c = 0; c < column; ++c) {
            if (initial_board[row][c] == 'o') safe = false;
        }
        if (safe) add_choice(index, row, column + 1);  // left

        safe = true;
        for (int c = column + 1; c < N; ++c) {
            if (initial_board[row][c] == 'o') safe = false;
        }
        if (safe) add_choice(index, N + row, N - column);  // right

        safe = true;
        for (int r = 0; r < row; ++r) {
            if (initial_board[r][column] == 'o') safe = false;
        }
        if (safe) add_choice(index, 2 * N + column, row + 1);  // up

        safe = true;
        for (int r = row + 1; r < N; ++r) {
            if (initial_board[r][column] == 'o') safe = false;
        }
        if (safe) add_choice(index, 3 * N + column, N - row);  // down

        assert(!choices[index].empty());
    }

    auto assignment_cost = [&](const vector<int>& assignment) {
        vector<int> maximum_depth(group_count, 0);
        for (int index = 0; index < oni_count; ++index) {
            const int group = assignment[index];
            maximum_depth[group] =
                max(maximum_depth[group], depth[index][group]);
        }
        return 2 * accumulate(maximum_depth.begin(), maximum_depth.end(), 0);
    };

    auto greedy_assignment = [&](vector<int> order) {
        vector<int> assignment(oni_count, -1);
        vector<int> maximum_depth(group_count, 0);
        for (int index : order) {
            int best_group = -1;
            int best_increase = numeric_limits<int>::max();
            int best_depth = numeric_limits<int>::max();
            for (const Choice& choice : choices[index]) {
                const int increase =
                    max(0, choice.depth - maximum_depth[choice.group]);
                if (increase < best_increase
                    || (increase == best_increase
                        && choice.depth < best_depth)) {
                    best_increase = increase;
                    best_depth = choice.depth;
                    best_group = choice.group;
                }
            }
            assignment[index] = best_group;
            maximum_depth[best_group] =
                max(maximum_depth[best_group], depth[index][best_group]);
        }
        return assignment;
    };

    vector<int> natural_order(oni_count);
    iota(natural_order.begin(), natural_order.end(), 0);
    vector<int> best_assignment = greedy_assignment(natural_order);
    int best_cost = assignment_cost(best_assignment);

    Random construction_random(0x42004200ULL);
    for (int trial = 0; trial < 200; ++trial) {
        vector<int> order = natural_order;
        for (int index = oni_count - 1; index > 0; --index) {
            swap(order[index], order[construction_random.next_int(index + 1)]);
        }
        vector<int> candidate = greedy_assignment(order);
        const int cost = assignment_cost(candidate);
        if (cost < best_cost) {
            best_cost = cost;
            best_assignment = move(candidate);
        }
    }

    auto anneal = [&](vector<int> initial_assignment, int iterations,
                      uint64_t seed) {
        Random random(seed);
        vector<int> current = move(initial_assignment);
        vector<int> best = current;
        int current_cost = assignment_cost(current);
        int local_best_cost = current_cost;

        for (int iteration = 0; iteration < iterations; ++iteration) {
            vector<int> candidate = current;
            const int move_type = random.next_int(10);

            if (move_type < 7) {
                const int index = random.next_int(oni_count);
                const vector<Choice>& options = choices[index];
                candidate[index] =
                    options[random.next_int(static_cast<int>(options.size()))]
                        .group;
            } else if (move_type < 9) {
                const int first = random.next_int(oni_count);
                int second = random.next_int(oni_count - 1);
                if (second >= first) ++second;
                const int first_group = candidate[first];
                const int second_group = candidate[second];
                if (depth[first][second_group] < 0
                    || depth[second][first_group] < 0) {
                    continue;
                }
                candidate[first] = second_group;
                candidate[second] = first_group;
            } else {
                const int pivot = random.next_int(oni_count);
                const vector<Choice>& options = choices[pivot];
                const int group =
                    options[random.next_int(static_cast<int>(options.size()))]
                        .group;
                for (int index = 0; index < oni_count; ++index) {
                    if (depth[index][group] >= 0 && random.next_int(3) == 0) {
                        candidate[index] = group;
                    }
                }
            }

            const int candidate_cost = assignment_cost(candidate);
            const double progress = static_cast<double>(iteration)
                                  / static_cast<double>(iterations);
            const double temperature = 12.0 * pow(0.05 / 12.0, progress);
            const bool accept = candidate_cost <= current_cost
                || random.next_double()
                   < exp(static_cast<double>(current_cost - candidate_cost)
                         / temperature);
            if (accept) {
                current = move(candidate);
                current_cost = candidate_cost;
                if (current_cost < local_best_cost) {
                    local_best_cost = current_cost;
                    best = current;
                }
            }
        }
        return best;
    };

    vector<int> first_search =
        anneal(best_assignment, 500000, 0x42111111ULL);
    vector<int> second_search =
        anneal(best_assignment, 500000, 0x42222222ULL);
    if (assignment_cost(first_search) < best_cost) {
        best_assignment = move(first_search);
        best_cost = assignment_cost(best_assignment);
    }
    if (assignment_cost(second_search) < best_cost) {
        best_assignment = move(second_search);
        best_cost = assignment_cost(best_assignment);
    }

    vector<vector<uint64_t>> cover_mask(
        group_count, vector<uint64_t>(N + 1, 0));
    for (int group = 0; group < group_count; ++group) {
        for (int move_count = 1; move_count <= N; ++move_count) {
            uint64_t mask = 0;
            for (int index = 0; index < oni_count; ++index) {
                if (depth[index][group] >= 0
                    && depth[index][group] <= move_count) {
                    mask |= 1ULL << index;
                }
            }
            cover_mask[group][move_count] = mask;
        }
    }
    const uint64_t full_mask = (1ULL << oni_count) - 1;

    auto selected_depths = [&](const vector<int>& assignment) {
        vector<int> result(group_count, 0);
        for (int index = 0; index < oni_count; ++index) {
            const int group = assignment[index];
            result[group] = max(result[group], depth[index][group]);
        }
        return result;
    };

    auto selection_cost = [&](const vector<int>& selected) {
        return 2 * accumulate(selected.begin(), selected.end(), 0);
    };

    auto covered_by = [&](const vector<int>& selected, int ignored_group) {
        uint64_t result = 0;
        for (int group = 0; group < group_count; ++group) {
            if (group != ignored_group && selected[group] > 0) {
                result |= cover_mask[group][selected[group]];
            }
        }
        return result;
    };

    auto normalize = [&](vector<int> selected) {
        bool changed = true;
        while (changed) {
            changed = false;
            for (int group = 0; group < group_count; ++group) {
                if (selected[group] == 0) continue;
                const uint64_t other = covered_by(selected, group);
                for (int move_count = 0;
                     move_count < selected[group]; ++move_count) {
                    const uint64_t group_mask = move_count == 0
                        ? 0 : cover_mask[group][move_count];
                    if ((other | group_mask) == full_mask) {
                        selected[group] = move_count;
                        changed = true;
                        break;
                    }
                }
            }
        }
        return selected;
    };

    vector<int> selected = normalize(selected_depths(best_assignment));

    // Adding one bundle can make several old bundles redundant.  Try every
    // possible addition, normalize again, and keep strict improvements.
    bool improved = true;
    while (improved) {
        improved = false;
        vector<int> best_selected = selected;
        int selected_cost = selection_cost(selected);
        for (int group = 0; group < group_count; ++group) {
            for (int move_count = selected[group] + 1;
                 move_count <= N; ++move_count) {
                if (cover_mask[group][move_count] == 0) continue;
                vector<int> candidate = selected;
                candidate[group] = move_count;
                candidate = normalize(move(candidate));
                const int cost = selection_cost(candidate);
                if (cost < selected_cost) {
                    selected_cost = cost;
                    best_selected = move(candidate);
                    improved = true;
                }
            }
        }
        selected = move(best_selected);
    }

    auto append_round_trip = [&](vector<Move>& answer, int group,
                                 int move_count) {
        const int type = group / N;
        const int line = group % N;
        const array<char, 4> outward{'L', 'R', 'U', 'D'};
        const array<char, 4> backward{'R', 'L', 'D', 'U'};
        for (int repeat = 0; repeat < move_count; ++repeat) {
            answer.push_back({outward[type], line});
        }
        for (int repeat = 0; repeat < move_count; ++repeat) {
            answer.push_back({backward[type], line});
        }
    };

    vector<Move> answer;
    vector<int> active_groups;
#ifdef AHC042_HINT_BASELINE
    for (int index = 0; index < oni_count; ++index) {
        Choice best = choices[index][0];
        for (const Choice& choice : choices[index]) {
            if (choice.depth < best.depth) best = choice;
        }
        append_round_trip(answer, best.group, best.depth);
    }
#else
    for (int group = 0; group < group_count; ++group) {
        if (selected[group] > 0) {
            active_groups.push_back(group);
            append_round_trip(answer, group, selected[group]);
        }
    }
#endif

    auto simulate = [&](const vector<Move>& moves) {
        vector<string> board = initial_board;
        bool removed_fuku = false;
        for (const Move& move : moves) {
            const int line = move.line;
            if (move.direction == 'L') {
                if (board[line][0] == 'o') removed_fuku = true;
                for (int column = 0; column + 1 < N; ++column) {
                    board[line][column] = board[line][column + 1];
                }
                board[line][N - 1] = '.';
            } else if (move.direction == 'R') {
                if (board[line][N - 1] == 'o') removed_fuku = true;
                for (int column = N - 1; column > 0; --column) {
                    board[line][column] = board[line][column - 1];
                }
                board[line][0] = '.';
            } else if (move.direction == 'U') {
                if (board[0][line] == 'o') removed_fuku = true;
                for (int row = 0; row + 1 < N; ++row) {
                    board[row][line] = board[row + 1][line];
                }
                board[N - 1][line] = '.';
            } else {
                if (board[N - 1][line] == 'o') removed_fuku = true;
                for (int row = N - 1; row > 0; --row) {
                    board[row][line] = board[row - 1][line];
                }
                board[0][line] = '.';
            }
        }
        int remaining_oni = 0;
        for (const string& row : board) {
            remaining_oni += static_cast<int>(count(row.begin(), row.end(), 'x'));
        }
        return !removed_fuku && remaining_oni == 0;
    };

#if !defined(AHC042_HINT_BASELINE) && !defined(AHC042_GROUP_ONLY)
    auto reduce_operations = [&](vector<Move> sequence, uint64_t seed) {
        Random random(seed);
        assert(simulate(sequence));
        for (int iteration = 0; iteration < 30000; ++iteration) {
            if (sequence.empty()) break;
            vector<Move> candidate = sequence;
            const int move_type = random.next_int(5);
            if (move_type == 0 || candidate.size() == 1) {
                const int position =
                    random.next_int(static_cast<int>(candidate.size()));
                candidate.erase(candidate.begin() + position);
            } else if (move_type == 1) {
                const int first =
                    random.next_int(static_cast<int>(candidate.size()));
                const int maximum_length = min(
                    8, static_cast<int>(candidate.size()) - first);
                const int length = 1 + random.next_int(maximum_length);
                candidate.erase(candidate.begin() + first,
                                candidate.begin() + first + length);
            } else if (move_type == 2) {
                int first =
                    random.next_int(static_cast<int>(candidate.size()));
                int second = random.next_int(
                    static_cast<int>(candidate.size()) - 1);
                if (second >= first) ++second;
                if (first > second) swap(first, second);
                candidate.erase(candidate.begin() + second);
                candidate.erase(candidate.begin() + first);
            } else if (move_type == 3) {
                const int first =
                    random.next_int(static_cast<int>(candidate.size()));
                int second = random.next_int(
                    static_cast<int>(candidate.size()) - 1);
                if (second >= first) ++second;
                swap(candidate[first], candidate[second]);
            } else {
                const int first =
                    random.next_int(static_cast<int>(candidate.size()));
                int second = random.next_int(
                    static_cast<int>(candidate.size()) - 1);
                if (second >= first) ++second;
                const Move moved = candidate[first];
                candidate.erase(candidate.begin() + first);
                candidate.insert(candidate.begin() + second, moved);
            }
            if (simulate(candidate)) sequence = move(candidate);
        }
        return sequence;
    };

    answer = reduce_operations(move(answer), 0x42444444ULL);
    Random order_random(0x42555555ULL);
    for (int trial = 0; trial < 12; ++trial) {
        vector<int> order = active_groups;
        for (int index = static_cast<int>(order.size()) - 1;
             index > 0; --index) {
            swap(order[index], order[order_random.next_int(index + 1)]);
        }
        vector<Move> candidate;
        for (int group : order) {
            append_round_trip(candidate, group, selected[group]);
        }
        candidate = reduce_operations(
            move(candidate), 0x42666666ULL + static_cast<uint64_t>(trial));
        if (candidate.size() < answer.size()) answer = move(candidate);
    }
#endif

    assert(static_cast<int>(answer.size()) <= 4 * N * N);
    assert(simulate(answer));
    for (const Move& move : answer) {
        cout << move.direction << ' ' << move.line << '\n';
    }
}
