#include <bits/stdc++.h>
using namespace std;

#ifndef AHC025_ITEM_BUDGET_NUMERATOR
#define AHC025_ITEM_BUDGET_NUMERATOR 3
#endif

#ifndef AHC025_ITEM_BUDGET_DENOMINATOR
#define AHC025_ITEM_BUDGET_DENOMINATOR 4
#endif

struct Random {
    uint64_t state = 0x123456789abcdef0ULL;

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
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int N, D, Q;
    cin >> N >> D >> Q;
    int query_count = 0;

    auto ask = [&](const vector<int>& left, const vector<int>& right) {
        cout << left.size() << ' ' << right.size();
        for (int item : left) cout << ' ' << item;
        for (int item : right) cout << ' ' << item;
        cout << endl;
        ++query_count;
        char result;
        cin >> result;
        return result;
    };

    Random random;
    vector<int> item_order(N);
    iota(item_order.begin(), item_order.end(), 0);
    bool exact_item_order = false;

#ifndef AHC025_NO_ITEM_LEARNING
    const int level_count = [&]() {
        int level = 0;
        while ((1 << level) < N) ++level;
        return level;
    }();
    const int merge_sort_worst = N * level_count - (1 << level_count) + N;
    const int item_query_budget =
        Q * AHC025_ITEM_BUDGET_NUMERATOR / AHC025_ITEM_BUDGET_DENOMINATOR;

    if (merge_sort_worst <= item_query_budget) {
        // With enough queries, determine the exact nondecreasing item order.
        vector<int> merge_buffer(N);
        function<void(int, int)> merge_sort = [&](int begin, int end) {
                if (end - begin <= 1) return;
                const int middle = (begin + end) / 2;
                merge_sort(begin, middle);
                merge_sort(middle, end);

                int left_index = begin;
                int right_index = middle;
                int write_index = begin;
                while (left_index < middle && right_index < end) {
                    const char comparison = ask(
                        vector<int>{item_order[left_index]},
                        vector<int>{item_order[right_index]});
                    if (comparison == '<'
                        || (comparison == '='
                            && item_order[left_index] < item_order[right_index])) {
                        merge_buffer[write_index++] = item_order[left_index++];
                    } else {
                        merge_buffer[write_index++] = item_order[right_index++];
                    }
                }
                while (left_index < middle) {
                    merge_buffer[write_index++] = item_order[left_index++];
                }
                while (right_index < end) {
                    merge_buffer[write_index++] = item_order[right_index++];
                }
                for (int index = begin; index < end; ++index) {
                    item_order[index] = merge_buffer[index];
                }
            };
        merge_sort(0, N);
        exact_item_order = true;
    } else {
        // When Q is small, compare similarly rated items.  Every answer is
        // noiseless, but a few comparisons per item only determine a partial
        // order, so an Elo-like value is used as a simple rank estimate.
        vector<double> rating(N, 0.0);
        vector<int> comparison_count(N, 0);
        vector<vector<char>> already_compared(N, vector<char>(N, false));

        while (query_count < item_query_budget) {
            int best_left = -1;
            int best_right = -1;
            double best_priority = 1e100;
            for (int trial = 0; trial < 300; ++trial) {
                int left = random.next_int(N);
                int right = random.next_int(N - 1);
                if (right >= left) ++right;
                if (already_compared[left][right]) continue;
                const double priority = abs(rating[left] - rating[right])
                                      + 0.08 * (comparison_count[left]
                                              + comparison_count[right]);
                if (priority < best_priority) {
                    best_priority = priority;
                    best_left = left;
                    best_right = right;
                }
            }
            if (best_left == -1) {
                for (int left = 0; left < N && best_left == -1; ++left) {
                    for (int right = left + 1; right < N; ++right) {
                        if (!already_compared[left][right]) {
                            best_left = left;
                            best_right = right;
                            break;
                        }
                    }
                }
            }
            if (best_left == -1) break;

            already_compared[best_left][best_right] = true;
            already_compared[best_right][best_left] = true;
            const char comparison = ask(
                vector<int>{best_left}, vector<int>{best_right});
            const double expected_left =
                1.0 / (1.0 + exp(-(rating[best_left] - rating[best_right])));
            double actual_left = 0.5;
            if (comparison == '>') actual_left = 1.0;
            if (comparison == '<') actual_left = 0.0;
            const double change = actual_left - expected_left;
            rating[best_left] += change;
            rating[best_right] -= change;
            ++comparison_count[best_left];
            ++comparison_count[best_right];
        }

        sort(item_order.begin(), item_order.end(), [&](int left, int right) {
            if (rating[left] != rating[right]) return rating[left] < rating[right];
            return left < right;
        });
    }
#endif

    const int ranking_query_count = query_count;

    // The generator uses a truncated exponential distribution.  Convert the
    // observed rank to its prior quantile; only relative sizes are needed.
    vector<long double> estimated_weight(N, 1.0L);
    const long double upper = 100000.0L * N / D;
    const long double kept_probability = 1.0L - exp(-upper / 100000.0L);
    for (int rank = 0; rank < N; ++rank) {
        const long double percentile = (rank + 0.5L) / N;
        const long double original_probability = percentile * kept_probability;
        estimated_weight[item_order[rank]] =
            -100000.0L * log(1.0L - original_probability);
    }

    // LPT construction using the estimated sizes.
    vector<vector<int>> groups(D);
    vector<long double> estimated_sum(D, 0.0L);
    for (int rank = N - 1; rank >= 0; --rank) {
        const int item = item_order[rank];
        int lightest_group = 0;
        for (int group = 1; group < D; ++group) {
            if (estimated_sum[group] < estimated_sum[lightest_group]) {
                lightest_group = group;
            }
        }
        groups[lightest_group].push_back(item);
        estimated_sum[lightest_group] += estimated_weight[item];
    }

    int accepted_moves = 0;
#ifndef AHC025_NO_BALANCE
    // A move from heavy A to light B improves their squared error exactly when
    // item_weight < sum(A)-sum(B).  This can be checked by asking A\item ? B.
    set<tuple<int, int, int>> failed_move;
    set<tuple<int, int, int, int>> failed_swap;
    int attempt = 0;
    while (query_count + 2 <= Q) {
        vector<int> group_order(D);
        iota(group_order.begin(), group_order.end(), 0);
        sort(group_order.begin(), group_order.end(), [&](int left, int right) {
            return estimated_sum[left] < estimated_sum[right];
        });

        const int choice_count = min(3, D);
        const int operation_number = attempt++;
        int light = group_order[(operation_number / choice_count) % choice_count];
        int heavy = group_order[D - 1 - (operation_number % choice_count)];
        if (light == heavy) continue;

        const char group_comparison = ask(groups[heavy], groups[light]);
        if (group_comparison == '<') swap(heavy, light);
        if (group_comparison == '=') continue;
        if (groups[heavy].size() <= 1) continue;

        const long double predicted_difference =
            abs(estimated_sum[heavy] - estimated_sum[light]);

        // With an exact item order, one out of three trials swaps a heavy item
        // and a light item.  A\x > B\y proves x-y < sum(A)-sum(B), so swapping
        // x,y strictly reduces the two group totals' difference when x>y.
        const bool try_swap = exact_item_order
                           && operation_number % 3 == 2
                           && groups[light].size() > 1;
        if (try_swap) {
            vector<pair<int, int>> candidates;
            for (int heavy_item : groups[heavy]) {
                for (int light_item : groups[light]) {
                    if (estimated_weight[heavy_item] <= estimated_weight[light_item]) {
                        continue;
                    }
                    if (!failed_swap.count({heavy, light, heavy_item, light_item})) {
                        candidates.push_back({heavy_item, light_item});
                    }
                }
            }
            sort(candidates.begin(), candidates.end(), [&](const auto& left,
                                                            const auto& right) {
                const long double left_change =
                    estimated_weight[left.first] - estimated_weight[left.second];
                const long double right_change =
                    estimated_weight[right.first] - estimated_weight[right.second];
                return abs(left_change - predicted_difference / 2)
                     < abs(right_change - predicted_difference / 2);
            });
            if (candidates.empty()) continue;

            const auto [heavy_item, light_item] = candidates.front();
            vector<int> reduced_heavy;
            vector<int> reduced_light;
            for (int item : groups[heavy]) {
                if (item != heavy_item) reduced_heavy.push_back(item);
            }
            for (int item : groups[light]) {
                if (item != light_item) reduced_light.push_back(item);
            }
            const char swap_test = ask(reduced_heavy, reduced_light);
            if (swap_test == '>') {
                for (int& item : groups[heavy]) {
                    if (item == heavy_item) {
                        item = light_item;
                        break;
                    }
                }
                for (int& item : groups[light]) {
                    if (item == light_item) {
                        item = heavy_item;
                        break;
                    }
                }
                const long double change =
                    estimated_weight[heavy_item] - estimated_weight[light_item];
                estimated_sum[heavy] -= change;
                estimated_sum[light] += change;
                failed_move.clear();
                failed_swap.clear();
                ++accepted_moves;
            } else {
                failed_swap.insert({heavy, light, heavy_item, light_item});
            }
            continue;
        }

        vector<int> candidates = groups[heavy];
        sort(candidates.begin(), candidates.end(), [&](int left, int right) {
            return abs(estimated_weight[left] - predicted_difference / 2)
                 < abs(estimated_weight[right] - predicted_difference / 2);
        });

        int selected = -1;
        for (int candidate : candidates) {
            if (!failed_move.count({heavy, light, candidate})) {
                selected = candidate;
                break;
            }
        }
        if (selected == -1) continue;

        vector<int> reduced_heavy;
        reduced_heavy.reserve(groups[heavy].size() - 1);
        for (int value : groups[heavy]) {
            if (value != selected) reduced_heavy.push_back(value);
        }
        const char move_test = ask(reduced_heavy, groups[light]);
        if (move_test == '>') {
            groups[heavy] = move(reduced_heavy);
            groups[light].push_back(selected);
            estimated_sum[heavy] -= estimated_weight[selected];
            estimated_sum[light] += estimated_weight[selected];
            failed_move.clear();
            failed_swap.clear();
            ++accepted_moves;
        } else {
            failed_move.insert({heavy, light, selected});
        }
    }
#endif

    // The protocol requires exactly Q queries.  Extra answers are intentionally
    // ignored after the division has been decided.
    while (query_count < Q) {
        ask(vector<int>{0}, vector<int>{1});
    }

#ifdef AHC025_DEBUG
    cerr << "ranking_queries=" << ranking_query_count
         << " accepted_moves=" << accepted_moves
         << " total_queries=" << query_count << '\n';
#else
    static_cast<void>(ranking_query_count);
    static_cast<void>(exact_item_order);
#ifndef AHC025_NO_BALANCE
    static_cast<void>(accepted_moves);
#endif
#endif

    vector<int> answer(N, 0);
    for (int group = 0; group < D; ++group) {
        for (int item : groups[group]) answer[item] = group;
    }
    for (int item = 0; item < N; ++item) {
        if (item) cout << ' ';
        cout << answer[item];
    }
    cout << endl;
}
