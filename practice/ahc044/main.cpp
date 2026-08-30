#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <tuple>
#include <vector>
using namespace std;

struct Rule {
    array<int, 100> odd{};
    array<int, 100> even{};
};

struct Evaluation {
    long long error = (1LL << 60);
    array<int, 100> visits{};
};

Evaluation evaluate(const Rule& rule, const array<int, 100>& target,
                    int weeks) {
    Evaluation result;
    result.visits.fill(0);
    int employee = 0;
    for (int week = 0; week < weeks; ++week) {
        ++result.visits[employee];
        employee = (result.visits[employee] & 1) ? rule.odd[employee]
                                                 : rule.even[employee];
    }
    result.error = 0;
    for (int i = 0; i < 100; ++i) {
        result.error += llabs((long long)result.visits[i] - target[i]);
    }
    return result;
}

vector<int> make_pendulum_order(const array<int, 100>& target, int variant) {
    vector<int> positive;
    for (int i = 0; i < 100; ++i) {
        if (target[i] > 0) positive.push_back(i);
    }
    sort(positive.begin(), positive.end(), [&](int a, int b) {
        return make_pair(target[a], a) < make_pair(target[b], b);
    });
    if (variant & 1) reverse(positive.begin(), positive.end());

    vector<int> order;
    for (int parity = 0; parity < 2; ++parity) {
        vector<int> part;
        for (int i = parity; i < (int)positive.size(); i += 2) {
            part.push_back(positive[i]);
        }
        if (parity == 1) reverse(part.begin(), part.end());
        order.insert(order.end(), part.begin(), part.end());
    }
    if (variant & 2) reverse(order.begin(), order.end());
    return order;
}

Rule make_initial_rule(const array<int, 100>& target,
                       const vector<int>& cycle_order, int assignment_mode) {
    Rule rule;
    int first_positive = cycle_order.empty() ? 0 : cycle_order[0];
    for (int i = 0; i < 100; ++i) {
        rule.odd[i] = first_positive;
        rule.even[i] = first_positive;
    }
    if (cycle_order.empty()) return rule;

    array<int, 100> previous{};
    for (int position = 0; position < (int)cycle_order.size(); ++position) {
        int current = cycle_order[position];
        int next = cycle_order[(position + 1) % cycle_order.size()];
        rule.odd[current] = next;
        previous[next] = current;
    }

    vector<double> capacity(100, 0.0), load(100, 0.0);
    for (int destination : cycle_order) {
        capacity[destination] =
            target[destination] - 0.5 * target[previous[destination]];
    }

    vector<int> sources = cycle_order;
    sort(sources.begin(), sources.end(), [&](int a, int b) {
        if (assignment_mode == 1) {
            return make_pair(target[a], a) < make_pair(target[b], b);
        }
        return make_pair(target[a], a) > make_pair(target[b], b);
    });
    for (int source : sources) {
        double weight = 0.5 * target[source];
        int best_destination = cycle_order[0];
        double best_change = numeric_limits<double>::infinity();
        for (int destination : cycle_order) {
            double before = abs(load[destination] - capacity[destination]);
            double after =
                abs(load[destination] + weight - capacity[destination]);
            double change = after - before;
            if (make_pair(change, destination) <
                make_pair(best_change, best_destination)) {
                best_change = change;
                best_destination = destination;
            }
        }
        rule.even[source] = best_destination;
        load[best_destination] += weight;
    }
    return rule;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, weeks;
    cin >> n >> weeks;
    array<int, 100> target{};
    for (int& value : target) cin >> value;

#ifdef AHC044_BASELINE
    for (int i = 0; i < n; ++i) {
        cout << (i + 1) % n << ' ' << (i + 1) % n << '\n';
    }
    return 0;
#endif

    Rule best_rule;
    Evaluation best;
    for (int variant = 0; variant < 4; ++variant) {
        vector<int> order = make_pendulum_order(target, variant);
        for (int mode = 0; mode < 2; ++mode) {
            Rule candidate = make_initial_rule(target, order, mode);
            Evaluation evaluation = evaluate(candidate, target, weeks);
            if (evaluation.error < best.error) {
                best = evaluation;
                best_rule = candidate;
            }
        }
    }

    // Redirect an even edge from an overrepresented destination to an
    // underrepresented one.  Every trial is scored by the exact 500,000-week
    // simulation; failed changes are immediately undone.
    mt19937 random(20250309);
    uniform_int_distribution<int> employee_distribution(0, n - 1);
    Rule current_rule = best_rule;
    Evaluation current = best;
#ifdef LOCAL
    const long long initial_error = best.error;
#endif
    for (int iteration = 0; iteration < 700; ++iteration) {
        vector<int> over, under;
        for (int i = 0; i < n; ++i) {
            if (current.visits[i] > target[i]) over.push_back(i);
            if (current.visits[i] < target[i]) under.push_back(i);
        }
        if (under.empty()) break;

        int source = employee_distribution(random);
        for (int retry = 0; retry < 12; ++retry) {
            int candidate = employee_distribution(random);
            if (find(over.begin(), over.end(), current_rule.even[candidate]) !=
                over.end()) {
                source = candidate;
                break;
            }
        }
        int destination = under[random() % under.size()];
        int old_destination = current_rule.even[source];
        current_rule.even[source] = destination;
        Evaluation next = evaluate(current_rule, target, weeks);
        if (next.error <= current.error) {
            current = next;
            if (next.error < best.error) {
                best = next;
                best_rule = current_rule;
            }
        } else {
            current_rule.even[source] = old_destination;
        }
    }

    // Finish with a small coordinate descent.  The estimated gain assumes
    // that redirecting an edge moves about half of its source visits.  Only
    // the promising moves are then checked by the exact simulator.
    struct EdgeMove {
        long long estimated_gain;
        int source;
        int destination;
    };
    for (int round = 0; round < 28; ++round) {
        vector<EdgeMove> moves;
        for (int source = 0; source < n; ++source) {
            int old_destination = current_rule.even[source];
            long long moved = current.visits[source] / 2;
            for (int destination = 0; destination < n; ++destination) {
                if (destination == old_destination ||
                    current.visits[destination] >= target[destination]) {
                    continue;
                }
                long long old_difference =
                    (long long)current.visits[old_destination] -
                    target[old_destination];
                long long new_difference =
                    (long long)current.visits[destination] -
                    target[destination];
                long long before =
                    llabs(old_difference) + llabs(new_difference);
                long long after = llabs(old_difference - moved) +
                                  llabs(new_difference + moved);
                moves.push_back(
                    {before - after, source, destination});
            }
        }
        sort(moves.begin(), moves.end(), [](const EdgeMove& x,
                                             const EdgeMove& y) {
            return tie(x.estimated_gain, x.source, x.destination) >
                   tie(y.estimated_gain, y.source, y.destination);
        });

        Evaluation round_best = current;
        Rule round_rule = current_rule;
        int checks = min(14, (int)moves.size());
        for (int i = 0; i < checks; ++i) {
            Rule candidate = current_rule;
            candidate.even[moves[i].source] = moves[i].destination;
            Evaluation evaluation = evaluate(candidate, target, weeks);
            if (evaluation.error < round_best.error) {
                round_best = evaluation;
                round_rule = candidate;
            }
        }
        if (round_best.error >= current.error) break;
        current = round_best;
        current_rule = round_rule;
        if (current.error < best.error) {
            best = current;
            best_rule = current_rule;
        }
    }

    // The odd edges formed the safety cycle.  Near the end, cautiously allow
    // a few of them to change too; an exact full-horizon check rejects traps.
#ifndef AHC044_SKIP_ODD_SEARCH
    for (int round = 0; round < 12; ++round) {
        vector<EdgeMove> moves;
        for (int source = 0; source < n; ++source) {
            int old_destination = current_rule.odd[source];
            long long moved = (current.visits[source] + 1LL) / 2;
            for (int destination = 0; destination < n; ++destination) {
                if (destination == old_destination ||
                    current.visits[destination] >= target[destination]) {
                    continue;
                }
                long long old_difference =
                    (long long)current.visits[old_destination] -
                    target[old_destination];
                long long new_difference =
                    (long long)current.visits[destination] -
                    target[destination];
                long long before =
                    llabs(old_difference) + llabs(new_difference);
                long long after = llabs(old_difference - moved) +
                                  llabs(new_difference + moved);
                moves.push_back(
                    {before - after, source, destination});
            }
        }
        sort(moves.begin(), moves.end(), [](const EdgeMove& x,
                                             const EdgeMove& y) {
            return tie(x.estimated_gain, x.source, x.destination) >
                   tie(y.estimated_gain, y.source, y.destination);
        });

        Evaluation round_best = current;
        Rule round_rule = current_rule;
        int checks = min(12, (int)moves.size());
        for (int i = 0; i < checks; ++i) {
            Rule candidate = current_rule;
            candidate.odd[moves[i].source] = moves[i].destination;
            Evaluation evaluation = evaluate(candidate, target, weeks);
            if (evaluation.error < round_best.error) {
                round_best = evaluation;
                round_rule = candidate;
            }
        }
        if (round_best.error >= current.error) break;
        current = round_best;
        current_rule = round_rule;
        if (current.error < best.error) {
            best = current;
            best_rule = current_rule;
        }
    }
#endif

#ifdef LOCAL
    cerr << "initial_error=" << initial_error << " final_error=" << best.error
         << '\n';
#endif

    for (int i = 0; i < n; ++i) {
        cout << best_rule.odd[i] << ' ' << best_rule.even[i] << '\n';
    }
}
