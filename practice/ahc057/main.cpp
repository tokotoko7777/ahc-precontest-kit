#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace std;

struct MovingPoint {
    int x = 0;
    int y = 0;
    int vx = 0;
    int vy = 0;
};

struct Problem {
    int n = 0;
    int turns = 0;
    int group_count = 0;
    int group_size = 0;
    int torus_size = 0;
    vector<MovingPoint> point;
    vector<int> position_x;
    vector<int> position_y;
};

struct Edge {
    int first = -1;
    int second = -1;
};

struct Tree {
    long long cost = 0;
    vector<Edge> edges;
};

struct GroupPlan {
    int time = 0;
    Tree tree;
};

struct Operation {
    int time = 0;
    int first = 0;
    int second = 0;
};

struct Solution {
    long long cost = numeric_limits<long long>::max();
    vector<vector<int>> groups;
    vector<int> times;
    vector<Operation> operations;
};

int positive_mod(long long value, int modulus) {
    value %= modulus;
    if (value < 0) {
        value += modulus;
    }
    return static_cast<int>(value);
}

int position_index(const Problem& problem, int time, int point) {
    return time * problem.n + point;
}

long long distance_squared(const Problem& problem, int time, int first,
                           int second) {
    const int first_index = position_index(problem, time, first);
    const int second_index = position_index(problem, time, second);
    int dx = abs(problem.position_x[first_index] - problem.position_x[second_index]);
    int dy = abs(problem.position_y[first_index] - problem.position_y[second_index]);
    dx = min(dx, problem.torus_size - dx);
    dy = min(dy, problem.torus_size - dy);
    return 1LL * dx * dx + 1LL * dy * dy;
}

int rounded_distance(const Problem& problem, int time, int first, int second) {
    return static_cast<int>(llround(sqrt(
        static_cast<double>(distance_squared(problem, time, first, second)))));
}

Tree minimum_spanning_tree(const Problem& problem, const vector<int>& members,
                           int time, bool save_edges) {
    const int size = static_cast<int>(members.size());
    vector<int> best(size, numeric_limits<int>::max());
    vector<int> parent(size, -1);
    vector<unsigned char> used(size, 0);
    best[0] = 0;

    Tree tree;
    if (save_edges) {
        tree.edges.reserve(size - 1);
    }
    for (int step = 0; step < size; ++step) {
        int chosen = -1;
        for (int i = 0; i < size; ++i) {
            if (used[i] == 0 && (chosen == -1 || best[i] < best[chosen])) {
                chosen = i;
            }
        }
        used[chosen] = 1;
        tree.cost += best[chosen];
        if (save_edges && parent[chosen] != -1) {
            tree.edges.push_back({members[chosen], members[parent[chosen]]});
        }
        for (int next = 0; next < size; ++next) {
            if (used[next] != 0) {
                continue;
            }
            const int distance = rounded_distance(
                problem, time, members[chosen], members[next]);
            if (distance < best[next]) {
                best[next] = distance;
                parent[next] = chosen;
            }
        }
    }
    return tree;
}

// Assign all points to fixed-capacity centers.  The point with the largest
// regret (second-best distance minus best distance) is assigned first.
vector<vector<int>> balanced_assignment(const Problem& problem, int time,
                                        const vector<int>& centers) {
    vector<vector<int>> groups(problem.group_count);
    vector<unsigned char> assigned(problem.n, 0);
    vector<int> capacity(problem.group_count, 0);

    for (int assigned_count = 0; assigned_count < problem.n; ++assigned_count) {
        int chosen_point = -1;
        int chosen_group = -1;
        long long chosen_regret = -1;
        long long chosen_best_distance = numeric_limits<long long>::max();

        for (int point = 0; point < problem.n; ++point) {
            if (assigned[point] != 0) {
                continue;
            }
            long long best_distance = numeric_limits<long long>::max();
            long long second_distance = numeric_limits<long long>::max();
            int best_group = -1;
            for (int group = 0; group < problem.group_count; ++group) {
                if (capacity[group] >= problem.group_size) {
                    continue;
                }
                const long long distance = distance_squared(
                    problem, time, point, centers[group]);
                if (distance < best_distance) {
                    second_distance = best_distance;
                    best_distance = distance;
                    best_group = group;
                } else if (distance < second_distance) {
                    second_distance = distance;
                }
            }
            const long long regret = second_distance == numeric_limits<long long>::max()
                                         ? numeric_limits<long long>::max() / 4
                                         : second_distance - best_distance;
            if (regret > chosen_regret ||
                (regret == chosen_regret && best_distance < chosen_best_distance)) {
                chosen_regret = regret;
                chosen_best_distance = best_distance;
                chosen_point = point;
                chosen_group = best_group;
            }
        }

        assigned[chosen_point] = 1;
        ++capacity[chosen_group];
        groups[chosen_group].push_back(chosen_point);
    }
    return groups;
}

int medoid(const Problem& problem, const vector<int>& group, int time) {
    int best_point = group.front();
    long long best_sum = numeric_limits<long long>::max();
    for (int candidate : group) {
        long long sum = 0;
        for (int other : group) {
            sum += distance_squared(problem, time, candidate, other);
        }
        if (sum < best_sum) {
            best_sum = sum;
            best_point = candidate;
        }
    }
    return best_point;
}

vector<vector<int>> make_groups(const Problem& problem, int time,
                                int first_center) {
    vector<int> centers;
    centers.push_back(first_center);
    while (static_cast<int>(centers.size()) < problem.group_count) {
        int farthest_point = -1;
        long long farthest_distance = -1;
        for (int point = 0; point < problem.n; ++point) {
            long long nearest = numeric_limits<long long>::max();
            for (int center : centers) {
                nearest = min(nearest,
                              distance_squared(problem, time, point, center));
            }
            if (nearest > farthest_distance) {
                farthest_distance = nearest;
                farthest_point = point;
            }
        }
        centers.push_back(farthest_point);
    }

    vector<vector<int>> groups;
    for (int iteration = 0; iteration < 3; ++iteration) {
        groups = balanced_assignment(problem, time, centers);
        for (int group = 0; group < problem.group_count; ++group) {
            centers[group] = medoid(problem, groups[group], time);
        }
    }
    return groups;
}

long long partition_cost_at_time(const Problem& problem,
                                 const vector<vector<int>>& groups,
                                 int time) {
    long long cost = 0;
    for (const vector<int>& group : groups) {
        cost += minimum_spanning_tree(problem, group, time, false).cost;
    }
    return cost;
}

GroupPlan best_group_plan(const Problem& problem, const vector<int>& group) {
    int best_time = 0;
    long long best_cost = numeric_limits<long long>::max();
    constexpr int coarse_step = 10;
    for (int time = 0; time < problem.turns; time += coarse_step) {
        const long long cost =
            minimum_spanning_tree(problem, group, time, false).cost;
        if (cost < best_cost) {
            best_cost = cost;
            best_time = time;
        }
    }

    const int left = max(0, best_time - coarse_step + 1);
    const int right = min(problem.turns - 1, best_time + coarse_step - 1);
    for (int time = left; time <= right; ++time) {
        const long long cost =
            minimum_spanning_tree(problem, group, time, false).cost;
        if (cost < best_cost) {
            best_cost = cost;
            best_time = time;
        }
    }
    return {best_time,
            minimum_spanning_tree(problem, group, best_time, true)};
}

Solution build_solution(const Problem& problem,
                        const vector<vector<int>>& groups) {
    Solution solution;
    solution.cost = 0;
    solution.groups = groups;
    solution.times.resize(problem.group_count);
    solution.operations.reserve(problem.n - problem.group_count);

    for (int group = 0; group < problem.group_count; ++group) {
        GroupPlan plan = best_group_plan(problem, groups[group]);
        solution.times[group] = plan.time;
        solution.cost += plan.tree.cost;
        for (const Edge& edge : plan.tree.edges) {
            solution.operations.push_back(
                {plan.time, edge.first, edge.second});
        }
    }
    return solution;
}

Solution baseline_solution(const Problem& problem) {
    Solution solution;
    solution.cost = 0;
    solution.groups.resize(problem.group_count);
    solution.times.assign(problem.group_count, 0);
    for (int group = 0; group < problem.group_count; ++group) {
        const int root = group * problem.group_size;
        for (int offset = 0; offset < problem.group_size; ++offset) {
            solution.groups[group].push_back(root + offset);
        }
        for (int offset = 1; offset < problem.group_size; ++offset) {
            const int point = root + offset;
            solution.cost += rounded_distance(problem, 0, root, point);
            solution.operations.push_back({0, root, point});
        }
    }
    return solution;
}

vector<vector<int>> improve_by_swaps(const Problem& problem,
                                     const Solution& initial,
                                     uint64_t seed) {
    vector<vector<int>> current = initial.groups;
    vector<vector<int>> best_groups = current;
    vector<long long> group_cost(problem.group_count, 0);
    long long current_cost = 0;
    for (int group = 0; group < problem.group_count; ++group) {
        group_cost[group] = minimum_spanning_tree(
                                problem, current[group], initial.times[group], false)
                                .cost;
        current_cost += group_cost[group];
    }
    long long best_cost = current_cost;

    mt19937_64 random(seed);
    uniform_real_distribution<double> random_real(0.0, 1.0);
    constexpr int iterations = 30000;
    constexpr double start_temperature = 2500.0;
    constexpr double end_temperature = 5.0;
    for (int iteration = 0; iteration < iterations; ++iteration) {
        int first_group = static_cast<int>(random() %
                                           static_cast<uint64_t>(problem.group_count));
        int second_group = static_cast<int>(random() %
                                            static_cast<uint64_t>(problem.group_count));
        if (first_group == second_group) {
            continue;
        }
        const int first_index = static_cast<int>(
            random() % static_cast<uint64_t>(problem.group_size));
        const int second_index = static_cast<int>(
            random() % static_cast<uint64_t>(problem.group_size));
        swap(current[first_group][first_index], current[second_group][second_index]);

        const long long first_cost =
            minimum_spanning_tree(problem, current[first_group],
                                  initial.times[first_group], false)
                .cost;
        const long long second_cost =
            minimum_spanning_tree(problem, current[second_group],
                                  initial.times[second_group], false)
                .cost;
        const long long difference =
            first_cost + second_cost -
            group_cost[first_group] - group_cost[second_group];
        const double progress = static_cast<double>(iteration) / iterations;
        const double temperature = start_temperature *
            pow(end_temperature / start_temperature, progress);
        const bool accept = difference <= 0 ||
            random_real(random) < exp(-static_cast<double>(difference) / temperature);
        if (accept) {
            current_cost += difference;
            group_cost[first_group] = first_cost;
            group_cost[second_group] = second_cost;
            if (current_cost < best_cost) {
                best_cost = current_cost;
                best_groups = current;
            }
        } else {
            swap(current[first_group][first_index], current[second_group][second_index]);
        }
    }
    return best_groups;
}

uint64_t input_seed(const Problem& problem) {
    uint64_t hash = 1469598103934665603ULL;
    for (const MovingPoint& point : problem.point) {
        hash ^= static_cast<uint64_t>(point.x) +
                100001ULL * static_cast<uint64_t>(point.y);
        hash *= 1099511628211ULL;
        hash ^= static_cast<uint64_t>(point.vx + 101) +
                203ULL * static_cast<uint64_t>(point.vy + 101);
        hash *= 1099511628211ULL;
    }
    return hash;
}

void print_solution(vector<Operation> operations) {
    sort(operations.begin(), operations.end(), [](const Operation& lhs,
                                                   const Operation& rhs) {
        return tie(lhs.time, lhs.first, lhs.second) <
               tie(rhs.time, rhs.first, rhs.second);
    });
    for (const Operation& operation : operations) {
        cout << operation.time << ' ' << operation.first << ' '
             << operation.second << '\n';
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Problem problem;
    cin >> problem.n >> problem.turns >> problem.group_count >>
        problem.group_size >> problem.torus_size;
    problem.point.resize(problem.n);
    for (MovingPoint& point : problem.point) {
        cin >> point.x >> point.y >> point.vx >> point.vy;
    }

    problem.position_x.resize(problem.turns * problem.n);
    problem.position_y.resize(problem.turns * problem.n);
    for (int time = 0; time < problem.turns; ++time) {
        for (int point = 0; point < problem.n; ++point) {
            const int index = position_index(problem, time, point);
            problem.position_x[index] = positive_mod(
                1LL * problem.point[point].x +
                    1LL * problem.point[point].vx * time,
                problem.torus_size);
            problem.position_y[index] = positive_mod(
                1LL * problem.point[point].y +
                    1LL * problem.point[point].vy * time,
                problem.torus_size);
        }
    }

    Solution best = baseline_solution(problem);
#ifdef SIMPLE_BASELINE
    print_solution(best.operations);
    return 0;
#endif
#ifdef LOCAL
    cerr << "baseline_cost " << best.cost << '\n';
#endif

    vector<pair<long long, vector<vector<int>>>> candidates;
    constexpr int time_step = 50;
    for (int time = 0; time < problem.turns; time += time_step) {
        for (int restart = 0; restart < 2; ++restart) {
            const int first_center =
                (time * 37 + restart * 149 + 17) % problem.n;
            vector<vector<int>> groups =
                make_groups(problem, time, first_center);
            const long long cost = partition_cost_at_time(problem, groups, time);
            candidates.emplace_back(cost, move(groups));
        }
    }
    sort(candidates.begin(), candidates.end(),
         [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

    constexpr int finalist_count = 6;
    for (int index = 0; index < finalist_count; ++index) {
        Solution candidate = build_solution(problem, candidates[index].second);
#ifdef LOCAL
        cerr << "finalist_" << index << " cost " << candidate.cost << '\n';
#endif
        if (candidate.cost < best.cost) {
            best = move(candidate);
        }
    }

    const uint64_t seed = input_seed(problem);
    for (int round = 0; round < 2; ++round) {
        vector<vector<int>> improved_groups = improve_by_swaps(
            problem, best, seed + 2000006ULL * static_cast<uint64_t>(round));
        Solution improved = build_solution(problem, improved_groups);
#ifdef LOCAL
        cerr << "after_swaps_" << round << " cost " << improved.cost << '\n';
#endif
        if (improved.cost < best.cost) {
            best = move(improved);
        }
    }

    print_solution(best.operations);
    return 0;
}
