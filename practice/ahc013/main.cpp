#include <bits/stdc++.h>
using namespace std;

struct Dsu {
    vector<int> parent;
    vector<int> component_size;

    explicit Dsu(int n) : parent(n), component_size(n, 1) {
        iota(parent.begin(), parent.end(), 0);
    }

    int leader(int v) {
        if (parent[v] == v) return v;
        return parent[v] = leader(parent[v]);
    }

    bool same(int a, int b) {
        return leader(a) == leader(b);
    }

    int size(int v) {
        return component_size[leader(v)];
    }

    bool unite(int a, int b) {
        a = leader(a);
        b = leader(b);
        if (a == b) return false;
        if (component_size[a] < component_size[b]) swap(a, b);
        parent[b] = a;
        component_size[a] += component_size[b];
        return true;
    }
};

struct Random {
    mt19937_64 engine;

    explicit Random(uint64_t seed) : engine(seed) {}

    template <class Int>
    Int next_int(Int left, Int right) {
        assert(left < right);
        return uniform_int_distribution<Int>(left, right - 1)(engine);
    }
};

struct Operation {
    int first_row;
    int first_column;
    int second_row;
    int second_column;
};

struct Edge {
    int first_cell;
    int second_cell;
    int length;
    int type;
};

struct Plan {
    vector<Operation> moves;
    vector<Operation> connections;
    long long score = -1;
};

int cell_id(int row, int column, int n) {
    return row * n + column;
}

int visible_same_neighbors(
    const vector<string>& board,
    int row,
    int column,
    char type
) {
    const int n = static_cast<int>(board.size());
    static constexpr int DR[4] = {-1, 1, 0, 0};
    static constexpr int DC[4] = {0, 0, -1, 1};
    int result = 0;
    for (int direction = 0; direction < 4; ++direction) {
        int next_row = row + DR[direction];
        int next_column = column + DC[direction];
        while (
            0 <= next_row && next_row < n &&
            0 <= next_column && next_column < n
        ) {
            if (board[next_row][next_column] != '0') {
                if (board[next_row][next_column] == type) ++result;
                break;
            }
            next_row += DR[direction];
            next_column += DC[direction];
        }
    }
    return result;
}

// 1歩で同種の見通し本数が増える移動だけを行う。
vector<Operation> make_helpful_moves(
    vector<string>& board,
    int move_limit,
    Random& random
) {
    const int n = static_cast<int>(board.size());
    static constexpr int DR[4] = {-1, 1, 0, 0};
    static constexpr int DC[4] = {0, 0, -1, 1};
    vector<Operation> moves;

    while (static_cast<int>(moves.size()) < move_limit) {
        int best_gain = 0;
        int best_new_neighbors = 0;
        vector<Operation> best_candidates;

        for (int row = 0; row < n; ++row) {
            for (int column = 0; column < n; ++column) {
                const char type = board[row][column];
                if (type == '0') continue;
                const int old_neighbors = visible_same_neighbors(
                    board, row, column, type
                );

                for (int direction = 0; direction < 4; ++direction) {
                    const int next_row = row + DR[direction];
                    const int next_column = column + DC[direction];
                    if (
                        next_row < 0 || next_row >= n ||
                        next_column < 0 || next_column >= n ||
                        board[next_row][next_column] != '0'
                    ) {
                        continue;
                    }

                    board[row][column] = '0';
                    board[next_row][next_column] = type;
                    const int new_neighbors = visible_same_neighbors(
                        board, next_row, next_column, type
                    );
                    board[next_row][next_column] = '0';
                    board[row][column] = type;

                    const int gain = new_neighbors - old_neighbors;
                    if (
                        gain > best_gain ||
                        (gain == best_gain && gain > 0 &&
                         new_neighbors > best_new_neighbors)
                    ) {
                        best_gain = gain;
                        best_new_neighbors = new_neighbors;
                        best_candidates.clear();
                    }
                    if (gain == best_gain && gain > 0 &&
                        new_neighbors == best_new_neighbors) {
                        best_candidates.push_back(
                            {row, column, next_row, next_column}
                        );
                    }
                }
            }
        }

        if (best_candidates.empty()) break;
        const Operation chosen = best_candidates[
            random.next_int(0, static_cast<int>(best_candidates.size()))
        ];
        board[chosen.second_row][chosen.second_column] =
            board[chosen.first_row][chosen.first_column];
        board[chosen.first_row][chosen.first_column] = '0';
        moves.push_back(chosen);
    }
    return moves;
}

vector<Edge> make_visible_same_type_edges(const vector<string>& board) {
    const int n = static_cast<int>(board.size());
    vector<Edge> edges;

    for (int row = 0; row < n; ++row) {
        int previous_column = -1;
        for (int column = 0; column < n; ++column) {
            if (board[row][column] == '0') continue;
            if (
                previous_column != -1 &&
                board[row][previous_column] == board[row][column]
            ) {
                edges.push_back({
                    cell_id(row, previous_column, n),
                    cell_id(row, column, n),
                    column - previous_column,
                    board[row][column] - '0'
                });
            }
            previous_column = column;
        }
    }

    for (int column = 0; column < n; ++column) {
        int previous_row = -1;
        for (int row = 0; row < n; ++row) {
            if (board[row][column] == '0') continue;
            if (
                previous_row != -1 &&
                board[previous_row][column] == board[row][column]
            ) {
                edges.push_back({
                    cell_id(previous_row, column, n),
                    cell_id(row, column, n),
                    row - previous_row,
                    board[row][column] - '0'
                });
            }
            previous_row = row;
        }
    }
    return edges;
}

bool cable_is_free(
    const Edge& edge,
    const vector<unsigned char>& cable,
    int n
) {
    const int first_row = edge.first_cell / n;
    const int first_column = edge.first_cell % n;
    const int second_row = edge.second_cell / n;
    const int second_column = edge.second_cell % n;
    const int row_step = (second_row > first_row) - (second_row < first_row);
    const int column_step =
        (second_column > first_column) -
        (second_column < first_column);

    int row = first_row + row_step;
    int column = first_column + column_step;
    while (row != second_row || column != second_column) {
        if (cable[cell_id(row, column, n)]) return false;
        row += row_step;
        column += column_step;
    }
    return true;
}

void place_cable(
    const Edge& edge,
    vector<unsigned char>& cable,
    int n
) {
    const int first_row = edge.first_cell / n;
    const int first_column = edge.first_cell % n;
    const int second_row = edge.second_cell / n;
    const int second_column = edge.second_cell % n;
    const int row_step = (second_row > first_row) - (second_row < first_row);
    const int column_step =
        (second_column > first_column) -
        (second_column < first_column);

    int row = first_row + row_step;
    int column = first_column + column_step;
    while (row != second_row || column != second_column) {
        cable[cell_id(row, column, n)] = 1;
        row += row_step;
        column += column_step;
    }
}

Plan connect_greedily(
    const vector<string>& board,
    const vector<Operation>& moves,
    int operation_limit,
    int strategy,
    Random& random
) {
    const int n = static_cast<int>(board.size());
    vector<Edge> edges = make_visible_same_type_edges(board);
    vector<long long> tie_priority(edges.size());
    for (long long& value : tie_priority) {
        value = random.next_int(-500000LL, 500001LL);
    }

    Dsu dsu(n * n);
    vector<unsigned char> cable(n * n, 0);
    vector<unsigned char> unavailable(edges.size(), 0);
    vector<Operation> connections;
    const int connection_limit = operation_limit - static_cast<int>(moves.size());

    while (static_cast<int>(connections.size()) < connection_limit) {
        long long best_value = numeric_limits<long long>::min();
        int best_edge = -1;

        for (int index = 0; index < static_cast<int>(edges.size()); ++index) {
            if (unavailable[index]) continue;
            const Edge& edge = edges[index];
            if (dsu.same(edge.first_cell, edge.second_cell)) {
                unavailable[index] = 1;
                continue;
            }
            if (!cable_is_free(edge, cable, n)) {
                unavailable[index] = 1;
                continue;
            }

            const long long merge_gain =
                1LL * dsu.size(edge.first_cell) * dsu.size(edge.second_cell);
            long long value = merge_gain * 10000000LL;
            if (strategy % 3 == 0) value -= 2000LL * edge.length;
            if (strategy % 3 == 1) value += 2000LL * edge.length;
            if (strategy >= 3) value += tie_priority[index];
            if (strategy >= 6) {
                value += edge.type == strategy - 5 ? 800000LL : 0LL;
            }
            if (value > best_value) {
                best_value = value;
                best_edge = index;
            }
        }

        if (best_edge == -1) break;
        unavailable[best_edge] = 1;
        const Edge& edge = edges[best_edge];
        place_cable(edge, cable, n);
        dsu.unite(edge.first_cell, edge.second_cell);
        connections.push_back({
            edge.first_cell / n,
            edge.first_cell % n,
            edge.second_cell / n,
            edge.second_cell % n
        });
    }

    long long score = 0;
    for (int row = 0; row < n; ++row) {
        for (int column = 0; column < n; ++column) {
            if (board[row][column] == '0') continue;
            const int cell = cell_id(row, column, n);
            if (dsu.leader(cell) != cell) continue;
            const long long size = dsu.size(cell);
            score += size * (size - 1) / 2;
        }
    }

    return {moves, connections, score};
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, type_count;
    cin >> n >> type_count;
    vector<string> original_board(n);
    uint64_t input_hash = 1469598103934665603ULL;
    for (string& row : original_board) {
        cin >> row;
        for (char value : row) {
            input_hash ^= static_cast<unsigned char>(value);
            input_hash *= 1099511628211ULL;
        }
    }

    Random random(input_hash);
    const int operation_limit = 100 * type_count;
    Plan best_plan;

    // 移動なしを必ず含め、少量の局所移動を許す複数案を比較する。
    vector<int> move_limits{0, 5, 10, 20, 30, 40};
    for (int move_limit : move_limits) {
        vector<string> board = original_board;
        vector<Operation> moves = make_helpful_moves(
            board,
            min(move_limit, operation_limit / 4),
            random
        );

        const int strategy_count = 6 + type_count;
        long long move_limit_best = -1;
        for (int strategy = 0; strategy < strategy_count; ++strategy) {
            Plan candidate = connect_greedily(
                board, moves, operation_limit, strategy, random
            );
            if (candidate.score > best_plan.score) {
                best_plan = move(candidate);
            }
            move_limit_best = max(move_limit_best, candidate.score);
        }
#ifdef LOCAL
        cerr << "move_limit=" << move_limit
             << " actual_moves=" << moves.size()
             << " score=" << move_limit_best << '\n';
#endif
    }

    cout << best_plan.moves.size() << '\n';
    for (const Operation& move : best_plan.moves) {
        cout << move.first_row << ' ' << move.first_column << ' '
             << move.second_row << ' ' << move.second_column << '\n';
    }
    cout << best_plan.connections.size() << '\n';
    for (const Operation& connection : best_plan.connections) {
        cout << connection.first_row << ' ' << connection.first_column << ' '
             << connection.second_row << ' ' << connection.second_column
             << '\n';
    }
    return 0;
}
