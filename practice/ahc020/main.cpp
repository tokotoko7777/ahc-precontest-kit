#include <bits/stdc++.h>
using namespace std;

struct Edge {
    int from;
    int to;
    int cost;
};

struct DSU {
    vector<int> parent;
    vector<int> size;

    explicit DSU(int n) : parent(n), size(n, 1) {
        iota(parent.begin(), parent.end(), 0);
    }

    int leader(int vertex) {
        if (parent[vertex] == vertex) return vertex;
        return parent[vertex] = leader(parent[vertex]);
    }

    bool unite(int a, int b) {
        a = leader(a);
        b = leader(b);
        if (a == b) return false;
        if (size[a] < size[b]) swap(a, b);
        parent[b] = a;
        size[a] += size[b];
        return true;
    }
};

struct Answer {
    vector<int> power;
    vector<int> use_edge;
    long long cost = (1LL << 62);
};

int ceil_square_root(long long value) {
    int result = static_cast<int>(sqrt(static_cast<long double>(value)));
    while (1LL * result * result < value) ++result;
    while (result > 0 && 1LL * (result - 1) * (result - 1) >= value) --result;
    return result;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int N, M, K;
    cin >> N >> M >> K;
    vector<int> station_x(N), station_y(N);
    for (int station = 0; station < N; ++station) {
        cin >> station_x[station] >> station_y[station];
    }

    vector<Edge> edges(M);
    const long long INF = (1LL << 60);
    vector<vector<long long>> shortest(N, vector<long long>(N, INF));
    vector<vector<int>> next_vertex(N, vector<int>(N, -1));
    vector<vector<int>> direct_edge(N, vector<int>(N, -1));
    for (int station = 0; station < N; ++station) {
        shortest[station][station] = 0;
        next_vertex[station][station] = station;
    }
    for (int id = 0; id < M; ++id) {
        int u, v, w;
        cin >> u >> v >> w;
        --u;
        --v;
        edges[id] = {u, v, w};
        if (w < shortest[u][v]) {
            shortest[u][v] = shortest[v][u] = w;
            next_vertex[u][v] = v;
            next_vertex[v][u] = u;
            direct_edge[u][v] = direct_edge[v][u] = id;
        }
    }

    vector<int> resident_x(K), resident_y(K);
    for (int resident = 0; resident < K; ++resident) {
        cin >> resident_x[resident] >> resident_y[resident];
    }

    for (int middle = 0; middle < N; ++middle) {
        for (int from = 0; from < N; ++from) {
            if (shortest[from][middle] == INF) continue;
            for (int to = 0; to < N; ++to) {
                const long long candidate = shortest[from][middle] + shortest[middle][to];
                if (candidate < shortest[from][to]) {
                    shortest[from][to] = candidate;
                    next_vertex[from][to] = next_vertex[from][middle];
                }
            }
        }
    }

    vector<vector<int>> required_power(N, vector<int>(K, 5001));
    vector<vector<pair<int, int>>> residents_by_distance(N);
    for (int station = 0; station < N; ++station) {
        residents_by_distance[station].reserve(K);
        for (int resident = 0; resident < K; ++resident) {
            const long long dx = station_x[station] - resident_x[resident];
            const long long dy = station_y[station] - resident_y[resident];
            const int power = ceil_square_root(dx * dx + dy * dy);
            required_power[station][resident] = power;
            if (power <= 5000) {
                residents_by_distance[station].push_back({power, resident});
            }
        }
        sort(residents_by_distance[station].begin(), residents_by_distance[station].end());
    }

    vector<int> edge_order(M);
    iota(edge_order.begin(), edge_order.end(), 0);
    sort(edge_order.begin(), edge_order.end(), [&](int left, int right) {
        return edges[left].cost < edges[right].cost;
    });

    auto make_network = [&](const vector<int>& power) {
        vector<char> terminal(N, false);
        terminal[0] = true;
        vector<int> terminals{0};
        for (int station = 1; station < N; ++station) {
            if (power[station] > 0) {
                terminal[station] = true;
                terminals.push_back(station);
            }
        }

        vector<int> metric_order;
        const int terminal_count = static_cast<int>(terminals.size());
        metric_order.reserve(terminal_count * (terminal_count - 1) / 2);
        for (int left = 0; left < terminal_count; ++left) {
            for (int right = left + 1; right < terminal_count; ++right) {
                metric_order.push_back(left * terminal_count + right);
            }
        }
        sort(metric_order.begin(), metric_order.end(), [&](int a, int b) {
            const int a_left = a / terminal_count;
            const int a_right = a % terminal_count;
            const int b_left = b / terminal_count;
            const int b_right = b % terminal_count;
            return shortest[terminals[a_left]][terminals[a_right]]
                 < shortest[terminals[b_left]][terminals[b_right]];
        });

        vector<char> candidate_edge(M, false);
        DSU terminal_dsu(terminal_count);
        for (int encoded : metric_order) {
            const int left = encoded / terminal_count;
            const int right = encoded % terminal_count;
            if (!terminal_dsu.unite(left, right)) continue;
            int current = terminals[left];
            const int goal = terminals[right];
            while (current != goal) {
                const int next = next_vertex[current][goal];
                const int id = direct_edge[current][next];
                candidate_edge[id] = true;
                current = next;
            }
        }

        // Remove cycles from the union of shortest paths.
        vector<int> metric_tree(M, 0);
        DSU graph_dsu(N);
        for (int id : edge_order) {
            if (candidate_edge[id] && graph_dsu.unite(edges[id].from, edges[id].to)) {
                metric_tree[id] = 1;
            }
        }

        auto prune_nonterminal_leaves = [&](vector<int> tree) {
            vector<vector<pair<int, int>>> tree_graph(N);
            vector<int> degree(N, 0);
            for (int id = 0; id < M; ++id) {
                if (!tree[id]) continue;
                const Edge& edge = edges[id];
                tree_graph[edge.from].push_back({edge.to, id});
                tree_graph[edge.to].push_back({edge.from, id});
                ++degree[edge.from];
                ++degree[edge.to];
            }
            queue<int> leaves;
            for (int station = 0; station < N; ++station) {
                if (!terminal[station] && degree[station] <= 1) leaves.push(station);
            }
            while (!leaves.empty()) {
                const int station = leaves.front();
                leaves.pop();
                if (terminal[station] || degree[station] != 1) continue;
                for (const auto& [next, id] : tree_graph[station]) {
                    if (!tree[id]) continue;
                    tree[id] = 0;
                    --degree[station];
                    --degree[next];
                    if (!terminal[next] && degree[next] == 1) leaves.push(next);
                    break;
                }
            }
            return tree;
        };
        metric_tree = prune_nonterminal_leaves(metric_tree);

        // A pruned MST of the original graph is a different cheap candidate.
        vector<int> full_tree(M, 0);
        DSU full_dsu(N);
        for (int id : edge_order) {
            if (full_dsu.unite(edges[id].from, edges[id].to)) full_tree[id] = 1;
        }
        full_tree = prune_nonterminal_leaves(full_tree);

        auto edge_cost = [&](const vector<int>& tree) {
            long long result = 0;
            for (int id = 0; id < M; ++id) {
                if (tree[id]) result += edges[id].cost;
            }
            return result;
        };
        if (edge_cost(full_tree) < edge_cost(metric_tree)) return full_tree;
        return metric_tree;
    };

    auto clean_redundant_power = [&](vector<int> power, vector<int> order) {
        vector<int> cover_count(K, 0);
        for (int station = 0; station < N; ++station) {
            if (power[station] == 0) continue;
            for (int resident = 0; resident < K; ++resident) {
                if (required_power[station][resident] <= power[station]) {
                    ++cover_count[resident];
                }
            }
        }
        for (int station : order) {
            int necessary_power = 0;
            for (int resident = 0; resident < K; ++resident) {
                if (required_power[station][resident] <= power[station]
                    && cover_count[resident] == 1) {
                    necessary_power = max(necessary_power, required_power[station][resident]);
                }
            }
            if (necessary_power >= power[station]) continue;
            const int old_power = power[station];
            power[station] = necessary_power;
            for (int resident = 0; resident < K; ++resident) {
                if (necessary_power < required_power[station][resident]
                    && required_power[station][resident] <= old_power) {
                    --cover_count[resident];
                }
            }
        }
        return power;
    };

    Answer best;
    int candidate_number = 0;
    auto evaluate_answer = [&](vector<int> power) {
        for (int resident = 0; resident < K; ++resident) {
            bool covered = false;
            for (int station = 0; station < N; ++station) {
                if (required_power[station][resident] <= power[station]) {
                    covered = true;
                    break;
                }
            }
            if (!covered) return;
        }
        vector<int> use_edge = make_network(power);
        long long cost = 0;
        for (int value : power) cost += 1LL * value * value;
        for (int id = 0; id < M; ++id) {
            if (use_edge[id]) cost += edges[id].cost;
        }
#ifdef AHC020_DEBUG
        cerr << "candidate=" << candidate_number << " cost=" << cost << '\n';
#endif
        ++candidate_number;
        if (cost < best.cost) best = {move(power), move(use_edge), cost};
    };

    auto consider = [&](const vector<int>& original_power) {
        vector<int> order(N);
        iota(order.begin(), order.end(), 0);

        sort(order.begin(), order.end(), [&](int left, int right) {
            return original_power[left] > original_power[right];
        });
        evaluate_answer(clean_redundant_power(original_power, order));

        reverse(order.begin(), order.end());
        evaluate_answer(clean_redundant_power(original_power, order));

        iota(order.begin(), order.end(), 0);
        evaluate_answer(clean_redundant_power(original_power, order));

        sort(order.begin(), order.end(), [&](int left, int right) {
            const uint64_t left_hash =
                (static_cast<uint64_t>(left + 1) * 0x9e3779b97f4a7c15ULL)
                ^ static_cast<uint64_t>(K);
            const uint64_t right_hash =
                (static_cast<uint64_t>(right + 1) * 0x9e3779b97f4a7c15ULL)
                ^ static_cast<uint64_t>(K);
            return left_hash < right_hash;
        });
        evaluate_answer(clean_redundant_power(original_power, order));
    };

    // Baseline: every resident uses its closest station.
    vector<int> nearest_power(N, 0);
    for (int resident = 0; resident < K; ++resident) {
        int best_station = -1;
        int best_required = 5001;
        for (int station = 0; station < N; ++station) {
            if (required_power[station][resident] < best_required) {
                best_required = required_power[station][resident];
                best_station = station;
            }
        }
        nearest_power[best_station] = max(nearest_power[best_station], best_required);
    }
    consider(nearest_power);

    // Weighted set cover.  Activating a station pays an approximate connection
    // cost; changing lambda gives both sparse and dense networks.
#ifndef AHC020_NEAREST_ONLY
    const array<long double, 8> connection_weight{
        0.0L, 0.03L, 0.07L, 0.15L, 0.30L, 0.60L, 1.0L, 2.0L};
    for (long double lambda : connection_weight) {
        vector<int> power(N, 0);
        vector<char> covered(K, false);
        int uncovered_count = K;

        while (uncovered_count > 0) {
            int best_station = -1;
            int best_power = -1;
            long double best_ratio = 1e100L;

            for (int station = 0; station < N; ++station) {
                int newly_covered = 0;
                int position = 0;
                const auto& sorted_residents = residents_by_distance[station];
                while (position < static_cast<int>(sorted_residents.size())) {
                    const int radius = sorted_residents[position].first;
                    int next_position = position;
                    while (next_position < static_cast<int>(sorted_residents.size())
                           && sorted_residents[next_position].first == radius) {
                        const int resident = sorted_residents[next_position].second;
                        if (radius > power[station] && !covered[resident]) {
                            ++newly_covered;
                        }
                        ++next_position;
                    }
                    position = next_position;
                    if (radius <= power[station] || newly_covered == 0) continue;
                    long double additional = 1.0L * radius * radius
                                           - 1.0L * power[station] * power[station];
                    if (power[station] == 0 && station != 0) {
                        additional += lambda * shortest[0][station];
                    }
                    const long double ratio = additional / newly_covered;
                    if (ratio < best_ratio) {
                        best_ratio = ratio;
                        best_station = station;
                        best_power = radius;
                    }
                }
            }

            if (best_station == -1) break;
            power[best_station] = best_power;
            for (int resident = 0; resident < K; ++resident) {
                if (!covered[resident]
                    && required_power[best_station][resident] <= best_power) {
                    covered[resident] = true;
                    --uncovered_count;
                }
            }
        }
        consider(power);
    }
#endif

#ifdef AHC020_DEBUG
    cerr << "best_cost=" << best.cost << '\n';
#endif

    for (int station = 0; station < N; ++station) {
        if (station) cout << ' ';
        cout << best.power[station];
    }
    cout << '\n';
    for (int id = 0; id < M; ++id) {
        if (id) cout << ' ';
        cout << best.use_edge[id];
    }
    cout << '\n';
}
