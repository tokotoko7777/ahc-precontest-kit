#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <tuple>
#include <vector>
using namespace std;

constexpr int MAX_K = 20;
constexpr int MAX_RECIPE_SIZE = 48;
constexpr int MAX_GROUP_SIZE = 24;

struct Color {
    double c = 0.0;
    double m = 0.0;
    double y = 0.0;
};

Color operator+(Color a, const Color& b) {
    a.c += b.c;
    a.m += b.m;
    a.y += b.y;
    return a;
}

Color operator-(Color a, const Color& b) {
    a.c -= b.c;
    a.m -= b.m;
    a.y -= b.y;
    return a;
}

Color operator*(Color a, double scale) {
    a.c *= scale;
    a.m *= scale;
    a.y *= scale;
    return a;
}

Color operator/(Color a, double scale) {
    return a * (1.0 / scale);
}

double squared_distance(const Color& a, const Color& b) {
    double dc = a.c - b.c;
    double dm = a.m - b.m;
    double dy = a.y - b.y;
    return dc * dc + dm * dm + dy * dy;
}

double color_distance(const Color& a, const Color& b) {
    return sqrt(squared_distance(a, b));
}

using Counts = array<unsigned short, MAX_K>;

struct Mix {
    Counts count{};
    Color color;
    double error = numeric_limits<double>::infinity();
};

Color sum_of_recipe(const Counts& count, const vector<Color>& tube) {
    Color sum;
    for (int k = 0; k < (int)tube.size(); ++k) {
        sum = sum + tube[k] * count[k];
    }
    return sum;
}

void improve_recipe(Counts& count, Color& sum, int amount,
                    const Color& wanted, const vector<Color>& tube,
                    int max_rounds = 20) {
    const int k_count = (int)tube.size();
    for (int round = 0; round < max_rounds; ++round) {
        double best_distance = squared_distance(sum / amount, wanted);
        int best_remove = -1;
        int best_add = -1;
        Color best_sum = sum;
        for (int remove = 0; remove < k_count; ++remove) {
            if (count[remove] == 0) continue;
            for (int add = 0; add < k_count; ++add) {
                if (add == remove) continue;
                Color candidate_sum = sum - tube[remove] + tube[add];
                double candidate_distance =
                    squared_distance(candidate_sum / amount, wanted);
                if (candidate_distance + 1e-18 < best_distance) {
                    best_distance = candidate_distance;
                    best_remove = remove;
                    best_add = add;
                    best_sum = candidate_sum;
                }
            }
        }
        if (best_remove == -1) break;
        --count[best_remove];
        ++count[best_add];
        sum = best_sum;
    }
}

Mix make_greedy_mix(const Color& wanted, int amount,
                    const vector<Color>& tube) {
    Mix result;
    Color sum;
    for (int used = 0; used < amount; ++used) {
        int best_tube = 0;
        double best_distance = numeric_limits<double>::infinity();
        for (int k = 0; k < (int)tube.size(); ++k) {
            double candidate_distance =
                squared_distance((sum + tube[k]) / (used + 1), wanted);
            if (candidate_distance < best_distance) {
                best_distance = candidate_distance;
                best_tube = k;
            }
        }
        ++result.count[best_tube];
        sum = sum + tube[best_tube];
    }
    improve_recipe(result.count, sum, amount, wanted, tube);
    result.color = sum / amount;
    result.error = color_distance(result.color, wanted);
    return result;
}

vector<Mix> make_single_target_options(const Color& wanted, int max_amount,
                                       const vector<Color>& tube) {
    vector<Mix> option(max_amount + 1);
    const int k_count = (int)tube.size();

    if (max_amount >= 1) {
        for (int a = 0; a < k_count; ++a) {
            double error = color_distance(tube[a], wanted);
            if (error < option[1].error) {
                option[1] = Mix();
                option[1].count[a] = 1;
                option[1].color = tube[a];
                option[1].error = error;
            }
        }
    }
    if (max_amount >= 2) {
        for (int a = 0; a < k_count; ++a) {
            for (int b = a; b < k_count; ++b) {
                Color color = (tube[a] + tube[b]) / 2.0;
                double error = color_distance(color, wanted);
                if (error < option[2].error) {
                    option[2] = Mix();
                    ++option[2].count[a];
                    ++option[2].count[b];
                    option[2].color = color;
                    option[2].error = error;
                }
            }
        }
    }
    if (max_amount >= 3) {
        for (int a = 0; a < k_count; ++a) {
            for (int b = a; b < k_count; ++b) {
                for (int c = b; c < k_count; ++c) {
                    Color color = (tube[a] + tube[b] + tube[c]) / 3.0;
                    double error = color_distance(color, wanted);
                    if (error < option[3].error) {
                        option[3] = Mix();
                        ++option[3].count[a];
                        ++option[3].count[b];
                        ++option[3].count[c];
                        option[3].color = color;
                        option[3].error = error;
                    }
                }
            }
        }
    }

    for (int amount = 4; amount <= max_amount; ++amount) {
        Counts count = option[amount - 1].count;
        Color sum = sum_of_recipe(count, tube);
        int best_tube = 0;
        double best_distance = numeric_limits<double>::infinity();
        for (int k = 0; k < k_count; ++k) {
            double candidate_distance =
                squared_distance((sum + tube[k]) / amount, wanted);
            if (candidate_distance < best_distance) {
                best_distance = candidate_distance;
                best_tube = k;
            }
        }
        ++count[best_tube];
        sum = sum + tube[best_tube];
        improve_recipe(count, sum, amount, wanted, tube);
        option[amount].count = count;
        option[amount].color = sum / amount;
        option[amount].error = color_distance(option[amount].color, wanted);
    }
    return option;
}

long long predicted_score(double total_error, long long extra_paint, int d) {
    return 1 + extra_paint * d + llround(10000.0 * total_error);
}

struct IndependentPlan {
    vector<int> amount;
    vector<vector<Mix>> option;
    double error = 0.0;
    long long score = numeric_limits<long long>::max();
};

IndependentPlan make_independent_plan(const vector<Color>& target,
                                      const vector<Color>& tube, int turns,
                                      int d) {
    const int h = (int)target.size();
    const int extra_budget = max(0, turns / 2 - h);
    int max_amount = min(MAX_RECIPE_SIZE, 1 + extra_budget);
    if (d > 0) {
        max_amount = min(max_amount, 1 + (int)(17321 / d));
    }

    IndependentPlan plan;
    plan.amount.assign(h, 1);
    plan.option.resize(h);
    for (int i = 0; i < h; ++i) {
        plan.option[i] =
            make_single_target_options(target[i], max_amount, tube);
    }

    auto option_cost = [&](int i, int amount) {
        return 10000.0 * plan.option[i][amount].error +
               (double)d * (amount - 1);
    };

    struct Jump {
        double gain_per_gram;
        int target_index;
        int from;
        int to;
        bool operator<(const Jump& other) const {
            return gain_per_gram < other.gain_per_gram;
        }
    };
    auto best_jump = [&](int i, int from, int available) {
        Jump best{0.0, i, from, from};
        int last = min(max_amount, from + available);
        for (int to = from + 1; to <= last; ++to) {
            double gain = option_cost(i, from) - option_cost(i, to);
            double ratio = gain / (to - from);
            if (ratio > best.gain_per_gram) {
                best = {ratio, i, from, to};
            }
        }
        return best;
    };

    priority_queue<Jump> queue;
    for (int i = 0; i < h; ++i) {
        Jump jump = best_jump(i, 1, max_amount - 1);
        if (jump.gain_per_gram > 0.0) queue.push(jump);
    }
    int remaining = extra_budget;
    while (remaining > 0 && !queue.empty()) {
        Jump jump = queue.top();
        queue.pop();
        int i = jump.target_index;
        if (plan.amount[i] != jump.from) continue;
        if (jump.to - jump.from > remaining) {
            jump = best_jump(i, jump.from, remaining);
            if (jump.gain_per_gram > 0.0) queue.push(jump);
            continue;
        }
        plan.amount[i] = jump.to;
        remaining -= jump.to - jump.from;
        Jump next = best_jump(i, jump.to, max_amount - jump.to);
        if (next.gain_per_gram > 0.0) queue.push(next);
    }

    long long extra_paint = 0;
    for (int i = 0; i < h; ++i) {
        plan.error += plan.option[i][plan.amount[i]].error;
        extra_paint += plan.amount[i] - 1;
    }
    plan.score = predicted_score(plan.error, extra_paint, d);
    return plan;
}

Color geometric_median(const vector<Color>& target, int begin, int end) {
    Color point;
    for (int i = begin; i < end; ++i) point = point + target[i];
    point = point / (end - begin);
    for (int iteration = 0; iteration < 10; ++iteration) {
        Color weighted_sum;
        double weight_sum = 0.0;
        bool exactly_on_target = false;
        for (int i = begin; i < end; ++i) {
            double distance = color_distance(point, target[i]);
            if (distance < 1e-12) {
                point = target[i];
                exactly_on_target = true;
                break;
            }
            double weight = 1.0 / distance;
            weighted_sum = weighted_sum + target[i] * weight;
            weight_sum += weight;
        }
        if (exactly_on_target) break;
        point = weighted_sum / weight_sum;
    }
    return point;
}

struct Block {
    Mix mix;
    double error = numeric_limits<double>::infinity();
};

struct GroupPlan {
    vector<int> begin;
    vector<Block> block;
    double error = 0.0;
    long long score = numeric_limits<long long>::max();
};

GroupPlan make_group_plan(const vector<Color>& target,
                          const vector<Color>& tube, int d) {
    const int h = (int)target.size();
    vector<vector<Block>> candidate(
        h, vector<Block>(MAX_GROUP_SIZE + 1));
    for (int begin = 0; begin < h; ++begin) {
        int last_length = min(MAX_GROUP_SIZE, h - begin);
        for (int length = 1; length <= last_length; ++length) {
            Color center = geometric_median(target, begin, begin + length);
            Mix mix = make_greedy_mix(center, length, tube);
            double error = 0.0;
            for (int i = begin; i < begin + length; ++i) {
                error += color_distance(mix.color, target[i]);
            }
            candidate[begin][length] = {mix, error};
        }
    }

    vector<double> dp(h + 1, numeric_limits<double>::infinity());
    vector<int> previous(h + 1, -1);
    dp[0] = 0.0;
    for (int begin = 0; begin < h; ++begin) {
        int last_length = min(MAX_GROUP_SIZE, h - begin);
        for (int length = 1; length <= last_length; ++length) {
            int end = begin + length;
            double value = dp[begin] + candidate[begin][length].error;
            if (value < dp[end]) {
                dp[end] = value;
                previous[end] = begin;
            }
        }
    }

    GroupPlan plan;
    for (int end = h; end > 0; end = previous[end]) {
        int begin = previous[end];
        plan.begin.push_back(begin);
        plan.block.push_back(candidate[begin][end - begin]);
    }
    reverse(plan.begin.begin(), plan.begin.end());
    reverse(plan.block.begin(), plan.block.end());
    plan.error = dp[h];
    plan.score = predicted_score(plan.error, 0, d);
    return plan;
}

vector<Color> make_kmeans_centers(const vector<Color>& target, int groups) {
    const int h = (int)target.size();
    vector<Color> center;
    center.reserve(groups);
    center.push_back(target[0]);
    vector<double> nearest(h, numeric_limits<double>::infinity());
    while ((int)center.size() < groups) {
        int farthest = 0;
        for (int i = 0; i < h; ++i) {
            nearest[i] = min(nearest[i],
                             squared_distance(target[i], center.back()));
            if (nearest[i] > nearest[farthest]) farthest = i;
        }
        center.push_back(target[farthest]);
    }

    vector<int> assignment(h, 0);
    for (int iteration = 0; iteration < 10; ++iteration) {
        vector<Color> sum(groups);
        vector<int> count(groups, 0);
        for (int i = 0; i < h; ++i) {
            int best = 0;
            double best_distance = squared_distance(target[i], center[0]);
            for (int g = 1; g < groups; ++g) {
                double distance = squared_distance(target[i], center[g]);
                if (distance < best_distance) {
                    best_distance = distance;
                    best = g;
                }
            }
            assignment[i] = best;
            sum[best] = sum[best] + target[i];
            ++count[best];
        }
        for (int g = 0; g < groups; ++g) {
            if (count[g] > 0) center[g] = sum[g] / count[g];
        }
    }
    return center;
}

struct ReservoirPlan {
    int well_size = 0;
    vector<Counts> initial_recipe;
    vector<int> selected_well;
    // -1 means "take existing paint without adding a new gram".
    vector<int> selected_tube;
    double error = 0.0;
    long long score = numeric_limits<long long>::max();
};

ReservoirPlan make_reservoir_plan(const vector<Color>& target,
                                  const vector<Color>& tube, int n,
                                  int turns, int d) {
    const vector<int> well_sizes = {2, 4, 5, 10, 20};
    const vector<int> group_candidates = {1, 2, 4, 8, 16, 32, 64, 100, 200};
    map<int, vector<Color>> center_cache;
    ReservoirPlan best;

    for (int well_size : well_sizes) {
        for (int groups : group_candidates) {
            if (groups * well_size > n * n) continue;
            if (2 * (int)target.size() + groups * (well_size - 1) > turns) {
                continue;
            }
            if (!center_cache.count(groups)) {
                center_cache[groups] = make_kmeans_centers(target, groups);
            }
            const vector<Color>& center = center_cache[groups];
            vector<Counts> initial(groups);
            vector<Color> initial_state(groups);
            for (int g = 0; g < groups; ++g) {
                Mix mix = make_greedy_mix(center[g], well_size - 1, tube);
                initial[g] = mix.count;
                initial_state[g] = mix.color;
            }

            // A direct extraction consumes one gram already in a well.  It
            // therefore saves D points compared with leaving that gram at the
            // end.  Several weights are tried because saving it too early can
            // destroy a useful slowly-changing color state.
            const array<double, 4> drain_weights = {-1.0, 0.25, 0.55, 1.0};
            for (double drain_weight : drain_weights) {
                vector<Color> state(groups);
                vector<int> volume(groups, well_size - 1);
                for (int g = 0; g < groups; ++g) {
                    state[g] = initial_state[g];
                }
                vector<int> selected_well(target.size());
                vector<int> selected_tube(target.size());
                vector<char> used(groups, false);
                double total_error = 0.0;
                for (int i = 0; i < (int)target.size(); ++i) {
                    int best_group = 0;
                    int best_tube = 0;
                    Color best_color;
                    double best_error = numeric_limits<double>::infinity();
                    double best_adjusted = numeric_limits<double>::infinity();
                    for (int g = 0; g < groups; ++g) {
                        if (drain_weight >= 0.0 && volume[g] > 0) {
                            double error = color_distance(state[g], target[i]);
                            double adjusted =
                                10000.0 * error - drain_weight * d;
                            if (adjusted < best_adjusted) {
                                best_adjusted = adjusted;
                                best_error = error;
                                best_group = g;
                                best_tube = -1;
                                best_color = state[g];
                            }
                        }
                        for (int k = 0; k < (int)tube.size(); ++k) {
                            Color made =
                                (state[g] * volume[g] + tube[k]) /
                                (volume[g] + 1);
                            double error = color_distance(made, target[i]);
                            double adjusted = 10000.0 * error;
                            if (adjusted < best_adjusted) {
                                best_adjusted = adjusted;
                                best_error = error;
                                best_group = g;
                                best_tube = k;
                                best_color = made;
                            }
                        }
                    }
                    selected_well[i] = best_group;
                    selected_tube[i] = best_tube;
                    used[best_group] = true;
                    if (best_tube == -1) {
                        --volume[best_group];
                    } else {
                        state[best_group] = best_color;
                    }
                    total_error += best_error;
                }

                vector<int> new_index(groups, -1);
                vector<Counts> compact_initial;
                long long extra_paint = 0;
                for (int g = 0; g < groups; ++g) {
                    if (used[g]) {
                        new_index[g] = (int)compact_initial.size();
                        compact_initial.push_back(initial[g]);
                        extra_paint += volume[g];
                    }
                }
                for (int& g : selected_well) g = new_index[g];
                long long score =
                    predicted_score(total_error, extra_paint, d);
                if (score < best.score) {
                    best.well_size = well_size;
                    best.initial_recipe = move(compact_initial);
                    best.selected_well = move(selected_well);
                    best.selected_tube = move(selected_tube);
                    best.error = total_error;
                    best.score = score;
                }
            }
        }
    }
    return best;
}

void print_uniform_dividers(int n, int value) {
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j + 1 < n; ++j) {
            if (j) cout << ' ';
            cout << value;
        }
        cout << '\n';
    }
    for (int i = 0; i + 1 < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (j) cout << ' ';
            cout << value;
        }
        cout << '\n';
    }
}

void print_reservoir_dividers(int n, int well_size, int wells) {
    int wells_per_row = n / well_size;
    for (int row = 0; row < n; ++row) {
        for (int column = 0; column + 1 < n; ++column) {
            if (column) cout << ' ';
            int left_well = row * wells_per_row + column / well_size;
            bool inside_used_well = left_well < wells;
            bool same_segment = column % well_size != well_size - 1;
            cout << (inside_used_well && same_segment ? 0 : 1);
        }
        cout << '\n';
    }
    for (int row = 0; row + 1 < n; ++row) {
        for (int column = 0; column < n; ++column) {
            if (column) cout << ' ';
            cout << 1;
        }
        cout << '\n';
    }
}

pair<int, int> reservoir_cell(int well, int n, int well_size) {
    int wells_per_row = n / well_size;
    return {well / wells_per_row, (well % wells_per_row) * well_size};
}

void pour_recipe(const Counts& count, int row, int column,
                 const vector<Color>& tube, long long& turns_used) {
    for (int k = 0; k < (int)tube.size(); ++k) {
        for (int repeat = 0; repeat < count[k]; ++repeat) {
            cout << "1 " << row << ' ' << column << ' ' << k << '\n';
            ++turns_used;
        }
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, k, h, turns, d;
    cin >> n >> k >> h >> turns >> d;
    vector<Color> tube(k), target(h);
    for (Color& color : tube) cin >> color.c >> color.m >> color.y;
    for (Color& color : target) cin >> color.c >> color.m >> color.y;

#ifdef AHC048_BASELINE
    print_uniform_dividers(n, 1);
    for (const Color& wanted : target) {
        int best = 0;
        for (int candidate = 1; candidate < k; ++candidate) {
            if (squared_distance(tube[candidate], wanted) <
                squared_distance(tube[best], wanted)) {
                best = candidate;
            }
        }
        cout << "1 0 0 " << best << '\n';
        cout << "2 0 0\n";
    }
    return 0;
#endif

    IndependentPlan independent =
        make_independent_plan(target, tube, turns, d);
    GroupPlan group = make_group_plan(target, tube, d);
    ReservoirPlan reservoir =
        make_reservoir_plan(target, tube, n, turns, d);

    enum class PlanType { INDEPENDENT, GROUP, RESERVOIR };
    PlanType chosen = PlanType::INDEPENDENT;
    long long best_score = independent.score;
    if (group.score < best_score) {
        chosen = PlanType::GROUP;
        best_score = group.score;
    }
    if (reservoir.score < best_score) {
        chosen = PlanType::RESERVOIR;
        best_score = reservoir.score;
    }

#ifdef LOCAL
    cerr << fixed << setprecision(3);
    cerr << "predicted independent=" << independent.score
         << " group=" << group.score << " reservoir=" << reservoir.score
         << " chosen=" << best_score << " reservoir_size="
         << reservoir.well_size << " reservoir_wells="
         << reservoir.initial_recipe.size() << '\n';
#endif

    long long turns_used = 0;
    if (chosen == PlanType::RESERVOIR) {
        int wells = (int)reservoir.initial_recipe.size();
        print_reservoir_dividers(n, reservoir.well_size, wells);
        for (int well = 0; well < wells; ++well) {
            auto [row, column] = reservoir_cell(well, n,
                                                reservoir.well_size);
            pour_recipe(reservoir.initial_recipe[well], row, column, tube,
                        turns_used);
        }
        for (int i = 0; i < h; ++i) {
            int well = reservoir.selected_well[i];
            auto [row, column] = reservoir_cell(well, n,
                                                reservoir.well_size);
            if (reservoir.selected_tube[i] != -1) {
                cout << "1 " << row << ' ' << column << ' '
                     << reservoir.selected_tube[i] << '\n';
                ++turns_used;
            }
            cout << "2 " << row << ' ' << column << '\n';
            ++turns_used;
        }
    } else {
        // One large well is enough because it is emptied between recipes.
        print_uniform_dividers(n, 0);
        if (chosen == PlanType::INDEPENDENT) {
            for (int i = 0; i < h; ++i) {
                int amount = independent.amount[i];
                const Mix& mix = independent.option[i][amount];
                pour_recipe(mix.count, 0, 0, tube, turns_used);
                cout << "2 0 0\n";
                ++turns_used;
                for (int repeat = 1; repeat < amount; ++repeat) {
                    cout << "3 0 0\n";
                    ++turns_used;
                }
            }
        } else {
            for (int block_index = 0;
                 block_index < (int)group.block.size(); ++block_index) {
                int begin = group.begin[block_index];
                int end = block_index + 1 < (int)group.begin.size()
                              ? group.begin[block_index + 1]
                              : h;
                int amount = end - begin;
                pour_recipe(group.block[block_index].mix.count, 0, 0, tube,
                            turns_used);
                for (int repeat = 0; repeat < amount; ++repeat) {
                    cout << "2 0 0\n";
                    ++turns_used;
                }
            }
        }
    }
    assert(turns_used <= turns);
    return 0;
}
