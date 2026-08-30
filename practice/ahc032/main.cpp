#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <utility>
#include <vector>

using namespace std;

constexpr uint32_t MOD = 998244353U;
constexpr int BOARD_SIZE = 9;
constexpr int CELL_COUNT = BOARD_SIZE * BOARD_SIZE;
constexpr int STAMP_SIZE = 3;
constexpr int POSITION_COUNT = BOARD_SIZE - STAMP_SIZE + 1;

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

struct Operation {
    int stamp = 0;
    int row = 0;
    int column = 0;
};

struct Combo {
    int length = 0;
    array<int, 3> stamp_ids{};
    array<uint32_t, 9> addition{};
};

struct State {
    array<uint32_t, CELL_COUNT> board{};
    long long total = 0;
    long long fixed_sum = 0;
    int used = 0;
    vector<Operation> operations;
};

struct Candidate {
    long long evaluation = 0;
    long long total = 0;
    long long fixed_sum = 0;
    int parent = 0;
    int combo = 0;
    int used = 0;
};

struct SmallerEvaluationFirst {
    bool operator()(const Candidate& left, const Candidate& right) const {
        if (left.evaluation != right.evaluation) {
            return left.evaluation > right.evaluation;
        }
        return left.total > right.total;
    }
};

uint32_t add_mod(uint32_t left, uint32_t right) {
    uint32_t sum = left + right;
    if (sum >= MOD) sum -= MOD;
    return sum;
}

long long board_sum(const array<uint32_t, CELL_COUNT>& board) {
    long long sum = 0;
    for (uint32_t value : board) sum += value;
    return sum;
}

void apply_stamp(array<uint32_t, CELL_COUNT>& board, long long& total,
                 const array<array<uint32_t, 9>, 20>& stamp, int stamp_id,
                 int top_row, int top_column) {
    for (int row = 0; row < STAMP_SIZE; ++row) {
        for (int column = 0; column < STAMP_SIZE; ++column) {
            const int cell = (top_row + row) * BOARD_SIZE + top_column + column;
            const uint32_t before = board[cell];
            const uint32_t after = add_mod(before, stamp[stamp_id][row * 3 + column]);
            board[cell] = after;
            total += static_cast<long long>(after) - before;
        }
    }
}

void remove_stamp(array<uint32_t, CELL_COUNT>& board, long long& total,
                  const array<array<uint32_t, 9>, 20>& stamp, int stamp_id,
                  int top_row, int top_column) {
    for (int row = 0; row < STAMP_SIZE; ++row) {
        for (int column = 0; column < STAMP_SIZE; ++column) {
            const int cell = (top_row + row) * BOARD_SIZE + top_column + column;
            const uint32_t before = board[cell];
            const uint32_t value = stamp[stamp_id][row * 3 + column];
            const uint32_t after = before >= value ? before - value : before + MOD - value;
            board[cell] = after;
            total += static_cast<long long>(after) - before;
        }
    }
}

vector<Combo> make_combos(const array<array<uint32_t, 9>, 20>& stamp) {
    vector<Combo> combos;

    auto add_combo = [&](initializer_list<int> ids) {
        Combo combo;
        combo.length = static_cast<int>(ids.size());
        int index = 0;
        for (int stamp_id : ids) combo.stamp_ids[index++] = stamp_id;
        for (int cell = 0; cell < 9; ++cell) {
            uint32_t sum = 0;
            for (int stamp_id : ids) sum = add_mod(sum, stamp[stamp_id][cell]);
            combo.addition[cell] = sum;
        }
        combos.push_back(combo);
    };

    add_combo({});
    for (int first = 0; first < 20; ++first) {
        add_combo({first});
        for (int second = first; second < 20; ++second) {
            add_combo({first, second});
            for (int third = second; third < 20; ++third) {
                add_combo({first, second, third});
            }
        }
    }
    return combos;
}

State beam_search(const array<uint32_t, CELL_COUNT>& initial_board,
                  const vector<Combo>& combos, int operation_limit,
                  bool reverse_rows, bool reverse_columns, int beam_width) {
    State initial;
    initial.board = initial_board;
    initial.total = board_sum(initial.board);
    vector<State> beam = {initial};

    vector<pair<int, int>> positions;
    positions.reserve(POSITION_COUNT * POSITION_COUNT);
    for (int row_index = 0; row_index < POSITION_COUNT; ++row_index) {
        const int row = reverse_rows ? POSITION_COUNT - 1 - row_index : row_index;
        for (int column_index = 0; column_index < POSITION_COUNT; ++column_index) {
            const int column =
                reverse_columns ? POSITION_COUNT - 1 - column_index : column_index;
            positions.push_back({row, column});
        }
    }

    for (int stage = 0; stage < static_cast<int>(positions.size()); ++stage) {
        const auto [top_row, top_column] = positions[stage];
        const int fixed_local = (reverse_rows ? 2 : 0) * 3 +
                                (reverse_columns ? 2 : 0);

        const double target = static_cast<double>(stage + 1) * operation_limit /
                              static_cast<int>(positions.size());
        const int minimum_used =
            max(0, static_cast<int>(floor(target)) - 1);
        const int maximum_used =
            min(operation_limit, static_cast<int>(ceil(target)) + 1);
        const int remaining_weight =
            1000 * (stage + 1) / static_cast<int>(positions.size());

        priority_queue<Candidate, vector<Candidate>, SmallerEvaluationFirst> top;

        for (int parent_index = 0; parent_index < static_cast<int>(beam.size());
             ++parent_index) {
            const State& parent = beam[parent_index];
            for (int combo_index = 0; combo_index < static_cast<int>(combos.size());
                 ++combo_index) {
                const Combo& combo = combos[combo_index];
                const int used = parent.used + combo.length;
                if (used < minimum_used || used > maximum_used) continue;

                long long total = parent.total;
                uint32_t fixed_value = 0;
                for (int local_row = 0; local_row < 3; ++local_row) {
                    for (int local_column = 0; local_column < 3; ++local_column) {
                        const int local = local_row * 3 + local_column;
                        const int cell = (top_row + local_row) * BOARD_SIZE +
                                         top_column + local_column;
                        const uint32_t before = parent.board[cell];
                        const uint32_t after = add_mod(before, combo.addition[local]);
                        total += static_cast<long long>(after) - before;
                        if (local == fixed_local) fixed_value = after;
                    }
                }

                const long long fixed_sum = parent.fixed_sum + fixed_value;
                const long long remaining_sum = total - fixed_sum;
                const long long evaluation =
                    fixed_sum * 1000LL + remaining_sum * remaining_weight;
                Candidate candidate{evaluation, total, fixed_sum, parent_index,
                                    combo_index, used};

                if (static_cast<int>(top.size()) < beam_width) {
                    top.push(candidate);
                } else if (candidate.evaluation > top.top().evaluation ||
                           (candidate.evaluation == top.top().evaluation &&
                            candidate.total > top.top().total)) {
                    top.pop();
                    top.push(candidate);
                }
            }
        }

        vector<Candidate> selected;
        selected.reserve(top.size());
        while (!top.empty()) {
            selected.push_back(top.top());
            top.pop();
        }
        sort(selected.begin(), selected.end(), [](const Candidate& left,
                                                   const Candidate& right) {
            if (left.evaluation != right.evaluation) {
                return left.evaluation > right.evaluation;
            }
            return left.total > right.total;
        });

        vector<State> next_beam;
        next_beam.reserve(selected.size());
        for (const Candidate& candidate : selected) {
            State child = beam[candidate.parent];
            child.total = candidate.total;
            child.fixed_sum = candidate.fixed_sum;
            child.used = candidate.used;
            const Combo& combo = combos[candidate.combo];
            for (int local_row = 0; local_row < 3; ++local_row) {
                for (int local_column = 0; local_column < 3; ++local_column) {
                    const int local = local_row * 3 + local_column;
                    const int cell = (top_row + local_row) * BOARD_SIZE +
                                     top_column + local_column;
                    child.board[cell] = add_mod(child.board[cell], combo.addition[local]);
                }
            }
            for (int index = 0; index < combo.length; ++index) {
                child.operations.push_back(
                    {combo.stamp_ids[index], top_row, top_column});
            }
            next_beam.push_back(move(child));
        }
        beam = move(next_beam);
    }

    return *max_element(beam.begin(), beam.end(), [](const State& left,
                                                      const State& right) {
        return left.total < right.total;
    });
}

State greedy_baseline(const array<uint32_t, CELL_COUNT>& initial_board,
                      const array<array<uint32_t, 9>, 20>& stamp,
                      int operation_limit) {
    State state;
    state.board = initial_board;
    state.total = board_sum(state.board);

    while (static_cast<int>(state.operations.size()) < operation_limit) {
        long long best_difference = 0;
        Operation best_operation;
        for (int row = 0; row < POSITION_COUNT; ++row) {
            for (int column = 0; column < POSITION_COUNT; ++column) {
                for (int stamp_id = 0; stamp_id < 20; ++stamp_id) {
                    long long difference = 0;
                    for (int local_row = 0; local_row < 3; ++local_row) {
                        for (int local_column = 0; local_column < 3; ++local_column) {
                            const int cell = (row + local_row) * BOARD_SIZE +
                                             column + local_column;
                            const uint32_t before = state.board[cell];
                            const uint32_t after = add_mod(
                                before, stamp[stamp_id][local_row * 3 + local_column]);
                            difference += static_cast<long long>(after) - before;
                        }
                    }
                    if (difference > best_difference) {
                        best_difference = difference;
                        best_operation = {stamp_id, row, column};
                    }
                }
            }
        }
        if (best_difference <= 0) break;
        apply_stamp(state.board, state.total, stamp, best_operation.stamp,
                    best_operation.row, best_operation.column);
        state.operations.push_back(best_operation);
    }
    state.used = static_cast<int>(state.operations.size());
    return state;
}

void fill_unused_operations(State& state,
                            const array<array<uint32_t, 9>, 20>& stamp,
                            int operation_limit) {
    while (static_cast<int>(state.operations.size()) < operation_limit) {
        long long best_difference = 0;
        Operation best_operation;
        for (int row = 0; row < POSITION_COUNT; ++row) {
            for (int column = 0; column < POSITION_COUNT; ++column) {
                for (int stamp_id = 0; stamp_id < 20; ++stamp_id) {
                    long long difference = 0;
                    for (int local_row = 0; local_row < 3; ++local_row) {
                        for (int local_column = 0; local_column < 3; ++local_column) {
                            const int cell = (row + local_row) * BOARD_SIZE +
                                             column + local_column;
                            const uint32_t before = state.board[cell];
                            const uint32_t after = add_mod(
                                before, stamp[stamp_id][local_row * 3 + local_column]);
                            difference += static_cast<long long>(after) - before;
                        }
                    }
                    if (difference > best_difference) {
                        best_difference = difference;
                        best_operation = {stamp_id, row, column};
                    }
                }
            }
        }
        if (best_difference <= 0) break;
        apply_stamp(state.board, state.total, stamp, best_operation.stamp,
                    best_operation.row, best_operation.column);
        state.operations.push_back(best_operation);
    }
    state.used = static_cast<int>(state.operations.size());
}

void improve_by_replacement(State& state,
                            const array<array<uint32_t, 9>, 20>& stamp,
                            Random& random, const Timer& timer,
                            double deadline_ms) {
    if (state.operations.empty()) return;
    vector<Operation> best_operations = state.operations;
    array<uint32_t, CELL_COUNT> best_board = state.board;
    long long best_total = state.total;
    int iteration = 0;
    double current_time = timer.milliseconds();
    const double random_phase_end = max(0.0, deadline_ms - 100.0);

    while (current_time < random_phase_end) {
        if ((iteration & 255) == 0) current_time = timer.milliseconds();
        ++iteration;
        const int operation_index =
            random.next_int(static_cast<int>(state.operations.size()));
        const Operation old_operation = state.operations[operation_index];

        Operation new_operation;
        if (random.next_int(100) < 55) {
            new_operation = {random.next_int(20), old_operation.row,
                             old_operation.column};
        } else {
            new_operation = {random.next_int(20), random.next_int(POSITION_COUNT),
                             random.next_int(POSITION_COUNT)};
        }
        if (new_operation.stamp == old_operation.stamp &&
            new_operation.row == old_operation.row &&
            new_operation.column == old_operation.column) {
            continue;
        }

        const long long old_total = state.total;
        remove_stamp(state.board, state.total, stamp, old_operation.stamp,
                     old_operation.row, old_operation.column);
        apply_stamp(state.board, state.total, stamp, new_operation.stamp,
                    new_operation.row, new_operation.column);
        const long long difference = state.total - old_total;

        const double progress = min(1.0, current_time / deadline_ms);
        const double temperature = 1.2e9 * (1.0 - progress) + 2.0e6 * progress;
        if (difference >= 0 ||
            random.next_double() < exp(static_cast<double>(difference) / temperature)) {
            state.operations[operation_index] = new_operation;
            if (state.total > best_total) {
                best_total = state.total;
                best_operations = state.operations;
                best_board = state.board;
            }
        } else {
            remove_stamp(state.board, state.total, stamp, new_operation.stamp,
                         new_operation.row, new_operation.column);
            apply_stamp(state.board, state.total, stamp, old_operation.stamp,
                        old_operation.row, old_operation.column);
        }
    }

    auto restore_best = [&]() {
        state.operations = best_operations;
        state.board = best_board;
        state.total = best_total;
    };
    auto save_if_best = [&]() {
        if (state.total > best_total) {
            best_total = state.total;
            best_operations = state.operations;
            best_board = state.board;
        }
    };

    // Coordinate descent: remove one operation and compare every possible
    // replacement.  One full sweep examines only 81 * 20 * 7 * 7 * 9 cells.
    auto coordinate_descent = [&]() {
        vector<int> order(state.operations.size());
        iota(order.begin(), order.end(), 0);
        while (timer.milliseconds() < deadline_ms) {
            for (int index = static_cast<int>(order.size()) - 1; index > 0;
                 --index) {
                swap(order[index], order[random.next_int(index + 1)]);
            }
            bool improved = false;
            for (int order_index = 0; order_index < static_cast<int>(order.size());
                 ++order_index) {
                if ((order_index & 7) == 0 &&
                    timer.milliseconds() >= deadline_ms) {
                    return;
                }
                const int operation_index = order[order_index];
                const Operation old_operation = state.operations[operation_index];
                const long long old_total = state.total;
                if (old_operation.stamp >= 0) {
                    remove_stamp(state.board, state.total, stamp,
                                 old_operation.stamp, old_operation.row,
                                 old_operation.column);
                }

                long long best_addition = 0;
                Operation best_operation{-1, 0, 0};
                for (int row = 0; row < POSITION_COUNT; ++row) {
                    for (int column = 0; column < POSITION_COUNT; ++column) {
                        for (int stamp_id = 0; stamp_id < 20; ++stamp_id) {
                            long long addition = 0;
                            for (int local_row = 0; local_row < 3; ++local_row) {
                                for (int local_column = 0; local_column < 3;
                                     ++local_column) {
                                    const int cell =
                                        (row + local_row) * BOARD_SIZE + column +
                                        local_column;
                                    const uint32_t before = state.board[cell];
                                    const uint32_t after = add_mod(
                                        before, stamp[stamp_id]
                                                     [local_row * 3 + local_column]);
                                    addition +=
                                        static_cast<long long>(after) - before;
                                }
                            }
                            if (addition > best_addition) {
                                best_addition = addition;
                                best_operation = {stamp_id, row, column};
                            }
                        }
                    }
                }

                if (best_operation.stamp >= 0) {
                    apply_stamp(state.board, state.total, stamp,
                                best_operation.stamp, best_operation.row,
                                best_operation.column);
                }
                state.operations[operation_index] = best_operation;
                improved |= state.total > old_total;
            }
            if (!improved) return;
        }
    };

    restore_best();
    coordinate_descent();
    save_if_best();
    restore_best();
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, m, operation_limit;
    cin >> n >> m >> operation_limit;
    array<uint32_t, CELL_COUNT> initial_board{};
    uint64_t input_hash = 1469598103934665603ULL;
    for (uint32_t& value : initial_board) {
        cin >> value;
        input_hash ^= value;
        input_hash *= 1099511628211ULL;
    }

    array<array<uint32_t, 9>, 20> stamp{};
    for (int stamp_id = 0; stamp_id < m; ++stamp_id) {
        for (uint32_t& value : stamp[stamp_id]) {
            cin >> value;
            input_hash ^= value;
            input_hash *= 1099511628211ULL;
        }
    }

#ifdef AHC032_BASELINE
    State answer = greedy_baseline(initial_board, stamp, operation_limit);
#else
    const vector<Combo> combos = make_combos(stamp);
    Timer timer;
    Random random(input_hash ^ 0x9e3779b97f4a7c15ULL);
#ifdef LOCAL_SHORT_TIME
    constexpr int BEAM_WIDTH = 10;
    constexpr double DEADLINE_MS = 90.0;
#else
    constexpr int BEAM_WIDTH = 80;
    constexpr double DEADLINE_MS = 1850.0;
#endif

    State answer;
    answer.total = numeric_limits<long long>::min();
    for (int row_direction = 0; row_direction < 2; ++row_direction) {
        for (int column_direction = 0; column_direction < 2; ++column_direction) {
            State candidate = beam_search(initial_board, combos, operation_limit,
                                          row_direction != 0,
                                          column_direction != 0, BEAM_WIDTH);
            if (candidate.total > answer.total) answer = move(candidate);
        }
    }
    fill_unused_operations(answer, stamp, operation_limit);
    improve_by_replacement(answer, stamp, random, timer, DEADLINE_MS);
#endif

    answer.operations.erase(
        remove_if(answer.operations.begin(), answer.operations.end(),
                  [](const Operation& operation) { return operation.stamp < 0; }),
        answer.operations.end());
    cout << answer.operations.size() << '\n';
    for (const Operation& operation : answer.operations) {
        cout << operation.stamp << ' ' << operation.row << ' ' << operation.column
             << '\n';
    }
    return 0;
}
