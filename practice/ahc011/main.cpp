#include <bits/stdc++.h>
using namespace std;

// AHC011: Sliding Tree Puzzle
// 盤面をそのまま状態にした、重複除去付きのビームサーチ。

constexpr int MAX_N = 10;
constexpr int MAX_CELLS = MAX_N * MAX_N;
constexpr double BEAM_END_MS = 2200.0;
constexpr double SEARCH_END_MS = 2500.0;

struct Timer {
    chrono::steady_clock::time_point start = chrono::steady_clock::now();

    double elapsed_ms() const {
        return chrono::duration<double, milli>(
                   chrono::steady_clock::now() - start)
            .count();
    }
};

struct UnionFind {
    array<int, MAX_CELLS> parent{};

    UnionFind() {
        parent.fill(-1);
    }

    int root(int x) {
        while (parent[x] >= 0) {
            if (parent[parent[x]] >= 0) parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    }

    int size(int x) {
        return -parent[root(x)];
    }

    int unite(int a, int b) {
        a = root(a);
        b = root(b);
        if (a == b) return a;
        if (parent[a] > parent[b]) swap(a, b);
        parent[a] += parent[b];
        parent[b] = a;
        return a;
    }
};

struct Evaluation {
    int largest_tree = 0;
    int matched_edges = 0;
    int tree_square_sum = 0;
    int cyclic_square_sum = 0;
    long long priority = 0;
};

struct HistoryNode {
    int parent = -1;
    char move = '?';
};

struct State {
    array<unsigned char, MAX_CELLS> board{};
    uint64_t hash = 0;
    int history_node = 0;
    int empty_cell = 0;
    int previous_direction = -1;
    Evaluation evaluation;
};

uint64_t splitmix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

struct Random {
    uint64_t state;

    explicit Random(uint64_t seed) : state(seed) {}

    uint64_t next() {
        state ^= state << 7;
        state ^= state >> 9;
        return state;
    }

    int next_int(int upper_bound) {
        return static_cast<int>(next() % static_cast<uint64_t>(upper_bound));
    }

    double next_double() {
        return static_cast<double>(next() >> 11) *
               (1.0 / 9007199254740992.0);
    }
};

struct Solver {
    int n = 0;
    int turn_limit = 0;
    int cell_count = 0;
    array<array<uint64_t, 16>, MAX_CELLS> zobrist{};
    Timer timer;

    static constexpr array<int, 4> DR = {-1, 1, 0, 0};
    static constexpr array<int, 4> DC = {0, 0, -1, 1};
    static constexpr array<int, 4> OPPOSITE = {1, 0, 3, 2};
    static constexpr array<char, 4> COMMAND = {'U', 'D', 'L', 'R'};

    explicit Solver(int board_size, int maximum_turns)
        : n(board_size),
          turn_limit(maximum_turns),
          cell_count(board_size * board_size) {
        for (int cell = 0; cell < cell_count; ++cell) {
            for (int tile = 0; tile < 16; ++tile) {
                zobrist[cell][tile] = splitmix64(
                    static_cast<uint64_t>(cell * 16 + tile + 1)
                );
            }
        }
    }

    bool inside(int row, int column) const {
        return 0 <= row && row < n && 0 <= column && column < n;
    }

    uint64_t board_hash(
        const array<unsigned char, MAX_CELLS>& board
    ) const {
        uint64_t result = 0;
        for (int cell = 0; cell < cell_count; ++cell) {
            result ^= zobrist[cell][board[cell]];
        }
        return result;
    }

    // 公式と同じく、閉路を含まない連結成分だけを「木」として数える。
    Evaluation evaluate(
        const array<unsigned char, MAX_CELLS>& board
    ) const {
        UnionFind uf;
        array<unsigned char, MAX_CELLS> cyclic{};
        int matched_edges = 0;

        auto add_edge = [&](int a, int b) {
            ++matched_edges;
            int root_a = uf.root(a);
            int root_b = uf.root(b);
            if (root_a == root_b) {
                cyclic[root_a] = 1;
                return;
            }
            const bool has_cycle = cyclic[root_a] || cyclic[root_b];
            const int new_root = uf.unite(root_a, root_b);
            cyclic[new_root] = static_cast<unsigned char>(has_cycle);
        };

        for (int row = 0; row < n; ++row) {
            for (int column = 0; column < n; ++column) {
                const int cell = row * n + column;
                const int tile = board[cell];
                if (tile == 0) continue;

                if (column + 1 < n &&
                    (tile & 4) != 0 &&
                    (board[cell + 1] & 1) != 0) {
                    add_edge(cell, cell + 1);
                }
                if (row + 1 < n &&
                    (tile & 8) != 0 &&
                    (board[cell + n] & 2) != 0) {
                    add_edge(cell, cell + n);
                }
            }
        }

        Evaluation result;
        result.matched_edges = matched_edges;
        for (int cell = 0; cell < cell_count; ++cell) {
            if (board[cell] == 0 || uf.root(cell) != cell) continue;
            const int component_size = uf.size(cell);
            const int square = component_size * component_size;
            if (cyclic[cell]) {
                result.cyclic_square_sum += square;
            } else {
                result.largest_tree = max(result.largest_tree, component_size);
                result.tree_square_sum += square;
            }
        }

        // 一致辺は完成形へ向かう局所的な目印。
        // 木成分の二乗和は、小さな木同士をつなぐ手を高く評価する。
        // 閉路成分には逆符号の罰則を付ける。
        result.priority =
            800LL * result.matched_edges +
            15LL * result.tree_square_sum -
            30LL * result.cyclic_square_sum +
            1500LL * result.largest_tree;
        return result;
    }

    string restore_path(
        int node,
        const vector<HistoryNode>& history
    ) const {
        string answer;
        while (history[node].parent != -1) {
            answer.push_back(history[node].move);
            node = history[node].parent;
        }
        reverse(answer.begin(), answer.end());
        return answer;
    }

    string solve(State initial) {
        // Nが大きいほど1状態の評価が重いため、幅を少し狭める。
        const int beam_width = max(
            900,
            4200 - 350 * (n - 6)
        );

        vector<HistoryNode> history;
        history.reserve(static_cast<size_t>(beam_width) * 500);
        history.push_back({-1, '?'});

        initial.evaluation = evaluate(initial.board);
        vector<State> beam(1, initial);
        State best = initial;

        unordered_set<uint64_t> visited;
        visited.reserve(1 << 20);
        visited.insert(initial.hash);

        for (int depth = 0; depth < turn_limit; ++depth) {
            if (timer.elapsed_ms() >= BEAM_END_MS) break;

            vector<State> candidates;
            candidates.reserve(beam.size() * 3);

            for (const State& state : beam) {
                const int empty_row = state.empty_cell / n;
                const int empty_column = state.empty_cell % n;

                for (int direction = 0; direction < 4; ++direction) {
                    if (state.previous_direction != -1 &&
                        direction == OPPOSITE[state.previous_direction]) {
                        continue;
                    }
                    const int next_row = empty_row + DR[direction];
                    const int next_column = empty_column + DC[direction];
                    if (!inside(next_row, next_column)) continue;

                    const int next_empty = next_row * n + next_column;
                    State candidate = state;
                    const int moved_tile = candidate.board[next_empty];
                    candidate.hash ^=
                        zobrist[state.empty_cell][0] ^
                        zobrist[next_empty][moved_tile] ^
                        zobrist[state.empty_cell][moved_tile] ^
                        zobrist[next_empty][0];
                    swap(
                        candidate.board[state.empty_cell],
                        candidate.board[next_empty]
                    );
                    candidate.empty_cell = next_empty;
                    candidate.previous_direction = direction;

                    // 同じ盤面へ戻る遠回りは、得点上も有利にならない。
                    if (!visited.insert(candidate.hash).second) continue;

                    candidate.evaluation = evaluate(candidate.board);
                    // 同点状態の形を少し散らし、同じ局所解への集中を防ぐ。
                    candidate.evaluation.priority +=
                        static_cast<long long>(candidate.hash & 511ULL);
                    candidate.history_node =
                        (state.history_node << 2) | direction;
                    candidates.push_back(candidate);
                }
            }

            if (candidates.empty()) break;

            auto better = [](const State& a, const State& b) {
                return a.evaluation.priority > b.evaluation.priority;
            };
            if (static_cast<int>(candidates.size()) > beam_width) {
                nth_element(
                    candidates.begin(),
                    candidates.begin() + beam_width,
                    candidates.end(),
                    better
                );
                candidates.resize(beam_width);
            }

            // 候補作成中のhistory_nodeには「親番号×4+方向」を一時保存した。
            // 採用状態だけ履歴ノードを作るので、履歴メモリを節約できる。
            beam.clear();
            beam.reserve(candidates.size());
            for (State& candidate : candidates) {
                const int direction = candidate.history_node & 3;
                const int parent_node = candidate.history_node >> 2;
                history.push_back({parent_node, COMMAND[direction]});
                candidate.history_node = static_cast<int>(history.size()) - 1;

                const Evaluation& value = candidate.evaluation;
                if (value.largest_tree > best.evaluation.largest_tree) {
                    best = candidate;
                }
                beam.push_back(candidate);
            }

            if (best.evaluation.largest_tree == cell_count - 1) break;
        }

        string best_path = restore_path(best.history_node, history);
        best.evaluation = evaluate(best.board);

        // 大きい木の完成直前では、一度つながりを崩す必要があることが多い。
        // 残り時間で短い合法手列を提案し、bestを失わずに谷を越えてみる。
        Random random(initial.hash ^ 0x8b8b8b8b8b8b8b8bULL);
        State current = best;
        string current_path = best_path;
        int iteration = 0;

        while (timer.elapsed_ms() < SEARCH_END_MS &&
               best.evaluation.largest_tree < cell_count - 1) {
            if (static_cast<int>(current_path.size()) + 12 > turn_limit ||
                iteration % 80 == 79) {
                current = best;
                current_path = best_path;
            }

            State candidate = current;
            string extra_moves;
            const int proposed_length =
                2 + random.next_int(19);
            int previous_direction = candidate.previous_direction;

            for (int step = 0;
                 step < proposed_length &&
                 static_cast<int>(current_path.size() + extra_moves.size()) <
                     turn_limit;
                 ++step) {
                const int row = candidate.empty_cell / n;
                const int column = candidate.empty_cell % n;
                array<int, 4> choices{};
                int choice_count = 0;
                for (int direction = 0; direction < 4; ++direction) {
                    if (previous_direction != -1 &&
                        direction == OPPOSITE[previous_direction]) {
                        continue;
                    }
                    if (inside(row + DR[direction], column + DC[direction])) {
                        choices[choice_count++] = direction;
                    }
                }
                if (choice_count == 0) break;

                const int direction = choices[random.next_int(choice_count)];
                const int next_cell =
                    (row + DR[direction]) * n + column + DC[direction];
                const int moved_tile = candidate.board[next_cell];
                candidate.hash ^=
                    zobrist[candidate.empty_cell][0] ^
                    zobrist[next_cell][moved_tile] ^
                    zobrist[candidate.empty_cell][moved_tile] ^
                    zobrist[next_cell][0];
                swap(
                    candidate.board[candidate.empty_cell],
                    candidate.board[next_cell]
                );
                candidate.empty_cell = next_cell;
                candidate.previous_direction = direction;
                previous_direction = direction;
                extra_moves.push_back(COMMAND[direction]);
            }

            candidate.evaluation = evaluate(candidate.board);
            if (candidate.evaluation.largest_tree >
                best.evaluation.largest_tree) {
                best = candidate;
                best_path = current_path + extra_moves;
            }

            const double progress = min(
                1.0,
                timer.elapsed_ms() / SEARCH_END_MS
            );
            const double temperature = 12000.0 * (1.0 - progress) + 300.0;
            const long long difference =
                candidate.evaluation.priority - current.evaluation.priority;
            if (difference >= 0 ||
                random.next_double() < exp(difference / temperature)) {
                current = candidate;
                current_path += extra_moves;
            }
            ++iteration;
        }

        return best_path;
    }
};

int hexadecimal_value(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    return c - 'a' + 10;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, turn_limit;
    cin >> n >> turn_limit;

    State initial;
    for (int row = 0; row < n; ++row) {
        string line;
        cin >> line;
        for (int column = 0; column < n; ++column) {
            const int cell = row * n + column;
            initial.board[cell] = static_cast<unsigned char>(
                hexadecimal_value(line[column])
            );
            if (initial.board[cell] == 0) initial.empty_cell = cell;
        }
    }

    Solver solver(n, turn_limit);
    initial.hash = solver.board_hash(initial.board);
    cout << solver.solve(initial) << '\n';
}
