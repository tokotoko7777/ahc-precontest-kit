#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <vector>

using namespace std;

// 公式テスタとの単純比較用です。
// g++ ... -DSIMPLE_BASELINE main.cpp とすると、毎回 0～35 を順に植えます。

#ifndef SEARCH_STEPS
#define SEARCH_STEPS 12000
#endif

#ifndef RISK_WEIGHT
#define RISK_WEIGHT 0.70
#endif

#ifndef COVERAGE_WEIGHT
#define COVERAGE_WEIGHT 4.0
#endif

#ifndef RARE_DEGREE_WEIGHT
#define RARE_DEGREE_WEIGHT 2.5
#endif

struct XorShift {
    uint64_t state;

    explicit XorShift(uint64_t seed) : state(seed) {
        if (state == 0) state = 0x123456789abcdefULL;
    }

    uint64_t next() {
        state ^= state << 7;
        state ^= state >> 9;
        return state;
    }

    int next_int(int upper) {
        return static_cast<int>(next() % static_cast<uint64_t>(upper));
    }

    double next_double() {
        return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0);
    }
};

struct Planner {
    int n;
    int m;
    int seed_count;
    vector<vector<int>> value;
    vector<pair<int, int>> edges;
    vector<int> degree;
    vector<int> total;
    vector<double> rare_bonus;
    vector<vector<double>> pair_score;

    Planner(int board_size, int dimension, const vector<vector<int>>& seeds)
        : n(board_size),
          m(dimension),
          seed_count(static_cast<int>(seeds.size())),
          value(seeds),
          degree(n * n, 0),
          total(seed_count, 0),
          rare_bonus(seed_count, 0.0),
          pair_score(seed_count, vector<double>(seed_count, 0.0)) {
        make_edges();
        prepare_scores();
    }

    void make_edges() {
        for (int row = 0; row < n; ++row) {
            for (int col = 0; col < n; ++col) {
                int cell = row * n + col;
                if (col + 1 < n) {
                    int next_cell = cell + 1;
                    edges.push_back({cell, next_cell});
                    ++degree[cell];
                    ++degree[next_cell];
                }
                if (row + 1 < n) {
                    int next_cell = cell + n;
                    edges.push_back({cell, next_cell});
                    ++degree[cell];
                    ++degree[next_cell];
                }
            }
        }
    }

    void prepare_scores() {
        for (int seed = 0; seed < seed_count; ++seed) {
            total[seed] = accumulate(value[seed].begin(), value[seed].end(), 0);
        }

        // 各成分の最高値と2番目の値を調べます。
        // 最高値と2番目の差が大きい成分ほど、失うと取り戻せません。
        for (int component = 0; component < m; ++component) {
            int best = -1;
            int second = -1;
            for (int seed = 0; seed < seed_count; ++seed) {
                int x = value[seed][component];
                if (x > best) {
                    second = best;
                    best = x;
                } else if (x > second) {
                    second = x;
                }
            }
            int gap = max(1, best - second);
            for (int seed = 0; seed < seed_count; ++seed) {
                if (value[seed][component] == best) {
                    rare_bonus[seed] += gap;
                }
            }
        }

        // 子の合計値の期待値は両親の平均です。
        // 成分差がある親同士には、運よく良い側を多く受け継ぐ上振れもあります。
        for (int left = 0; left < seed_count; ++left) {
            for (int right = left + 1; right < seed_count; ++right) {
                double squared_difference = 0.0;
                for (int component = 0; component < m; ++component) {
                    double difference =
                        static_cast<double>(value[left][component] - value[right][component]);
                    squared_difference += difference * difference;
                }
                double expected = 0.5 * static_cast<double>(total[left] + total[right]);
                double upper_chance = RISK_WEIGHT * sqrt(squared_difference);
                pair_score[left][right] = expected + upper_chance;
                pair_score[right][left] = pair_score[left][right];
            }
        }
    }

    double evaluate(const vector<int>& board) const {
        double score = 0.0;

        for (const auto& edge : edges) {
            score += pair_score[board[edge.first]][board[edge.second]];
        }

        // 今の世代に存在する各成分の最高値を、なるべく全て親に残します。
        for (int component = 0; component < m; ++component) {
            int best_on_board = 0;
            for (int cell = 0; cell < n * n; ++cell) {
                best_on_board = max(best_on_board, value[board[cell]][component]);
            }
            score += COVERAGE_WEIGHT * static_cast<double>(best_on_board);
        }

        // 希少な最高成分を持つ種は、子を多く作れる中央へ置く価値があります。
        for (int cell = 0; cell < n * n; ++cell) {
            score += RARE_DEGREE_WEIGHT * static_cast<double>(degree[cell]) *
                     rare_bonus[board[cell]];
        }

        return score;
    }

    vector<int> initial_board() const {
        vector<int> seeds(seed_count);
        iota(seeds.begin(), seeds.end(), 0);
        sort(seeds.begin(), seeds.end(), [&](int left, int right) {
            double left_score = static_cast<double>(total[left]) + 3.0 * rare_bonus[left];
            double right_score = static_cast<double>(total[right]) + 3.0 * rare_bonus[right];
            if (left_score != right_score) return left_score > right_score;
            return left < right;
        });

        vector<int> cells(n * n);
        iota(cells.begin(), cells.end(), 0);
        sort(cells.begin(), cells.end(), [&](int left, int right) {
            if (degree[left] != degree[right]) return degree[left] > degree[right];
            return left < right;
        });

        vector<int> board(n * n, -1);
        for (int index = 0; index < n * n; ++index) {
            board[cells[index]] = seeds[index];
        }
        return board;
    }

    vector<int> solve(uint64_t random_seed) const {
        vector<int> board = initial_board();
        vector<char> used(seed_count, false);
        for (int seed : board) used[seed] = true;

        XorShift random(random_seed);
        double current_score = evaluate(board);
        double best_score = current_score;
        vector<int> best_board = board;

        for (int step = 0; step < SEARCH_STEPS; ++step) {
            int first_cell = random.next_int(n * n);
            int second_cell = -1;
            int old_seed = board[first_cell];
            int new_seed = -1;
            bool replacement = (random.next() % 4 == 0);

            if (replacement) {
                do {
                    new_seed = random.next_int(seed_count);
                } while (used[new_seed]);
                board[first_cell] = new_seed;
                used[old_seed] = false;
                used[new_seed] = true;
            } else {
                do {
                    second_cell = random.next_int(n * n);
                } while (second_cell == first_cell);
                swap(board[first_cell], board[second_cell]);
            }

            double next_score = evaluate(board);
            double progress = static_cast<double>(step) / static_cast<double>(SEARCH_STEPS);
            double temperature = 20.0 * (1.0 - progress) + 0.05;
            double difference = next_score - current_score;
            bool accept = difference >= 0.0 ||
                          random.next_double() < exp(difference / temperature);

            if (accept) {
                current_score = next_score;
                if (current_score > best_score) {
                    best_score = current_score;
                    best_board = board;
                }
            } else if (replacement) {
                board[first_cell] = old_seed;
                used[new_seed] = false;
                used[old_seed] = true;
            } else {
                swap(board[first_cell], board[second_cell]);
            }
        }

        return best_board;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, m, turn_count;
    cin >> n >> m >> turn_count;
    int seed_count = 2 * n * (n - 1);
    vector<vector<int>> seeds(seed_count, vector<int>(m));
    for (auto& seed : seeds) {
        for (int& x : seed) cin >> x;
    }

    for (int turn = 0; turn < turn_count; ++turn) {
        vector<int> board(n * n);

#ifdef SIMPLE_BASELINE
        iota(board.begin(), board.end(), 0);
#else
        Planner planner(n, m, seeds);
        uint64_t random_seed = 0x9e3779b97f4a7c15ULL ^ static_cast<uint64_t>(turn + 1);
        for (const auto& seed : seeds) {
            for (int x : seed) {
                random_seed ^= static_cast<uint64_t>(x + 1);
                random_seed *= 0xbf58476d1ce4e5b9ULL;
            }
        }
        board = planner.solve(random_seed);
#endif

        for (int row = 0; row < n; ++row) {
            for (int col = 0; col < n; ++col) {
                if (col > 0) cout << ' ';
                cout << board[row * n + col];
            }
            cout << '\n';
        }
        cout << flush;

        // 公式テスタは最終ターンの後にも次世代を送ります。
        for (auto& seed : seeds) {
            for (int& x : seed) cin >> x;
        }
    }

    return 0;
}
