#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <tuple>
#include <utility>
#include <vector>

using namespace std;

// A tiny deterministic random-number generator.  A fixed seed makes local
// experiments reproducible.
uint64_t random_state = 0x123456789abcdef0ULL;

uint64_t next_random() {
    random_state += 0x9e3779b97f4a7c15ULL;
    uint64_t z = random_state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

struct Offset {
    int y;
    int x;
    int move_cost;
};

// Expected value after adding normal noise and clipping to [0, 1000].
// Rounding changes this by much less than one and is ignored here.
double expected_measurement(double temperature, double noise) {
    const double inv_sqrt_2 = 1.0 / sqrt(2.0);
    const double inv_sqrt_2pi = 1.0 / sqrt(2.0 * acos(-1.0));
    const auto normal_cdf = [&](double z) {
        return 0.5 * erfc(-z * inv_sqrt_2);
    };
    const auto normal_pdf = [&](double z) {
        return inv_sqrt_2pi * exp(-0.5 * z * z);
    };

    const double a = -temperature / noise;
    const double b = (1000.0 - temperature) / noise;
    const double inside = normal_cdf(b) - normal_cdf(a);
    return temperature * inside
         + noise * (normal_pdf(a) - normal_pdf(b))
         + 1000.0 * (1.0 - normal_cdf(b));
}

// Minimum-cost perfect matching.  The returned vector says which column is
// assigned to each row.  This also enforces that every exit is used once.
vector<int> hungarian(const vector<vector<double>>& cost) {
    const int n = static_cast<int>(cost.size());
    const double inf = numeric_limits<double>::infinity();
    vector<double> u(n + 1), v(n + 1);
    vector<int> p(n + 1), way(n + 1);

    for (int row = 1; row <= n; ++row) {
        p[0] = row;
        int column0 = 0;
        vector<double> min_value(n + 1, inf);
        vector<char> used(n + 1, false);
        do {
            used[column0] = true;
            const int row0 = p[column0];
            double delta = inf;
            int column1 = 0;
            for (int column = 1; column <= n; ++column) {
                if (used[column]) continue;
                const double current = cost[row0 - 1][column - 1]
                                     - u[row0] - v[column];
                if (current < min_value[column]) {
                    min_value[column] = current;
                    way[column] = column0;
                }
                if (min_value[column] < delta) {
                    delta = min_value[column];
                    column1 = column;
                }
            }
            for (int column = 0; column <= n; ++column) {
                if (used[column]) {
                    u[p[column]] += delta;
                    v[column] -= delta;
                } else {
                    min_value[column] -= delta;
                }
            }
            column0 = column1;
        } while (p[column0] != 0);

        do {
            const int column1 = way[column0];
            p[column0] = p[column1];
            column0 = column1;
        } while (column0 != 0);
    }

    vector<int> answer(n);
    for (int column = 1; column <= n; ++column) {
        answer[p[column] - 1] = column - 1;
    }
    return answer;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int L, N, S;
    cin >> L >> N >> S;
    vector<int> exit_y(N), exit_x(N);
    for (int i = 0; i < N; ++i) cin >> exit_y[i] >> exit_x[i];

    // We only consider inexpensive torus moves.  A larger pool gives the
    // adaptive measurements enough choices without making preparation slow.
    vector<Offset> offsets;
    for (int yy = 0; yy < L; ++yy) {
        const int dy = (yy <= L / 2 ? yy : yy - L);
        for (int xx = 0; xx < L; ++xx) {
            const int dx = (xx <= L / 2 ? xx : xx - L);
            offsets.push_back({dy, dx, abs(dy) + abs(dx)});
        }
    }
    sort(offsets.begin(), offsets.end(), [](const Offset& a, const Offset& b) {
        return tie(a.move_cost, a.y, a.x) < tie(b.move_cost, b.y, b.x);
    });
    if (static_cast<int>(offsets.size()) > 320) offsets.resize(320);
    const int C = static_cast<int>(offsets.size());

    int first_queries;
    int queries_per_entrance;
    if (S <= 4) {
        first_queries = 10;
        queries_per_entrance = 10;
    } else if (S <= 16) {
        first_queries = 12;
        queries_per_entrance = 16;
    } else if (S <= 49) {
        first_queries = 14;
        queries_per_entrance = 24;
    } else if (S <= 100) {
        first_queries = 16;
        queries_per_entrance = 40;
    } else if (S <= 225) {
        first_queries = 18;
        queries_per_entrance = 70;
    } else if (S <= 400) {
        first_queries = 20;
        queries_per_entrance = min(90, 10000 / N);
    } else {
        first_queries = 22;
        queries_per_entrance = 10000 / N;
    }
    first_queries = min(first_queries, C);
    queries_per_entrance = max(first_queries, queries_per_entrance);

    // Try several random black/white temperature patterns.  For each one,
    // greedily choose offsets that separate the still-similar exit pairs.
    vector<vector<int>> best_field;
    vector<vector<unsigned char>> best_bits;
    vector<int> best_initial_offsets;
    int best_min_distance = -1;
    double best_risk = numeric_limits<double>::infinity();
    int best_boundaries = numeric_limits<int>::max();
    vector<double> distance_weight(first_queries + 1);
    for (int d = 0; d <= first_queries; ++d) {
        distance_weight[d] = exp(-0.72 * d);
    }

    for (int trial = 0; trial < 4; ++trial) {
        vector<vector<int>> field(L, vector<int>(L));
        for (int y = 0; y < L; ++y) {
            for (int x = 0; x < L; ++x) {
                field[y][x] = static_cast<int>(next_random() & 1ULL);
            }
        }

        vector<vector<unsigned char>> bits(C, vector<unsigned char>(N));
        for (int c = 0; c < C; ++c) {
            for (int j = 0; j < N; ++j) {
                const int y = (exit_y[j] + offsets[c].y + L) % L;
                const int x = (exit_x[j] + offsets[c].x + L) % L;
                bits[c][j] = static_cast<unsigned char>(field[y][x]);
            }
        }

        vector<vector<int>> distance(N, vector<int>(N));
        vector<char> used(C, false);
        vector<int> chosen;
        chosen.reserve(first_queries);
        for (int step = 0; step < first_queries; ++step) {
            int best_c = -1;
            double best_gain = -1.0;
            for (int c = 0; c < C; ++c) {
                if (used[c]) continue;
                double gain = 0.0;
                for (int a = 0; a < N; ++a) {
                    for (int b = a + 1; b < N; ++b) {
                        if (bits[c][a] != bits[c][b]) {
                            gain += distance_weight[distance[a][b]];
                        }
                    }
                }
                gain -= 0.02 * offsets[c].move_cost;
                if (gain > best_gain) {
                    best_gain = gain;
                    best_c = c;
                }
            }
            used[best_c] = true;
            chosen.push_back(best_c);
            for (int a = 0; a < N; ++a) {
                for (int b = a + 1; b < N; ++b) {
                    distance[a][b] += (bits[best_c][a] != bits[best_c][b]);
                }
            }
        }

        int min_distance = first_queries;
        double risk = 0.0;
        for (int a = 0; a < N; ++a) {
            for (int b = a + 1; b < N; ++b) {
                min_distance = min(min_distance, distance[a][b]);
                risk += distance_weight[distance[a][b]];
            }
        }
        int boundaries = 0;
        for (int y = 0; y < L; ++y) {
            for (int x = 0; x < L; ++x) {
                boundaries += (field[y][x] != field[(y + 1) % L][x]);
                boundaries += (field[y][x] != field[y][(x + 1) % L]);
            }
        }

        if (min_distance > best_min_distance
            || (min_distance == best_min_distance && risk < best_risk)
            || (min_distance == best_min_distance
                && abs(risk - best_risk) < 1e-9
                && boundaries < best_boundaries)) {
            best_min_distance = min_distance;
            best_risk = risk;
            best_boundaries = boundaries;
            best_field = move(field);
            best_bits = move(bits);
            best_initial_offsets = move(chosen);
        }
    }

    // A larger measurement budget permits a smaller temperature difference.
    // Accuracy is still prioritized because every wrong correspondence costs
    // a factor of 0.8 in the score.
    int amplitude = static_cast<int>(ceil(
        3.5 * S / sqrt(max(1.0, queries_per_entrance / 3.0))));
    amplitude = clamp(amplitude, 8, 500);
    const int low_temperature = 500 - amplitude;
    const int high_temperature = 500 + amplitude;

    vector<vector<int>> temperature(L, vector<int>(L));
    for (int y = 0; y < L; ++y) {
        for (int x = 0; x < L; ++x) {
            temperature[y][x] = best_field[y][x]
                ? high_temperature : low_temperature;
            if (x) cout << ' ';
            cout << temperature[y][x];
        }
        cout << '\n';
    }
    cout << flush;

    const double expected_low = expected_measurement(low_temperature, S);
    const double expected_high = expected_measurement(high_temperature, S);
    const double expected_value[2] = {expected_low, expected_high};

    vector<vector<double>> total_loss(N, vector<double>(N));
    auto measure = [&](int entrance, int offset_index) {
        cout << entrance << ' ' << offsets[offset_index].y
             << ' ' << offsets[offset_index].x << '\n' << flush;
        int measured;
        cin >> measured;
        if (measured == -1) exit(0);
        for (int candidate = 0; candidate < N; ++candidate) {
            const double difference = measured
                                    - expected_value[best_bits[offset_index][candidate]];
            total_loss[entrance][candidate] += difference * difference;
        }
    };

    for (int entrance = 0; entrance < N; ++entrance) {
        for (int c : best_initial_offsets) measure(entrance, c);

        for (int turn = first_queries; turn < queries_per_entrance; ++turn) {
            // Only the most plausible exits matter when choosing the next
            // question.  Limiting this list is both faster and more robust
            // than letting many already-bad candidates affect the split.
            vector<int> plausible(N);
            iota(plausible.begin(), plausible.end(), 0);
            const int plausible_count = min(12, N);
            partial_sort(plausible.begin(), plausible.begin() + plausible_count,
                         plausible.end(), [&](int a, int b) {
                return total_loss[entrance][a] < total_loss[entrance][b];
            });
            const double best = total_loss[entrance][plausible[0]];
            const double scale = max(1.0, 2.0 * S * S);
            vector<double> weight(plausible_count);
            double weight_sum = 0.0;
            for (int rank = 0; rank < plausible_count; ++rank) {
                const int j = plausible[rank];
                weight[rank] = exp(-min(40.0,
                    (total_loss[entrance][j] - best) / scale));
                weight_sum += weight[rank];
            }

            int chosen_offset = 0;
            double chosen_value = -1.0;
            for (int c = 0; c < C; ++c) {
                double one_weight = 0.0;
                for (int rank = 0; rank < plausible_count; ++rank) {
                    if (best_bits[c][plausible[rank]]) {
                        one_weight += weight[rank];
                    }
                }
                const double split = one_weight * (weight_sum - one_weight);
                const double value = split / (1.0 + 0.025 * offsets[c].move_cost);
                if (value > chosen_value) {
                    chosen_value = value;
                    chosen_offset = c;
                }
            }
            measure(entrance, chosen_offset);
        }
    }

    const vector<int> answer = hungarian(total_loss);
    cout << "-1 -1 -1\n";
    for (int value : answer) cout << value << '\n';
    cout << flush;
    return 0;
}
