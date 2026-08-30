#include <bits/stdc++.h>
using namespace std;

// AHC055 "Weakpoint"
//
// A solution is kept as:
//   1. the order in which boxes are opened, and
//   2. the target of every use of every weapon.
// A weapon may attack only a box that appears later in the order.

#ifndef AHC055_TIME_LIMIT
#define AHC055_TIME_LIMIT 1.80
#endif

struct Timer {
    chrono::steady_clock::time_point start = chrono::steady_clock::now();

    double seconds() const {
        return chrono::duration<double>(chrono::steady_clock::now() - start).count();
    }
};

struct Random {
    uint64_t x;

    explicit Random(uint64_t seed) : x(seed ? seed : 1) {}

    uint64_t next_u64() {
        x ^= x << 7;
        x ^= x >> 9;
        return x;
    }

    int next_int(int upper) {
        return int(next_u64() % uint64_t(upper));
    }

    double next_double() {
        return (next_u64() >> 11) * (1.0 / 9007199254740992.0);
    }
};

struct Solver {
    static constexpr int MAX_N = 200;
    static constexpr int MAX_C = 6;

    int n;
    int h[MAX_N];
    int c[MAX_N];
    int power[MAX_N][MAX_N];

    int order[MAX_N];
    int position[MAX_N];
    int target[MAX_N][MAX_C];
    int damage[MAX_N];
    unsigned char edge_count[MAX_N][MAX_N];

    int best_order[MAX_N];
    int best_target[MAX_N][MAX_C];
    int current_cost = 0;
    int best_cost = 0;

    vector<vector<int>> strong_targets;
    Timer timer;
    Random random;

    Solver(int n_, uint64_t seed) : n(n_), strong_targets(n_), random(seed) {
        memset(target, -1, sizeof(target));
        memset(best_target, -1, sizeof(best_target));
        memset(edge_count, 0, sizeof(edge_count));
    }

    // First make a legal solution. Use the currently most effective weapon
    // attack. If no useful weapon remains, open a promising weapon by hand.
    void make_greedy_solution(bool use_lookahead) {
        int hp[MAX_N];
        int remaining[MAX_N];
        int next_slot[MAX_N] = {};
        bool opened[MAX_N] = {};

        memset(target, -1, sizeof(target));

        for (int i = 0; i < n; ++i) {
            hp[i] = h[i];
            remaining[i] = c[i];
        }

        int opened_count = 0;
        while (opened_count < n) {
            int best_weapon = -1;
            int best_box = -1;
            int best_effective_damage = 1;

            for (int w = 0; w < n; ++w) {
                if (!opened[w] || remaining[w] == 0) continue;
                for (int b = 0; b < n; ++b) {
                    if (opened[b]) continue;
                    int effective_damage = min(power[w][b], hp[b]);
                    if (effective_damage > best_effective_damage) {
                        best_effective_damage = effective_damage;
                        best_weapon = w;
                        best_box = b;
                    } else if (effective_damage == best_effective_damage && best_box != -1) {
                        // On a tie, prefer an attack that opens a new weapon now.
                        bool opens_now = power[w][b] >= hp[b];
                        bool old_opens_now = power[best_weapon][best_box] >= hp[best_box];
                        if (opens_now && !old_opens_now) {
                            best_weapon = w;
                            best_box = b;
                        }
                    }
                }
            }

            if (best_weapon != -1) {
                int k = next_slot[best_weapon]++;
                target[best_weapon][k] = best_box;
                --remaining[best_weapon];
                hp[best_box] -= power[best_weapon][best_box];

                if (hp[best_box] <= 0) {
                    opened[best_box] = true;
                    order[opened_count++] = best_box;
                }
            } else {
                if (!use_lookahead) {
                    int best_hp = INT_MAX;
                    for (int b = 0; b < n; ++b) {
                        if (!opened[b] && hp[b] < best_hp) {
                            best_hp = hp[b];
                            best_box = b;
                        }
                    }
                } else {
                    int best_value = INT_MAX;
                    for (int b = 0; b < n; ++b) {
                        if (opened[b]) continue;

                        // Estimate how many future hand attacks weapon b can
                        // save. C[b] is at most 6, so its best six targets are
                        // enough for this estimate.
                        int top_savings[MAX_C] = {};
                        for (int t = 0; t < n; ++t) {
                            if (opened[t] || t == b) continue;
                            int saving = min(power[b][t], hp[t]) - 1;
                            if (saving <= top_savings[c[b] - 1]) continue;
                            top_savings[c[b] - 1] = saving;
                            for (int k = c[b] - 1; k > 0; --k) {
                                if (top_savings[k] <= top_savings[k - 1]) break;
                                swap(top_savings[k], top_savings[k - 1]);
                            }
                        }
                        int future_saving = accumulate(top_savings,
                                                       top_savings + c[b], 0);
                        int value = hp[b] - future_saving;
                        if (value < best_value ||
                            (value == best_value &&
                             (best_box == -1 || hp[b] < hp[best_box]))) {
                            best_value = value;
                            best_box = b;
                        }
                    }
                }
                hp[best_box] = 0;
                opened[best_box] = true;
                order[opened_count++] = best_box;
            }
        }

        rebuild_current_state();
    }

    void rebuild_current_state() {
        memset(damage, 0, sizeof(damage));
        memset(edge_count, 0, sizeof(edge_count));

        for (int p = 0; p < n; ++p) position[order[p]] = p;

        int used_attacks = 0;
        for (int w = 0; w < n; ++w) {
            for (int k = 0; k < c[w]; ++k) {
                int b = target[w][k];
                if (b == -1) continue;
                damage[b] += power[w][b];
                ++edge_count[w][b];
                ++used_attacks;
            }
        }

        current_cost = used_attacks;
        for (int b = 0; b < n; ++b) {
            current_cost += max(0, h[b] - damage[b]);
        }
    }

    void save_best() {
        best_cost = current_cost;
        memcpy(best_order, order, sizeof(order));
        memcpy(best_target, target, sizeof(target));
    }

    void restore_best() {
        memcpy(order, best_order, sizeof(order));
        memcpy(target, best_target, sizeof(target));
        rebuild_current_state();
    }

    // Exact cost difference when one weapon slot changes its target.
    int change_cost(int w, int old_box, int new_box) const {
        if (old_box == new_box) return 0;

        int delta = (new_box != -1) - (old_box != -1);

        if (old_box != -1) {
            int before = max(0, h[old_box] - damage[old_box]);
            int after = max(0, h[old_box] - (damage[old_box] - power[w][old_box]));
            delta += after - before;
        }
        if (new_box != -1) {
            int before = max(0, h[new_box] - damage[new_box]);
            int after = max(0, h[new_box] - (damage[new_box] + power[w][new_box]));
            delta += after - before;
        }
        return delta;
    }

    void apply_target_change(int w, int k, int new_box, int delta) {
        int old_box = target[w][k];
        if (old_box != -1) {
            damage[old_box] -= power[w][old_box];
            --edge_count[w][old_box];
        }
        if (new_box != -1) {
            damage[new_box] += power[w][new_box];
            ++edge_count[w][new_box];
        }
        target[w][k] = new_box;
        current_cost += delta;
    }

    bool can_swap_positions(int p, int q) const {
        if (p == q) return false;
        if (p > q) swap(p, q);

        int x = order[p];
        int y = order[q];

        // For adjacent boxes, only x -> y can become backward.
        if (q == p + 1) return edge_count[x][y] == 0;

        auto new_position = [&](int v) {
            if (v == x) return q;
            if (v == y) return p;
            return position[v];
        };

        // Only edges touching x or y can change direction.
        for (int z = 0; z < n; ++z) {
            if (edge_count[x][z] && q >= new_position(z)) return false;
            if (edge_count[y][z] && p >= new_position(z)) return false;
            if (edge_count[z][x] && new_position(z) >= q) return false;
            if (edge_count[z][y] && new_position(z) >= p) return false;
        }
        return true;
    }

    void swap_positions(int p, int q) {
        int x = order[p];
        int y = order[q];
        swap(order[p], order[q]);
        position[x] = q;
        position[y] = p;
    }

    void prepare_candidates() {
        for (int w = 0; w < n; ++w) {
            strong_targets[w].resize(n);
            iota(strong_targets[w].begin(), strong_targets[w].end(), 0);
            sort(strong_targets[w].begin(), strong_targets[w].end(), [&](int a, int b) {
                return power[w][a] > power[w][b];
            });
        }
    }

    int choose_new_target(int w, int old_box) {
        int candidates[32];
        int candidate_count = 0;
        candidates[candidate_count++] = -1;

        // High-damage candidates are important because large A[w][b] is rare.
        int strong_count = 0;
        for (int b : strong_targets[w]) {
            if (position[b] <= position[w] || b == old_box) continue;
            candidates[candidate_count++] = b;
            if (++strong_count == 12) break;
        }

        // Random candidates keep the search from looking only at raw damage;
        // remaining hardness also matters.
        int later_count = n - position[w] - 1;
        if (later_count > 0) {
            for (int t = 0; t < 12; ++t) {
                int b = order[position[w] + 1 + random.next_int(later_count)];
                if (b != old_box) candidates[candidate_count++] = b;
            }
        }

        if (candidate_count == 1) return -1;

        if (random.next_int(100) < 55) {
            int best_box = candidates[0];
            int best_delta = change_cost(w, old_box, best_box);
            for (int i = 1; i < candidate_count; ++i) {
                int b = candidates[i];
                int delta = change_cost(w, old_box, b);
                if (delta < best_delta) {
                    best_delta = delta;
                    best_box = b;
                }
            }
            return best_box;
        }

        return candidates[random.next_int(candidate_count)];
    }

    void improve() {
        prepare_candidates();

        constexpr double START_TEMPERATURE = 18.0;
        constexpr double END_TEMPERATURE = 0.08;
        const double time_limit = AHC055_TIME_LIMIT;

        double temperature = START_TEMPERATURE;
        uint64_t iteration = 0;

        while (true) {
            if ((iteration & 4095) == 0) {
                double progress = timer.seconds() / time_limit;
                if (progress >= 1.0) break;
                temperature = START_TEMPERATURE * pow(END_TEMPERATURE / START_TEMPERATURE,
                                                       progress);
            }
            ++iteration;

            // A zero-cost order move changes which future assignments are legal.
            if (n >= 2 && random.next_int(8) == 0) {
                int p, q;
                if (random.next_int(2) == 0) {
                    p = random.next_int(n - 1);
                    q = p + 1;
                } else {
                    p = random.next_int(n);
                    q = random.next_int(n);
                }
                if (can_swap_positions(p, q)) swap_positions(p, q);
                continue;
            }

            int w = random.next_int(n);
            int k = random.next_int(c[w]);
            int old_box = target[w][k];
            int new_box = choose_new_target(w, old_box);
            if (new_box == old_box) continue;
            if (new_box != -1 && position[new_box] <= position[w]) continue;

            int delta = change_cost(w, old_box, new_box);
            bool accept = delta <= 0;
            if (!accept) {
                accept = random.next_double() < exp(-double(delta) / temperature);
            }
            if (!accept) continue;

            apply_target_change(w, k, new_box, delta);
            if (current_cost < best_cost) save_best();
        }
    }

    // Remove attacks that are no better than replacing them by hand attacks.
    // This also guarantees that the final attack list never hits an opened box.
    void remove_redundant_attacks() {
        int total_damage[MAX_N] = {};
        for (int w = 0; w < n; ++w) {
            for (int k = 0; k < c[w]; ++k) {
                int b = best_target[w][k];
                if (b != -1) total_damage[b] += power[w][b];
            }
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (int w = 0; w < n; ++w) {
                for (int k = 0; k < c[w]; ++k) {
                    int b = best_target[w][k];
                    if (b == -1) continue;

                    int hand_before = max(0, h[b] - total_damage[b]);
                    int after_damage = total_damage[b] - power[w][b];
                    int hand_after = max(0, h[b] - after_damage);
                    if (hand_after <= hand_before + 1) {
                        best_target[w][k] = -1;
                        total_damage[b] = after_damage;
                        changed = true;
                    }
                }
            }
        }
    }

    void print_answer() {
        remove_redundant_attacks();

        int best_position[MAX_N];
        for (int p = 0; p < n; ++p) best_position[best_order[p]] = p;

        vector<vector<int>> attackers(n);
        for (int w = 0; w < n; ++w) {
            for (int k = 0; k < c[w]; ++k) {
                int b = best_target[w][k];
                if (b != -1 && best_position[w] < best_position[b]) {
                    attackers[b].push_back(w);
                }
            }
        }

        vector<pair<int, int>> answer;
        answer.reserve(accumulate(h, h + n, 0) + 1200);

        for (int p = 0; p < n; ++p) {
            int b = best_order[p];

            // Small attacks first: if the total weapon damage is enough, the
            // box is opened by the final weapon attack, never in the middle.
            sort(attackers[b].begin(), attackers[b].end(), [&](int x, int y) {
                return power[x][b] < power[y][b];
            });

            int hp = h[b];
            for (int w : attackers[b]) {
                if (hp <= 0) break;
                answer.push_back({w, b});
                hp -= power[w][b];
            }
            while (hp > 0) {
                answer.push_back({-1, b});
                --hp;
            }
        }

        for (auto [w, b] : answer) cout << w << ' ' << b << '\n';
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n;
    cin >> n;

    // Derive a deterministic random seed from the input.
    uint64_t seed = 0x9e3779b97f4a7c15ULL;
    Solver solver(n, seed);

    for (int i = 0; i < n; ++i) cin >> solver.h[i];
    for (int i = 0; i < n; ++i) cin >> solver.c[i];
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            cin >> solver.power[i][j];
            seed ^= uint64_t(solver.power[i][j] + 1009 * i + 9176 * j);
            seed *= 0xbf58476d1ce4e5b9ULL;
        }
    }
    solver.random.x ^= seed;

    solver.make_greedy_solution(false);
    solver.save_best();
    solver.make_greedy_solution(true);
    if (solver.current_cost < solver.best_cost) {
        solver.save_best();
    } else {
        solver.restore_best();
    }
    solver.improve();
    solver.print_answer();
    return 0;
}
