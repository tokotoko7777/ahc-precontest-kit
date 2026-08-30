#include <bits/stdc++.h>
using namespace std;

#ifndef AHC017_TIME_LIMIT
#define AHC017_TIME_LIMIT 5.70
#endif

#ifndef AHC017_SAMPLE_COUNT
#define AHC017_SAMPLE_COUNT 16
#endif

struct Timer {
    chrono::steady_clock::time_point start = chrono::steady_clock::now();

    double seconds() const {
        return chrono::duration<double>(chrono::steady_clock::now() - start).count();
    }
};

struct Random {
    uint64_t state;

    explicit Random(uint64_t seed) : state(seed) {}

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

    double next_double() {
        return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0);
    }
};

struct Edge {
    int from;
    int to;
    int weight;
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Timer timer;

    int N, M, D, K;
    cin >> N >> M >> D >> K;

    vector<Edge> edges(M);
    vector<vector<pair<int, int>>> graph(N);
    vector<int> degree(N, 0);
    uint64_t input_hash = 0x123456789abcdef0ULL;
    for (int id = 0; id < M; ++id) {
        int u, v, w;
        cin >> u >> v >> w;
        --u;
        --v;
        edges[id] = {u, v, w};
        graph[u].push_back({v, id});
        graph[v].push_back({u, id});
        ++degree[u];
        ++degree[v];
        input_hash ^= static_cast<uint64_t>(u + 1) * 0x9e3779b97f4a7c15ULL;
        input_hash ^= static_cast<uint64_t>(v + 1) * 0xbf58476d1ce4e5b9ULL;
        input_hash ^= static_cast<uint64_t>(w) * 0x94d049bb133111ebULL;
    }

    vector<int> x(N), y(N);
    for (int vertex = 0; vertex < N; ++vertex) {
        cin >> x[vertex] >> y[vertex];
    }
    Random random(input_hash);

    // Pick geographically spread sources.  Each unpicked vertex is represented
    // by its nearest source, so sparse parts of the map are not ignored.
    const int sample_count = min(N, static_cast<int>(AHC017_SAMPLE_COUNT));
    vector<int> samples;
    vector<long long> nearest_squared(N, (1LL << 60));
    int first_source = 0;
    long long best_center_distance = (1LL << 60);
    for (int vertex = 0; vertex < N; ++vertex) {
        const long long dx = x[vertex] - 500;
        const long long dy = y[vertex] - 500;
        const long long distance = dx * dx + dy * dy;
        if (distance < best_center_distance) {
            best_center_distance = distance;
            first_source = vertex;
        }
    }
    samples.push_back(first_source);
    for (int count = 1; count < sample_count; ++count) {
        const int newest = samples.back();
        int farthest = 0;
        for (int vertex = 0; vertex < N; ++vertex) {
            const long long dx = x[vertex] - x[newest];
            const long long dy = y[vertex] - y[newest];
            nearest_squared[vertex] = min(nearest_squared[vertex], dx * dx + dy * dy);
            if (nearest_squared[vertex] > nearest_squared[farthest]) farthest = vertex;
        }
        samples.push_back(farthest);
    }

    vector<int> sample_weight(sample_count, 0);
    for (int vertex = 0; vertex < N; ++vertex) {
        int nearest = 0;
        long long distance_to_nearest = (1LL << 60);
        for (int index = 0; index < sample_count; ++index) {
            const long long dx = x[vertex] - x[samples[index]];
            const long long dy = y[vertex] - y[samples[index]];
            const long long distance = dx * dx + dy * dy;
            if (distance < distance_to_nearest) {
                distance_to_nearest = distance;
                nearest = index;
            }
        }
        ++sample_weight[nearest];
    }

    const int INF = 1000000000;
    vector<int> repair_day(M, -1);
    vector<int> distance(N, INF);
    vector<int> parent_edge(N, -1);

    auto dijkstra = [&](int source, int blocked_day, bool save_parent) {
        fill(distance.begin(), distance.end(), INF);
        if (save_parent) fill(parent_edge.begin(), parent_edge.end(), -1);
        priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> queue;
        distance[source] = 0;
        queue.push({0, source});
        while (!queue.empty()) {
            const auto [current_distance, vertex] = queue.top();
            queue.pop();
            if (current_distance != distance[vertex]) continue;
            for (const auto& [next_vertex, edge_id] : graph[vertex]) {
                if (repair_day[edge_id] == blocked_day) continue;
                const int next_distance = current_distance + edges[edge_id].weight;
                if (next_distance < distance[next_vertex]) {
                    distance[next_vertex] = next_distance;
                    if (save_parent) parent_edge[next_vertex] = edge_id;
                    queue.push({next_distance, next_vertex});
                }
            }
        }
    };

    // Baseline distances are constant.  The parent trees also give a cheap
    // estimate of edges whose simultaneous closure should be avoided.
    vector<vector<int>> base_distance(sample_count, vector<int>(N));
    vector<long long> importance(M, 1);
    for (int index = 0; index < sample_count; ++index) {
        dijkstra(samples[index], -2, true);
        base_distance[index] = distance;
        for (int vertex = 0; vertex < N; ++vertex) {
            const int edge_id = parent_edge[vertex];
            if (edge_id >= 0) importance[edge_id] += sample_weight[index];
        }
    }

    auto evaluate_day = [&](int day) {
        long long total = 0;
        for (int index = 0; index < sample_count; ++index) {
            dijkstra(samples[index], day, false);
            const long long weight = sample_weight[index];
            for (int vertex = 0; vertex < N; ++vertex) {
                total += weight * static_cast<long long>(distance[vertex] - base_distance[index][vertex]);
            }
        }
        return total;
    };

    vector<int> target_size(D, M / D);
    for (int day = 0; day < M % D; ++day) ++target_size[day];

    auto random_schedule = [&]() {
        vector<int> order(M);
        iota(order.begin(), order.end(), 0);
        for (int index = M - 1; index > 0; --index) {
            swap(order[index], order[random.next_int(index + 1)]);
        }
        vector<int> schedule(M, 0);
        int position = 0;
        for (int day = 0; day < D; ++day) {
            for (int count = 0; count < target_size[day]; ++count) {
                schedule[order[position++]] = day;
            }
        }
        return schedule;
    };

    auto spatial_schedule = [&]() {
        vector<int> order(M);
        iota(order.begin(), order.end(), 0);
        sort(order.begin(), order.end(), [&](int left, int right) {
            const int left_x = x[edges[left].from] + x[edges[left].to];
            const int right_x = x[edges[right].from] + x[edges[right].to];
            const int left_y = y[edges[left].from] + y[edges[left].to];
            const int right_y = y[edges[right].from] + y[edges[right].to];
            const int left_block = left_x / 100;
            const int right_block = right_x / 100;
            if (left_block != right_block) return left_block < right_block;
            if (left_block & 1) return left_y > right_y;
            return left_y < right_y;
        });
        vector<int> schedule(M, 0);
        vector<int> remaining = target_size;
        int day = random.next_int(D);
        for (int edge_id : order) {
            while (remaining[day] == 0) day = (day + 1) % D;
            schedule[edge_id] = day;
            --remaining[day];
            day = (day + 1) % D;
        }
        return schedule;
    };

    auto greedy_schedule = [&]() {
        vector<int> order(M);
        iota(order.begin(), order.end(), 0);
        sort(order.begin(), order.end(), [&](int left, int right) {
            const long long left_risk = importance[left] * 1000000LL
                                      / min(degree[edges[left].from], degree[edges[left].to]);
            const long long right_risk = importance[right] * 1000000LL
                                       / min(degree[edges[right].from], degree[edges[right].to]);
            if (left_risk != right_risk) return left_risk > right_risk;
            return left < right;
        });

        vector<int> schedule(M, -1);
        vector<int> load(D, 0);
        vector<long long> day_importance(D, 0);
        vector<vector<unsigned char>> incident(D, vector<unsigned char>(N, 0));
        const int bucket_count = 10 * 10;
        vector<vector<unsigned short>> bucket(D, vector<unsigned short>(bucket_count, 0));

        long double average_importance = 0.0L;
        for (long long value : importance) average_importance += value;
        average_importance /= D;

        for (int edge_id : order) {
            const Edge& edge = edges[edge_id];
            const int middle_x = (x[edge.from] + x[edge.to]) / 2;
            const int middle_y = (y[edge.from] + y[edge.to]) / 2;
            const int bx = min(9, middle_x / 101);
            const int by = min(9, middle_y / 101);

            int best_day = -1;
            long double best_value = 1e100L;
            for (int day = 0; day < D; ++day) {
                if (load[day] >= target_size[day]) continue;
                long double value = 20.0L * day_importance[day] / max(1.0L, average_importance);
                value += 12.0L * (incident[day][edge.from] + incident[day][edge.to]);
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        const int nx = bx + dx;
                        const int ny = by + dy;
                        if (0 <= nx && nx < 10 && 0 <= ny && ny < 10) {
                            value += (dx == 0 && dy == 0 ? 3.0L : 0.5L)
                                   * bucket[day][nx * 10 + ny];
                        }
                    }
                }
                value += static_cast<long double>(load[day]) / target_size[day];
                value += random.next_double() * 1e-6;
                if (value < best_value) {
                    best_value = value;
                    best_day = day;
                }
            }
            schedule[edge_id] = best_day;
            ++load[best_day];
            day_importance[best_day] += importance[edge_id];
            ++incident[best_day][edge.from];
            ++incident[best_day][edge.to];
            ++bucket[best_day][bx * 10 + by];
        }
        return schedule;
    };

    vector<int> best_schedule;
    vector<long long> best_day_cost(D);
    long long best_total = (1LL << 62);

    vector<vector<int>> initial_schedules;
    initial_schedules.push_back(greedy_schedule());
    initial_schedules.push_back(spatial_schedule());
    initial_schedules.push_back(random_schedule());
    initial_schedules.push_back(random_schedule());

    for (const vector<int>& schedule : initial_schedules) {
        repair_day = schedule;
        vector<long long> cost(D);
        long long total = 0;
        for (int day = 0; day < D; ++day) {
            cost[day] = evaluate_day(day);
            total += cost[day];
        }
        if (total < best_total) {
            best_total = total;
            best_schedule = schedule;
            best_day_cost = cost;
        }
    }

    repair_day = best_schedule;
    vector<long long> day_cost = best_day_cost;
    long long current_total = best_total;
    vector<vector<int>> day_edges(D);
    for (int edge_id = 0; edge_id < M; ++edge_id) {
        day_edges[repair_day[edge_id]].push_back(edge_id);
    }

    const long double start_temperature = max(1.0L, static_cast<long double>(current_total) / D * 0.02L);
    const long double end_temperature = max(0.01L, start_temperature * 0.002L);
    long long iterations = 0;
    long long accepted = 0;

    while (timer.seconds() < AHC017_TIME_LIMIT) {
        ++iterations;
        int day_a;
        if ((iterations & 1LL) == 0) {
            // Pick the worst of three random days slightly more often.
            day_a = random.next_int(D);
            for (int repeat = 0; repeat < 2; ++repeat) {
                const int candidate = random.next_int(D);
                if (day_cost[candidate] > day_cost[day_a]) day_a = candidate;
            }
        } else {
            day_a = random.next_int(D);
        }
        int day_b = random.next_int(D - 1);
        if (day_b >= day_a) ++day_b;

        const int position_a = random.next_int(static_cast<int>(day_edges[day_a].size()));
        const int position_b = random.next_int(static_cast<int>(day_edges[day_b].size()));
        const int edge_a = day_edges[day_a][position_a];
        const int edge_b = day_edges[day_b][position_b];

        repair_day[edge_a] = day_b;
        repair_day[edge_b] = day_a;
        const long long new_cost_a = evaluate_day(day_a);
        const long long new_cost_b = evaluate_day(day_b);
        const long long difference = new_cost_a + new_cost_b
                                   - day_cost[day_a] - day_cost[day_b];

        const long double progress = min(1.0, timer.seconds() / AHC017_TIME_LIMIT);
        const long double temperature = start_temperature
            * pow(end_temperature / start_temperature, progress);
        const bool accept = difference <= 0
            || random.next_double() < exp(-static_cast<long double>(difference) / temperature);

        if (accept) {
            ++accepted;
            current_total += difference;
            day_cost[day_a] = new_cost_a;
            day_cost[day_b] = new_cost_b;
            day_edges[day_a][position_a] = edge_b;
            day_edges[day_b][position_b] = edge_a;
            if (current_total < best_total) {
                best_total = current_total;
                best_schedule = repair_day;
                best_day_cost = day_cost;
            }
        } else {
            repair_day[edge_a] = day_a;
            repair_day[edge_b] = day_b;
        }
    }

#ifdef AHC017_DEBUG
    cerr << "sample_objective=" << best_total
         << " iterations=" << iterations
         << " accepted=" << accepted
         << " elapsed=" << timer.seconds() << '\n';
#endif

    vector<int> final_count(D, 0);
    for (int day : best_schedule) ++final_count[day];
    for (int day = 0; day < D; ++day) {
        if (final_count[day] > K) {
            // This cannot happen because every initial day has floor/ceil(M/D)
            // edges and the search only swaps two edges.
            return 0;
        }
    }
    for (int edge_id = 0; edge_id < M; ++edge_id) {
        if (edge_id) cout << ' ';
        cout << best_schedule[edge_id] + 1;
    }
    cout << '\n';
}
