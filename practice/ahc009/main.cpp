#pragma GCC optimize("O3,unroll-loops")
#include <bits/stdc++.h>
using namespace std;

constexpr int BOARD_SIZE = 20;
constexpr int CELL_COUNT = BOARD_SIZE * BOARD_SIZE;
constexpr int MAX_COMMANDS = 200;

// library/timer.hpp と同じ、時間制限付き探索用のタイマー。
struct Timer {
    chrono::steady_clock::time_point start = chrono::steady_clock::now();

    double elapsed_ms() const {
        return chrono::duration<double, milli>(
                   chrono::steady_clock::now() - start)
            .count();
    }
};

// library/random.hpp の提出用コピー。
struct Random {
    mt19937_64 engine;

    explicit Random(uint64_t seed) : engine(seed) {}

    template <class Int>
    Int next_int(Int left, Int right) {
        assert(left < right);
        return uniform_int_distribution<Int>(left, right - 1)(engine);
    }

    double next_double() {
        return uniform_real_distribution<double>(0.0, 1.0)(engine);
    }
};

struct Maze {
    // direction: 0=U, 1=D, 2=L, 3=R
    array<array<int, 4>, CELL_COUNT> move_to{};
    int start = 0;
    int goal = 0;
    double forget_probability = 0.0;
};

int cell_id(int row, int column) {
    return row * BOARD_SIZE + column;
}

int direction_id(char command) {
    if (command == 'U') return 0;
    if (command == 'D') return 1;
    if (command == 'L') return 2;
    return 3;
}

char direction_char(int direction) {
    static constexpr char COMMANDS[] = "UDLR";
    return COMMANDS[direction];
}

// 各コマンドについて、まだ到着していない人の位置確率を全て進める。
// 戻り値は公式得点（round前）と同じ 250000 * E[S]。
double evaluate_commands(const Maze& maze, const string& commands) {
    array<double, CELL_COUNT> first{};
    array<double, CELL_COUNT> second{};
    array<int, CELL_COUNT> first_active{};
    array<int, CELL_COUNT> second_active{};

    double* current = first.data();
    double* next = second.data();
    int* active = first_active.data();
    int* next_active = second_active.data();
    int active_count = 1;
    current[maze.start] = 1.0;
    active[0] = maze.start;

    const double move_probability = 1.0 - maze.forget_probability;
    double expected_value = 0.0;

    for (int turn = 0; turn < static_cast<int>(commands.size()); ++turn) {
        const int direction = direction_id(commands[turn]);
        int next_count = 0;

        auto add_probability = [&](int cell, double probability) {
            if (probability == 0.0) return;
            if (next[cell] == 0.0) next_active[next_count++] = cell;
            next[cell] += probability;
        };

        for (int index = 0; index < active_count; ++index) {
            const int cell = active[index];
            const double probability = current[cell];
            const int destination = maze.move_to[cell][direction];

            // コマンドを忘れた場合は現在地に残る。
            add_probability(
                cell, probability * maze.forget_probability
            );

            // 覚えていた場合。壁ならdestination==cellなので、そのまま残る。
            if (destination == maze.goal) {
                const double arrived = probability * move_probability;
                // turnは0始まり。公式のt=turn+1なので S=400-turn。
                expected_value += arrived * (400 - turn);
            } else {
                add_probability(destination, probability * move_probability);
            }
        }

        // 使い終えた配列だけ0に戻す。毎ターン400要素をfillする必要がない。
        for (int index = 0; index < active_count; ++index) {
            current[active[index]] = 0.0;
        }
        swap(current, next);
        swap(active, next_active);
        active_count = next_count;
        if (active_count == 0) break;
    }

    return expected_value * 250000.0;
}

// 辺重みを少しずつ変えたDijkstraで、多様な単純経路を作る。
string make_shortest_path(const Maze& maze, Random& random, bool randomize) {
    constexpr int INF = 1 << 29;
    array<int, CELL_COUNT> distance;
    array<int, CELL_COUNT> previous_cell;
    array<int, CELL_COUNT> previous_direction;
    distance.fill(INF);
    previous_cell.fill(-1);
    previous_direction.fill(-1);

    using QueueItem = pair<int, int>;
    priority_queue<QueueItem, vector<QueueItem>, greater<QueueItem>> queue;
    distance[maze.start] = 0;
    queue.push({0, maze.start});

    while (!queue.empty()) {
        const auto [current_distance, cell] = queue.top();
        queue.pop();
        if (current_distance != distance[cell]) continue;
        if (cell == maze.goal) break;

        for (int direction = 0; direction < 4; ++direction) {
            const int destination = maze.move_to[cell][direction];
            if (destination == cell) continue;
            const int edge_cost = randomize
                ? random.next_int(10, 31)
                : 10;
            const int candidate_distance = current_distance + edge_cost;
            if (candidate_distance >= distance[destination]) continue;
            distance[destination] = candidate_distance;
            previous_cell[destination] = cell;
            previous_direction[destination] = direction;
            queue.push({candidate_distance, destination});
        }
    }

    string reversed_path;
    for (int cell = maze.goal; cell != maze.start;
         cell = previous_cell[cell]) {
        if (previous_cell[cell] == -1) return "";
        reversed_path.push_back(direction_char(previous_direction[cell]));
    }
    reverse(reversed_path.begin(), reversed_path.end());
    return reversed_path;
}

// 失敗率を見越し、経路中の文字を複製して長さを約1/(1-p)倍にする。
string add_redundancy(
    string path,
    double forget_probability,
    Random& random
) {
    if (path.empty()) return path;
    const double move_probability = 1.0 - forget_probability;
    int target_length = static_cast<int>(
        lround(path.size() / move_probability)
    );
    target_length += random.next_int(-4, 11);
    target_length = clamp(
        target_length,
        static_cast<int>(path.size()),
        MAX_COMMANDS
    );

    while (static_cast<int>(path.size()) < target_length) {
        const int position = random.next_int(0, static_cast<int>(path.size()));
        path.insert(path.begin() + position, path[position]);
    }
    return path;
}

// 「各マスからは、そのマス専用の最短方向を選べる」と楽観した将来価値を使うbeam search。
// 実際には全マスへ同じ1文字を出すため上界そのものではないが、ゴールへ近い確率分布を
// 残す目印として使える。
string make_beam_commands(const Maze& maze, int beam_width) {
    array<int, CELL_COUNT> distance;
    distance.fill(1 << 20);
    queue<int> queue;
    distance[maze.goal] = 0;
    queue.push(maze.goal);
    while (!queue.empty()) {
        const int cell = queue.front();
        queue.pop();
        for (int direction = 0; direction < 4; ++direction) {
            const int neighbor = maze.move_to[cell][direction];
            if (neighbor == cell || distance[neighbor] <= distance[cell] + 1) {
                continue;
            }
            distance[neighbor] = distance[cell] + 1;
            queue.push(neighbor);
        }
    }

    // optimistic[used][d]: used文字出力済み、あとd回の成功で着くと仮定した将来得点。
    array<array<double, CELL_COUNT>, MAX_COMMANDS + 1> optimistic{};
    const double move_probability = 1.0 - maze.forget_probability;
    for (int used = MAX_COMMANDS - 1; used >= 0; --used) {
        for (int d = 1; d < CELL_COUNT; ++d) {
            const double success_value = d == 1
                ? 250000.0 * (400 - used)
                : optimistic[used + 1][d - 1];
            optimistic[used][d] =
                maze.forget_probability * optimistic[used + 1][d] +
                move_probability * success_value;
        }
    }

    struct BeamState {
        array<double, CELL_COUNT> probability{};
        double score = 0.0;
        double priority = 0.0;
        string commands;
    };

    vector<BeamState> beam(1);
    beam[0].probability[maze.start] = 1.0;
    beam[0].priority = optimistic[0][distance[maze.start]];

    for (int used = 0; used < MAX_COMMANDS; ++used) {
        vector<BeamState> candidates;
        candidates.reserve(beam.size() * 4);

        for (const BeamState& state : beam) {
            for (int direction = 0; direction < 4; ++direction) {
                BeamState candidate;
                candidate.score = state.score;
                candidate.commands = state.commands;
                candidate.commands.push_back(direction_char(direction));

                for (int cell = 0; cell < CELL_COUNT; ++cell) {
                    const double probability = state.probability[cell];
                    if (probability == 0.0) continue;
                    const int destination = maze.move_to[cell][direction];
                    candidate.probability[cell] +=
                        probability * maze.forget_probability;
                    if (destination == maze.goal) {
                        candidate.score += probability * move_probability *
                                           250000.0 * (400 - used);
                    } else {
                        candidate.probability[destination] +=
                            probability * move_probability;
                    }
                }

                candidate.priority = candidate.score;
                for (int cell = 0; cell < CELL_COUNT; ++cell) {
                    candidate.priority += candidate.probability[cell] *
                        optimistic[used + 1][distance[cell]];
                }
                candidates.push_back(move(candidate));
            }
        }

        const int keep = min(beam_width, static_cast<int>(candidates.size()));
        if (keep < static_cast<int>(candidates.size())) {
            nth_element(
                candidates.begin(),
                candidates.begin() + keep,
                candidates.end(),
                [](const BeamState& left, const BeamState& right) {
                    return left.priority > right.priority;
                }
            );
            candidates.resize(keep);
        }
        beam.swap(candidates);
    }

    return max_element(
        beam.begin(),
        beam.end(),
        [](const BeamState& left, const BeamState& right) {
            return left.score < right.score;
        }
    )->commands;
}

void mutate_commands(string& commands, Random& random) {
    int operation = random.next_int(0, 100);

    if (operation < 24 && static_cast<int>(commands.size()) < MAX_COMMANDS) {
        // 同じ命令を隣へ足す。失念への直接的な冗長化になる。
        const int position = random.next_int(0, static_cast<int>(commands.size()));
        commands.insert(commands.begin() + position, commands[position]);
    } else if (
        operation < 39 && static_cast<int>(commands.size()) < MAX_COMMANDS
    ) {
        // 任意方向を1文字追加する。
        const int position = random.next_int(
            0, static_cast<int>(commands.size()) + 1
        );
        commands.insert(
            commands.begin() + position,
            direction_char(random.next_int(0, 4))
        );
    } else if (operation < 56 && commands.size() > 1) {
        const int position = random.next_int(0, static_cast<int>(commands.size()));
        commands.erase(commands.begin() + position);
    } else if (operation < 78) {
        const int position = random.next_int(0, static_cast<int>(commands.size()));
        char replacement = direction_char(random.next_int(0, 4));
        if (replacement == commands[position]) {
            replacement = direction_char(
                (direction_id(replacement) + 1 + random.next_int(0, 3)) % 4
            );
        }
        commands[position] = replacement;
    } else if (commands.size() > 1) {
        // 1文字を別位置へ移す。長さを変えずに順序だけ調整する。
        const int from = random.next_int(0, static_cast<int>(commands.size()));
        const char command = commands[from];
        commands.erase(commands.begin() + from);
        const int to = random.next_int(0, static_cast<int>(commands.size()) + 1);
        commands.insert(commands.begin() + to, command);
    } else {
        commands[0] = direction_char(random.next_int(0, 4));
    }
}

// 現在の200文字列について、前向き確率と後ろ向き価値を保存する。
// 1文字だけ変えた得点なら、変更位置の400マスを見るだけで厳密に求められる。
struct OneCharacterChangeCache {
    const Maze& maze;
    vector<array<double, CELL_COUNT>> forward;
    vector<array<double, CELL_COUNT>> backward;
    vector<double> prefix_score;

    explicit OneCharacterChangeCache(const Maze& input_maze) : maze(input_maze) {}

    double build(const string& commands) {
        const int length = static_cast<int>(commands.size());
        forward.assign(length + 1, {});
        backward.assign(length + 1, {});
        prefix_score.assign(length + 1, 0.0);
        forward[0][maze.start] = 1.0;

        const double move_probability = 1.0 - maze.forget_probability;
        for (int turn = 0; turn < length; ++turn) {
            const int direction = direction_id(commands[turn]);
            prefix_score[turn + 1] = prefix_score[turn];
            for (int cell = 0; cell < CELL_COUNT; ++cell) {
                const double probability = forward[turn][cell];
                if (probability == 0.0) continue;
                const int destination = maze.move_to[cell][direction];
                forward[turn + 1][cell] +=
                    probability * maze.forget_probability;
                if (destination == maze.goal) {
                    prefix_score[turn + 1] += probability * move_probability *
                        250000.0 * (400 - turn);
                } else {
                    forward[turn + 1][destination] +=
                        probability * move_probability;
                }
            }
        }

        for (int turn = length - 1; turn >= 0; --turn) {
            const int direction = direction_id(commands[turn]);
            for (int cell = 0; cell < CELL_COUNT; ++cell) {
                const int destination = maze.move_to[cell][direction];
                double value = maze.forget_probability *
                               backward[turn + 1][cell];
                if (destination == maze.goal) {
                    value += move_probability * 250000.0 * (400 - turn);
                } else {
                    value += move_probability *
                             backward[turn + 1][destination];
                }
                backward[turn][cell] = value;
            }
        }
        return prefix_score[length];
    }

    double score_if_changed(int position, int new_direction) const {
        const double move_probability = 1.0 - maze.forget_probability;
        double score = prefix_score[position];
        for (int cell = 0; cell < CELL_COUNT; ++cell) {
            const double probability = forward[position][cell];
            if (probability == 0.0) continue;
            const int destination = maze.move_to[cell][new_direction];
            double value = maze.forget_probability *
                           backward[position + 1][cell];
            if (destination == maze.goal) {
                value += move_probability *
                         250000.0 * (400 - position);
            } else {
                value += move_probability *
                         backward[position + 1][destination];
            }
            score += probability * value;
        }
        return score;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int start_row, start_column, goal_row, goal_column;
    Maze maze;
    cin >> start_row >> start_column >> goal_row >> goal_column;
    cin >> maze.forget_probability;
    maze.start = cell_id(start_row, start_column);
    maze.goal = cell_id(goal_row, goal_column);

    vector<string> horizontal_walls(BOARD_SIZE);
    vector<string> vertical_walls(BOARD_SIZE - 1);
    for (string& row : horizontal_walls) cin >> row;
    for (string& row : vertical_walls) cin >> row;

    for (int row = 0; row < BOARD_SIZE; ++row) {
        for (int column = 0; column < BOARD_SIZE; ++column) {
            const int cell = cell_id(row, column);
            maze.move_to[cell] = {cell, cell, cell, cell};
            if (row > 0 && vertical_walls[row - 1][column] == '0') {
                maze.move_to[cell][0] = cell_id(row - 1, column);
            }
            if (
                row + 1 < BOARD_SIZE &&
                vertical_walls[row][column] == '0'
            ) {
                maze.move_to[cell][1] = cell_id(row + 1, column);
            }
            if (column > 0 && horizontal_walls[row][column - 1] == '0') {
                maze.move_to[cell][2] = cell_id(row, column - 1);
            }
            if (
                column + 1 < BOARD_SIZE &&
                horizontal_walls[row][column] == '0'
            ) {
                maze.move_to[cell][3] = cell_id(row, column + 1);
            }
        }
    }

    // 入力からseedを作り、CPU速度以外の乱数ぶれをなくす。
    uint64_t input_hash = 1469598103934665603ULL;
    auto mix = [&](uint64_t value) {
        input_hash ^= value;
        input_hash *= 1099511628211ULL;
    };
    mix(maze.start);
    mix(maze.goal);
    mix(static_cast<int>(lround(maze.forget_probability * 100.0)));
    for (const string& row : horizontal_walls) {
        for (char value : row) mix(value);
    }
    for (const string& row : vertical_walls) {
        for (char value : row) mix(value);
    }

    Random random(input_hash);
    Timer timer;
    constexpr double INITIAL_END_MS = 240.0;
    constexpr double VARIABLE_SEARCH_END_MS = 1620.0;
    constexpr double SEARCH_END_MS = 1870.0;

    string best_commands = make_shortest_path(maze, random, false);
    best_commands = add_redundancy(
        best_commands, maze.forget_probability, random
    );
    double best_score = evaluate_commands(maze, best_commands);
    int initial_candidates = 1;

    string beam_commands = make_beam_commands(maze, 96);
    const double beam_score = evaluate_commands(maze, beam_commands);
    if (beam_score > best_score) {
        best_score = beam_score;
        best_commands.swap(beam_commands);
    }

    // ランダムに少し違う経路を多数作り、最も頑丈なものから探索を始める。
    while (timer.elapsed_ms() < INITIAL_END_MS) {
        string candidate = make_shortest_path(maze, random, true);
        if (candidate.empty() || candidate.size() > MAX_COMMANDS) continue;
        candidate = add_redundancy(
            candidate, maze.forget_probability, random
        );
        const double score = evaluate_commands(maze, candidate);
        ++initial_candidates;
        if (score > best_score) {
            best_score = score;
            best_commands.swap(candidate);
        }
    }

    [[maybe_unused]] const double initial_best_score = best_score;

    string current_commands = best_commands;
    double current_score = best_score;
    constexpr double START_TEMPERATURE = 100000.0;
    constexpr double END_TEMPERATURE = 100.0;

    int iteration = 0;
    double progress = 0.0;
    double temperature = START_TEMPERATURE;
    while (true) {
        if ((iteration & 15) == 0) {
            const double elapsed = timer.elapsed_ms();
            if (elapsed >= VARIABLE_SEARCH_END_MS) break;
            progress = clamp(
                (elapsed - INITIAL_END_MS) /
                    (VARIABLE_SEARCH_END_MS - INITIAL_END_MS),
                0.0,
                1.0
            );
            temperature = START_TEMPERATURE * pow(
                END_TEMPERATURE / START_TEMPERATURE, progress
            );
        }
        ++iteration;

        string candidate = current_commands;
        mutate_commands(candidate, random);
        const double candidate_score = evaluate_commands(maze, candidate);
        const double improvement = candidate_score - current_score;
        const bool accept = improvement >= 0.0 ||
            random.next_double() < exp(improvement / temperature);
        if (!accept) continue;

        current_commands.swap(candidate);
        current_score = candidate_score;
        if (current_score > best_score) {
            best_score = current_score;
            best_commands = current_commands;
        }
    }

    [[maybe_unused]] const double variable_best_score = best_score;

    // 末尾への追加は、それ以前の到着確率を一切悪化させない。複数のfallback経路で
    // 必ず200文字まで埋め、最も得点が高いものを採用する。
    if (best_commands.size() < MAX_COMMANDS) {
        string padded_best = best_commands;
        double padded_best_score = best_score;
        for (int trial = 0; trial < 32; ++trial) {
            string candidate = best_commands;
            while (static_cast<int>(candidate.size()) < MAX_COMMANDS) {
                string suffix = make_shortest_path(maze, random, trial != 0);
                if (suffix.empty()) suffix = "DR";
                const int add = min(
                    static_cast<int>(suffix.size()),
                    MAX_COMMANDS - static_cast<int>(candidate.size())
                );
                candidate.append(suffix.begin(), suffix.begin() + add);
            }
            const double score = evaluate_commands(maze, candidate);
            if (score > padded_best_score) {
                padded_best_score = score;
                padded_best.swap(candidate);
            }
        }
        best_commands.swap(padded_best);
        best_score = padded_best_score;
    }

    // 最後は1文字変更を厳密O(400)で比較するbest-improvement山登り。
    // 変更を1つ採用した時だけ前後の表を作り直す。
    OneCharacterChangeCache change_cache(maze);
    while (timer.elapsed_ms() < SEARCH_END_MS) {
        const double cached_score = change_cache.build(best_commands);
#ifdef LOCAL
        assert(abs(cached_score - evaluate_commands(maze, best_commands)) < 1e-3);
#endif
        double next_score = cached_score;
        int best_position = -1;
        int best_direction = -1;
        const int first_position = random.next_int(
            0, static_cast<int>(best_commands.size())
        );

        for (int offset = 0;
             offset < static_cast<int>(best_commands.size());
             ++offset) {
            if ((offset & 31) == 0 && timer.elapsed_ms() >= SEARCH_END_MS) {
                break;
            }
            const int position =
                (first_position + offset) % best_commands.size();
            const int old_direction = direction_id(best_commands[position]);
            for (int direction = 0; direction < 4; ++direction) {
                if (direction == old_direction) continue;
                const double score =
                    change_cache.score_if_changed(position, direction);
                if (score > next_score + 0.5) {
                    next_score = score;
                    best_position = position;
                    best_direction = direction;
                }
            }
        }

        if (best_position == -1) break;
        best_commands[best_position] = direction_char(best_direction);
        best_score = next_score;
    }

    cout << best_commands << '\n';
#ifdef LOCAL
    cerr << "initial_candidates=" << initial_candidates
         << " iterations=" << iteration
         << " initial_score=" << llround(initial_best_score)
         << " variable_score=" << llround(variable_best_score)
         << " final_score=" << llround(best_score)
         << " length=" << best_commands.size() << '\n';
#endif
    return 0;
}
