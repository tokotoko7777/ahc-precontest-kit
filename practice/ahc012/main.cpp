#pragma GCC optimize("O3,unroll-loops")
#include <bits/stdc++.h>
using namespace std;

struct Timer {
    chrono::steady_clock::time_point start = chrono::steady_clock::now();

    double elapsed_ms() const {
        return chrono::duration<double, milli>(
                   chrono::steady_clock::now() - start)
            .count();
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

    double next_double() {
        return uniform_real_distribution<double>(0.0, 1.0)(engine);
    }
};

struct Point {
    int x;
    int y;
};

struct Evaluation {
    double search_score = -1e100;
    int matched_pieces = 0;
    array<int, 11> piece_count{};
};

// x_cutsの値cは、実際にはx=c+0.5付近の直線として出力する。
// したがって整数座標の点xは、c<xの切断線の本数で帯が決まる。
Evaluation evaluate_grid(
    const vector<int>& x_cuts,
    const vector<int>& y_cuts,
    const vector<Point>& points,
    const array<int, 11>& wanted
) {
    const int y_groups = static_cast<int>(y_cuts.size()) + 1;
    vector<int> cell_count(
        (static_cast<int>(x_cuts.size()) + 1) * y_groups,
        0
    );

    for (const Point& point : points) {
        const int x_group = static_cast<int>(
            lower_bound(x_cuts.begin(), x_cuts.end(), point.x) -
            x_cuts.begin()
        );
        const int y_group = static_cast<int>(
            lower_bound(y_cuts.begin(), y_cuts.end(), point.y) -
            y_cuts.begin()
        );
        ++cell_count[x_group * y_groups + y_group];
    }

    Evaluation result;
    int large_piece_points = 0;
    for (int count : cell_count) {
        if (1 <= count && count <= 10) {
            ++result.piece_count[count];
        } else if (count > 10) {
            large_piece_points += count - 10;
        }
    }

    double deficit_square = 0.0;
    double surplus_square = 0.0;
    int cumulative_difference = 0;
    int transport_distance = 0;
    for (int size = 1; size <= 10; ++size) {
        const int made = result.piece_count[size];
        result.matched_pieces += min(wanted[size], made);
        const int difference = made - wanted[size];
        if (difference < 0) {
            deficit_square +=
                1.0 * difference * difference / (wanted[size] + 5);
        } else {
            surplus_square +=
                1.0 * difference * difference / (wanted[size] + 5);
        }
        cumulative_difference += difference;
        transport_distance += abs(cumulative_difference);
    }

    // 公式値を最優先し、同点では「隣の大きさへ直しやすい分布」を選ぶ。
    result.search_score =
        10000.0 * result.matched_pieces -
        7.0 * deficit_square -
        2.0 * surplus_square -
        0.4 * transport_distance -
        0.05 * large_piece_points;
    return result;
}

vector<int> make_quantile_cuts(
    vector<int> coordinates,
    int group_count,
    int shift
) {
    sort(coordinates.begin(), coordinates.end());
    vector<int> cuts;
    cuts.reserve(group_count - 1);
    set<int> used;

    for (int group = 1; group < group_count; ++group) {
        int index = static_cast<int>(
            (1LL * group * coordinates.size() + shift) / group_count
        );
        index = clamp(index, 1, static_cast<int>(coordinates.size()) - 1);
        int cut = coordinates[index] - 1;
        cut = clamp(cut, -10000, 9999);

        // 同じx座標が続く場合などは、一番近い未使用の半整数境界へずらす。
        if (used.count(cut)) {
            for (int distance = 1; ; ++distance) {
                if (cut - distance >= -10000 && !used.count(cut - distance)) {
                    cut -= distance;
                    break;
                }
                if (cut + distance <= 9999 && !used.count(cut + distance)) {
                    cut += distance;
                    break;
                }
            }
        }
        used.insert(cut);
        cuts.push_back(cut);
    }
    sort(cuts.begin(), cuts.end());
    return cuts;
}

bool move_one_cut(vector<int>& cuts, Random& random) {
    if (cuts.empty()) return false;
    const int index = random.next_int(0, static_cast<int>(cuts.size()));

    if (random.next_int(0, 100) < 24) {
        // 一度削除して好きな順位へ戻すrelocate。境界同士を追い越せる。
        cuts.erase(cuts.begin() + index);
        for (int trial = 0; trial < 20; ++trial) {
            const int value = random.next_int(-10000, 10000);
            const auto position = lower_bound(cuts.begin(), cuts.end(), value);
            if (position == cuts.end() || *position != value) {
                cuts.insert(position, value);
                return true;
            }
        }
        cuts.insert(
            lower_bound(cuts.begin(), cuts.end(), -10000),
            -10000
        );
        return true;
    }

    const int lower = index == 0 ? -10000 : cuts[index - 1] + 1;
    const int upper = index + 1 == static_cast<int>(cuts.size())
        ? 9999
        : cuts[index + 1] - 1;
    if (lower > upper) return false;

    int next_value;
    if (random.next_int(0, 100) < 82) {
        const int radius = random.next_int(1, 401);
        next_value = cuts[index] + random.next_int(-radius, radius + 1);
    } else {
        next_value = random.next_int(lower, upper + 1);
    }
    cuts[index] = clamp(next_value, lower, upper);
    return true;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, maximum_cuts;
    cin >> n >> maximum_cuts;
    array<int, 11> wanted{};
    int wanted_pieces = 0;
    for (int size = 1; size <= 10; ++size) {
        cin >> wanted[size];
        wanted_pieces += wanted[size];
    }

    vector<Point> points(n);
    vector<int> x_coordinates(n);
    vector<int> y_coordinates(n);
    uint64_t input_hash = 1469598103934665603ULL;
    for (int i = 0; i < n; ++i) {
        cin >> points[i].x >> points[i].y;
        x_coordinates[i] = points[i].x;
        y_coordinates[i] = points[i].y;
        input_hash ^= static_cast<uint32_t>(points[i].x + 10000);
        input_hash *= 1099511628211ULL;
        input_hash ^= static_cast<uint32_t>(points[i].y + 10000);
        input_hash *= 1099511628211ULL;
    }

    Random random(input_hash);
    Timer timer;
    constexpr double SEARCH_END_MS = 1870.0;

    vector<int> best_x_cuts;
    vector<int> best_y_cuts;
    Evaluation best_evaluation;

    // 目標piece数の約1.0〜1.4倍の格子を試す。円盤の角には空cellができるため、
    // 長方形cell総数を少し多めに用意した候補も必要になる。
    for (int factor_percent : {95, 105, 115, 125, 135, 145}) {
        const int target_cells = max(
            4,
            (wanted_pieces * factor_percent + 50) / 100
        );
        for (int x_groups = 2; x_groups <= maximum_cuts + 1; ++x_groups) {
            int y_groups = max(2, (target_cells + x_groups / 2) / x_groups);
            if (x_groups + y_groups - 2 > maximum_cuts) continue;
            if (y_groups > maximum_cuts + 1) continue;

            for (int shift_kind = 0; shift_kind < 3; ++shift_kind) {
                const int x_shift = shift_kind * n / 3;
                const int y_shift = (2 - shift_kind) * n / 3;
                vector<int> x_cuts = make_quantile_cuts(
                    x_coordinates, x_groups, x_shift
                );
                vector<int> y_cuts = make_quantile_cuts(
                    y_coordinates, y_groups, y_shift
                );
                const Evaluation evaluation = evaluate_grid(
                    x_cuts, y_cuts, points, wanted
                );
                if (evaluation.search_score > best_evaluation.search_score) {
                    best_evaluation = evaluation;
                    best_x_cuts.swap(x_cuts);
                    best_y_cuts.swap(y_cuts);
                }
            }
        }
    }

    vector<int> current_x_cuts = best_x_cuts;
    vector<int> current_y_cuts = best_y_cuts;
    Evaluation current_evaluation = best_evaluation;
    [[maybe_unused]] const int initial_matched_pieces =
        best_evaluation.matched_pieces;

    constexpr double START_TEMPERATURE = 24000.0;
    constexpr double END_TEMPERATURE = 20.0;
    int iteration = 0;
    double temperature = START_TEMPERATURE;

    while (true) {
        if ((iteration & 63) == 0) {
            const double elapsed = timer.elapsed_ms();
            if (elapsed >= SEARCH_END_MS) break;
            const double progress = clamp(elapsed / SEARCH_END_MS, 0.0, 1.0);
            temperature = START_TEMPERATURE * pow(
                END_TEMPERATURE / START_TEMPERATURE,
                progress
            );
        }
        ++iteration;

        const bool change_x = random.next_int(0, 2) == 0;
        vector<int>& cuts = change_x ? current_x_cuts : current_y_cuts;
        if (cuts.empty()) continue;
        const vector<int> backup = cuts;
        if (!move_one_cut(cuts, random)) continue;

        const Evaluation candidate = evaluate_grid(
            current_x_cuts, current_y_cuts, points, wanted
        );
        const double improvement =
            candidate.search_score - current_evaluation.search_score;
        const bool accept = improvement >= 0.0 ||
            random.next_double() < exp(improvement / temperature);
        if (accept) {
            current_evaluation = candidate;
            if (candidate.search_score > best_evaluation.search_score) {
                best_evaluation = candidate;
                best_x_cuts = current_x_cuts;
                best_y_cuts = current_y_cuts;
            }
        } else {
            cuts = backup;
        }
    }

    cout << best_x_cuts.size() + best_y_cuts.size() << '\n';
    for (int cut : best_x_cuts) {
        // cake上では x=cut+0.5±0.000005。整数座標の苺を切らず、縦線として働く。
        cout << cut << ' ' << -1000000000 << ' '
             << cut + 1 << ' ' << 1000000000 << '\n';
    }
    for (int cut : best_y_cuts) {
        // cake上では y=cut+0.5±0.000005。
        cout << -1000000000 << ' ' << cut << ' '
             << 1000000000 << ' ' << cut + 1 << '\n';
    }

#ifdef LOCAL
    cerr << "iterations=" << iteration
         << " cuts=" << best_x_cuts.size() + best_y_cuts.size()
         << " initial_matched=" << initial_matched_pieces
         << " matched=" << best_evaluation.matched_pieces << '/'
         << wanted_pieces << '\n';
#endif
    return 0;
}
