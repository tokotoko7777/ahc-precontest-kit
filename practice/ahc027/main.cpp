#include <bits/stdc++.h>
using namespace std;

// Compile with -DAHC027_DFS_BASELINE to output only the sample-style DFS tour.
// Compile with -DAHC027_SINGLE_POLICY to try only one cleaning policy.
// Compile with -DAHC027_SHORT_VERIFY to shorten the route for a 100-case check.

struct Policy {
    int distance_mode;
    long double lookahead;
    int route_length;
    int commit_steps;
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int N;
    cin >> N;
    vector<string> horizontal_wall(N - 1);
    vector<string> vertical_wall(N);
    for (string& row : horizontal_wall) cin >> row;
    for (string& row : vertical_wall) cin >> row;

    const int vertex_count = N * N;
    vector<int> dirt(vertex_count);
    for (int row = 0; row < N; ++row) {
        for (int column = 0; column < N; ++column) {
            cin >> dirt[row * N + column];
        }
    }

    vector<vector<int>> graph(vertex_count);
    auto connect = [&](int left, int right) {
        graph[left].push_back(right);
        graph[right].push_back(left);
    };
    for (int row = 0; row < N; ++row) {
        for (int column = 0; column < N; ++column) {
            const int here = row * N + column;
            if (row + 1 < N && horizontal_wall[row][column] == '0') {
                connect(here, here + N);
            }
            if (column + 1 < N && vertical_wall[row][column] == '0') {
                connect(here, here + 1);
            }
        }
    }

    auto move_character = [&](int from, int to) {
        if (to == from - N) return 'U';
        if (to == from + N) return 'D';
        if (to == from - 1) return 'L';
        return 'R';
    };

    auto depth_first_tour = [&]() {
        vector<int> tour(1, 0);
        vector<char> visited(vertex_count, false);
        auto dfs = [&](auto&& self, int here) -> void {
            visited[here] = true;
            for (int next : graph[here]) {
                if (visited[next]) continue;
                tour.push_back(next);
                self(self, next);
                tour.push_back(here);
            }
        };
        dfs(dfs, 0);
        return tour;
    };

#ifdef AHC027_DFS_BASELINE
    const vector<int> baseline_answer = depth_first_tour();
    for (int index = 1; index < static_cast<int>(baseline_answer.size()); ++index) {
        cout << move_character(baseline_answer[index - 1], baseline_answer[index]);
    }
    cout << '\n';
    return 0;
#endif

    // dist[source][destination] is at most N*N-1, so uint16_t is enough.
    const uint16_t unreachable = numeric_limits<uint16_t>::max();
    vector<vector<uint16_t>> dist(
        vertex_count, vector<uint16_t>(vertex_count, unreachable));
    vector<int> bfs_queue(vertex_count);
    for (int source = 0; source < vertex_count; ++source) {
        int head = 0;
        int tail = 0;
        bfs_queue[tail++] = source;
        dist[source][source] = 0;
        while (head < tail) {
            const int here = bfs_queue[head++];
            for (int next : graph[here]) {
                if (dist[source][next] != unreachable) continue;
                dist[source][next] = static_cast<uint16_t>(dist[source][here] + 1);
                bfs_queue[tail++] = next;
            }
        }
    }

    // The exact cyclic objective before rounding.  Each visit gap g contributes
    // d * (0+1+...+(g-1)) = d*g*(g-1)/2.
    auto exact_numerator = [&](const vector<int>& route) {
        const int length = static_cast<int>(route.size()) - 1;
        vector<int> first(vertex_count, -1);
        vector<int> previous(vertex_count, -1);
        vector<char> seen(vertex_count, false);
        long long numerator = 0;
        for (int time = 1; time <= length; ++time) {
            const int cell = route[time];
            seen[cell] = true;
            if (first[cell] == -1) {
                first[cell] = time;
            } else {
                const long long gap = time - previous[cell];
                numerator += static_cast<long long>(dirt[cell])
                           * gap * (gap - 1) / 2;
            }
            previous[cell] = time;
        }
        for (int cell = 0; cell < vertex_count; ++cell) {
            if (!seen[cell]) return numeric_limits<long long>::max();
            const long long wrap_gap = first[cell] + length - previous[cell];
            numerator += static_cast<long long>(dirt[cell])
                       * wrap_gap * (wrap_gap - 1) / 2;
        }
        return numerator;
    };

    auto make_route = [&](const Policy& policy) {
#ifdef AHC027_SHORT_VERIFY
        const int warmup_length = 6000;
        const int answer_length = 35000;
#else
        const int warmup_length = 18000;
        const int answer_length = policy.route_length;
#endif
        vector<int> last_visit(vertex_count, 0);
        vector<int> route(1, 0);
        int current = 0;
        int time = 0;

        auto take_step = [&](int next) {
            current = next;
            ++time;
            last_visit[current] = time;
            route.push_back(current);
        };

        auto distance_penalty = [&](int distance) -> long double {
            const long double value = static_cast<long double>(distance + 1);
            if (policy.distance_mode == 0) return 1.0L;
            if (policy.distance_mode == 1) return sqrt(value);
            if (policy.distance_mode == 2) return value;
            return value * sqrt(value);
        };

        auto choose_target = [&]() {
            int best = -1;
            long double best_priority = -1.0L;
            for (int cell = 0; cell < vertex_count; ++cell) {
                if (cell == current) continue;
                const int distance = dist[current][cell];
                const long double predicted_age =
                    static_cast<long double>(time - last_visit[cell])
                    + policy.lookahead * distance;
                const long double priority =
                    static_cast<long double>(dirt[cell])
                    * predicted_age * predicted_age
                    / distance_penalty(distance);
                if (priority > best_priority) {
                    best_priority = priority;
                    best = cell;
                }
            }
            return best;
        };

        auto walk_to = [&](int target, int step_limit) {
            int steps = 0;
            while (current != target && steps < step_limit) {
                int best_next = -1;
                long long best_bonus = -1;
                for (int next : graph[current]) {
                    if (dist[next][target] + 1 != dist[current][target]) continue;
                    const long long age = time + 1LL - last_visit[next];
                    const long long bonus = static_cast<long long>(dirt[next])
                                              * age * age;
                    if (bonus > best_bonus) {
                        best_bonus = bonus;
                        best_next = next;
                    }
                }
                take_step(best_next);
                ++steps;
            }
        };

        // Discard an initial transient, then cut the cyclic answer at (0,0).
        while (time < warmup_length) {
            walk_to(choose_target(), policy.commit_steps);
        }
        walk_to(0, numeric_limits<int>::max());
        route.clear();
        route.push_back(0);
        const int answer_start_time = time;

        while (time - answer_start_time < answer_length) {
            const int target = choose_target();
            const int used = time - answer_start_time;
            const int needed = dist[current][target] + dist[target][0];
            if (used + needed > 99990) break;
            walk_to(target, policy.commit_steps);
        }
        walk_to(0, numeric_limits<int>::max());

        // This should not be needed after tens of thousands of moves.  It is a
        // simple safety net for unusually thin mazes and the short test build.
        vector<char> visited(vertex_count, false);
        for (int cell : route) visited[cell] = true;
        bool missing = false;
        for (char value : visited) missing |= !value;
        if (missing) return depth_first_tour();
        return route;
    };

    vector<Policy> policies = {
        {2, 1.25L, 50000, numeric_limits<int>::max()},
        {2, 1.50L, 97000, numeric_limits<int>::max()},
        {2, 1.00L, 70000, 16},
        {2, 1.00L, 70000, 8},
        {2, 1.00L, 70000, 4},
    };
#ifdef AHC027_SINGLE_POLICY
    policies.resize(1);
#endif
#ifdef AHC027_SHORT_VERIFY
    policies.resize(1);
#endif

    vector<int> answer;
    long long best_numerator = numeric_limits<long long>::max();
    vector<int> sorted_dirt = dirt;
    sort(sorted_dirt.begin(), sorted_dirt.end());

    auto remove_small_backtracks = [&](const vector<int>& original,
                                       int dirt_limit) {
        vector<int> occurrence(vertex_count, 0);
        for (int cell : original) ++occurrence[cell];
        vector<int> result;
        result.reserve(original.size());
        result.push_back(0);
        for (int index = 1; index < static_cast<int>(original.size()); ++index) {
            const int cell = original[index];
            if (result.size() >= 2 && result[result.size() - 2] == cell) {
                const int middle = result.back();
                if (dirt[middle] <= dirt_limit
                    && occurrence[middle] > 1
                    && occurrence[cell] > 1) {
                    result.pop_back();
                    --occurrence[middle];
                    --occurrence[cell];
                    continue;
                }
            }
            result.push_back(cell);
        }
        return result;
    };

    auto consider = [&](vector<int> candidate, const Policy& policy,
                        const char* variant) {
        const long long numerator = exact_numerator(candidate);
        const long long candidate_length = static_cast<long long>(candidate.size()) - 1;
        const long long answer_length = static_cast<long long>(answer.size()) - 1;
#ifdef LOCAL
        cerr << "mode=" << policy.distance_mode
             << " lookahead=" << static_cast<double>(policy.lookahead)
             << " target_length=" << policy.route_length
             << " commit=" << policy.commit_steps
             << " variant=" << variant
             << " length=" << candidate_length
             << " score=" << static_cast<long long>(
                    static_cast<long double>(numerator) / candidate_length + 0.5L)
             << '\n';
#else
        (void)policy;
        (void)variant;
#endif
        if (answer.empty()
            || static_cast<long double>(numerator) / candidate_length
               < static_cast<long double>(best_numerator) / answer_length) {
            best_numerator = numerator;
            answer = move(candidate);
        }
    };

    for (const Policy& policy : policies) {
        vector<int> candidate = make_route(policy);
        consider(candidate, policy, "original");
        const array<int, 4> percentiles = {20, 40, 60, 80};
        for (int percentile : percentiles) {
            const int dirt_limit =
                sorted_dirt[vertex_count * percentile / 100];
            consider(remove_small_backtracks(candidate, dirt_limit),
                     policy, "trimmed");
        }
    }

    for (int index = 1; index < static_cast<int>(answer.size()); ++index) {
        cout << move_character(answer[index - 1], answer[index]);
    }
    cout << '\n';
}
