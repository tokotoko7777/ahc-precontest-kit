#include <bits/stdc++.h>
using namespace std;
using i128 = __int128_t;

#ifndef ROLLOUT_DEPTH
#define ROLLOUT_DEPTH 3
#endif

#ifndef ROLLOUT_CANDIDATES
#define ROLLOUT_CANDIDATES 12
#endif

// Future-value estimates are only compared with each other.  Clamping them
// avoids overflow even when a deliberately extreme upgrade is examined.
const i128 VALUE_LIMIT = (i128{1} << 120);

i128 saturated_add(i128 a, i128 b) {
    if (a >= VALUE_LIMIT - b) return VALUE_LIMIT;
    return a + b;
}

i128 saturated_multiply(i128 a, i128 b) {
    if (a == 0 || b == 0) return 0;
    if (a >= VALUE_LIMIT / b) return VALUE_LIMIT;
    return a * b;
}

long long combination(int n, int k) {
    if (k < 0 || k > n) return 0;
    k = min(k, n - k);
    long long answer = 1;
    for (int i = 1; i <= k; ++i) {
        answer = answer * (n - k + i) / i;
    }
    return answer;
}

i128 future_production(
    int id,
    int turns,
    int levels,
    const vector<long long>& production,
    const vector<vector<i128>>& machine_count,
    const vector<vector<int>>& power
) {
    i128 sum_level0_machines = saturated_multiply(machine_count[0][id], turns);
    i128 power_product = 1;
    for (int level = 1; level < levels; ++level) {
        power_product = saturated_multiply(power_product, power[level][id]);
        i128 term = saturated_multiply(machine_count[level][id], power_product);
        term = saturated_multiply(term, combination(turns, level + 1));
        sum_level0_machines = saturated_add(sum_level0_machines, term);
    }
    i128 result = saturated_multiply(sum_level0_machines, power[0][id]);
    return saturated_multiply(result, production[id]);
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int N, L, T;
    long long K;
    cin >> N >> L >> T >> K;

    vector<long long> A(N);
    for (long long& x : A) cin >> x;

    vector<vector<long long>> C(L, vector<long long>(N));
    for (auto& row : C) {
        for (long long& x : row) cin >> x;
    }

    vector<vector<i128>> machine_count(L, vector<i128>(N, 1));
    vector<vector<int>> power(L, vector<int>(N, 0));
    i128 apples = K;

    auto advance_one_turn = [&](vector<vector<i128>>& count,
                                vector<vector<int>>& current_power,
                                i128& current_apples,
                                int action_level,
                                int action_id) {
        if (action_level != -1) {
            const long long cost = C[action_level][action_id]
                                 * (current_power[action_level][action_id] + 1LL);
            if (current_apples < cost) return false;
            current_apples -= cost;
            ++current_power[action_level][action_id];
        }

        for (int id = 0; id < N; ++id) {
            current_apples += count[0][id] * current_power[0][id] * A[id];
        }
        for (int level = 1; level < L; ++level) {
            for (int id = 0; id < N; ++id) {
                count[level - 1][id] += count[level][id] * current_power[level][id];
            }
        }
        return true;
    };

    auto value_with_at_most_one_upgrade = [&](const vector<vector<i128>>& count,
                                               vector<vector<int>>& current_power,
                                               i128 current_apples,
                                               int turns) {
        i128 value = current_apples;
        for (int id = 0; id < N; ++id) {
            value = saturated_add(
                value,
                future_production(id, turns, L, A, count, current_power)
            );
        }

        i128 best_gain = 0;
        for (int level = 0; level < L; ++level) {
            for (int id = 0; id < N; ++id) {
                const long long cost =
                    C[level][id] * (current_power[level][id] + 1LL);
                if (current_apples < cost) continue;

                const i128 before =
                    future_production(id, turns, L, A, count, current_power);
                ++current_power[level][id];
                const i128 after =
                    future_production(id, turns, L, A, count, current_power);
                --current_power[level][id];
                best_gain = max(best_gain, after - before - cost);
            }
        }
        return saturated_add(value, best_gain);
    };

    for (int turn = 0; turn < T; ++turn) {
        const int remaining_turns = T - turn;
        int best_level = -1;
        int best_id = -1;
        i128 best_value = -1;

        // Try every action now, then look two turns further ahead.
        // (-1, -1) means "do nothing".
        for (int first = -1; first < L * N; ++first) {
            const int first_level = (first == -1 ? -1 : first / N);
            const int first_id = (first == -1 ? -1 : first % N);

            auto next_count = machine_count;
            auto next_power = power;
            i128 next_apples = apples;

            if (!advance_one_turn(
                    next_count, next_power, next_apples, first_level, first_id)) continue;

            const int later_turns = remaining_turns - 1;
            i128 value;

#if ROLLOUT_DEPTH >= 3
            if (later_turns == 0) {
                value = next_apples;
            } else {
                struct RankedAction {
                    i128 gain;
                    int code;
                };
                vector<RankedAction> ranked;
                vector<int> best_of_level(L, -1);
                vector<i128> best_level_gain(L, -VALUE_LIMIT);

                for (int code = 0; code < L * N; ++code) {
                    const int level = code / N;
                    const int id = code % N;
                    const long long cost =
                        C[level][id] * (next_power[level][id] + 1LL);
                    if (next_apples < cost) continue;

                    const i128 before = future_production(
                        id, later_turns, L, A, next_count, next_power
                    );
                    ++next_power[level][id];
                    const i128 after = future_production(
                        id, later_turns, L, A, next_count, next_power
                    );
                    --next_power[level][id];
                    const i128 gain = after - before - cost;
                    ranked.push_back({gain, code});
                    if (gain > best_level_gain[level]) {
                        best_level_gain[level] = gain;
                        best_of_level[level] = code;
                    }
                }
                sort(ranked.begin(), ranked.end(), [](const auto& x, const auto& y) {
                    return x.gain > y.gain;
                });

                vector<int> second_actions = {-1};
                const int global_candidates =
                    min(ROLLOUT_CANDIDATES, static_cast<int>(ranked.size()));
                for (int k = 0; k < global_candidates; ++k) {
                    second_actions.push_back(ranked[k].code);
                }
                for (int level = 0; level < L; ++level) {
                    const int code = best_of_level[level];
                    if (code != -1
                        && find(second_actions.begin(), second_actions.end(), code)
                               == second_actions.end()) {
                        second_actions.push_back(code);
                    }
                }

                value = -1;
                for (int second : second_actions) {
                    const int second_level = (second == -1 ? -1 : second / N);
                    const int second_id = (second == -1 ? -1 : second % N);
                    auto after_count = next_count;
                    auto after_power = next_power;
                    i128 after_apples = next_apples;
                    if (!advance_one_turn(after_count, after_power, after_apples,
                                          second_level, second_id)) {
                        continue;
                    }
                    value = max(
                        value,
                        value_with_at_most_one_upgrade(
                            after_count, after_power, after_apples, later_turns - 1
                        )
                    );
                }
            }
#else
            value = value_with_at_most_one_upgrade(
                next_count, next_power, next_apples, later_turns
            );
#endif

            if (value > best_value) {
                best_value = value;
                best_level = first_level;
                best_id = first_id;
            }
        }

        if (best_level == -1) {
            cout << -1 << '\n';
        } else {
            cout << best_level << ' ' << best_id << '\n';
        }
        advance_one_turn(machine_count, power, apples, best_level, best_id);
    }
}
