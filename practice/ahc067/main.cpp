#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3,unroll-loops")
#endif

#include <bits/stdc++.h>

using namespace std;

// Build nested door/switch dependencies, then measure every finalist with the
// exact BFS on (room, switch mask). This file is self-contained.

struct Edge {
    int u, v;
};

struct Candidate {
    int bits = 0;
    vector<int> switch_cell;
    vector<vector<int>> switch_candidates;
    vector<int> static_edges;
    vector<int> doors;
    int estimate = -1;
};

class Solver {
public:
    void solve() {
        cin >> n >> m >> k;
        board.resize(n);
        for (string& row : board) cin >> row;

#ifdef SIMPLE_BASELINE
        cout << "0\n0\n";
        return;
#endif

        build_graph();
        start_time = chrono::steady_clock::now();

        Candidate best;
        vector<Candidate> elite;
        initialize(best, elite);
        if (best.estimate < 0) {
            cout << "0\n0\n";
            return;
        }

        anneal(best, elite);
        best = select_exact_best(best, elite);
        print_answer(best);
    }

private:
    // Change only this value to scale all search phases proportionally.
    static constexpr double SEARCH_TIME_LIMIT_MS = 1800.0;
    static constexpr double BASE_TIME_LIMIT_MS = 1800.0;

    static constexpr double scaled_deadline(double base_deadline_ms) {
        return base_deadline_ms * SEARCH_TIME_LIMIT_MS / BASE_TIME_LIMIT_MS;
    }

    int n, m, k;
    vector<string> board;
    vector<int> cell_id;
    vector<pair<int, int>> position;
    vector<Edge> edges;
    vector<vector<pair<int, int>>> graph;
    vector<pair<int, int>> blocking_pairs;
    int start_vertex, goal_vertex;

    mt19937 rng;
    chrono::steady_clock::time_point start_time;

    mutable vector<int> bfs_seen, bfs_distance, bfs_queue;
    mutable int bfs_generation = 0;
    mutable vector<int> exact_seen, exact_distance, exact_queue;
    mutable int exact_generation = 0;
    mutable vector<int> exact_switch_seen, exact_switch_type;
    mutable int exact_switch_generation = 0;

    double elapsed_ms() const {
        return chrono::duration<double, milli>(chrono::steady_clock::now() - start_time).count();
    }

    int begin_search(vector<int>& seen, int& generation, int size) const {
        if (static_cast<int>(seen.size()) != size) {
            seen.assign(size, 0);
            generation = 0;
        }
        ++generation;
        if (generation == numeric_limits<int>::max()) {
            fill(seen.begin(), seen.end(), 0);
            generation = 1;
        }
        return generation;
    }

    int random_int(int upper) {
        return static_cast<int>(rng() % upper);
    }

    void build_graph() {
        cell_id.assign(n * n, -1);
        uint32_t seed = 2166136261u;
        for (int r = 0; r < n; ++r) {
            for (int c = 0; c < n; ++c) {
                seed = (seed ^ static_cast<unsigned char>(board[r][c])) * 16777619u;
                if (board[r][c] == '.') {
                    cell_id[r * n + c] = static_cast<int>(position.size());
                    position.push_back({r, c});
                }
            }
        }
        rng.seed(seed ^ 0x9e3779b9u);

        graph.resize(position.size());
        const int dr[2] = {1, 0};
        const int dc[2] = {0, 1};
        for (int u = 0; u < static_cast<int>(position.size()); ++u) {
            auto [r, c] = position[u];
            for (int d = 0; d < 2; ++d) {
                int nr = r + dr[d], nc = c + dc[d];
                if (nr >= n || nc >= n || board[nr][nc] == '#') continue;
                int v = cell_id[nr * n + nc];
                int eid = static_cast<int>(edges.size());
                edges.push_back({u, v});
                graph[u].push_back({v, eid});
                graph[v].push_back({u, eid});
            }
        }
        start_vertex = cell_id[0];
        goal_vertex = cell_id[n * n - 1];
        for (int v = 0; v < static_cast<int>(graph.size()); ++v) {
            if (v == start_vertex || v == goal_vertex || graph[v].size() != 2) continue;
            blocking_pairs.push_back({graph[v][0].second, graph[v][1].second});
        }
    }

    int distance_with_mask(int source, int target, int mask, const vector<int>& doors) const {
        int vertex_count = static_cast<int>(position.size());
        int generation = begin_search(bfs_seen, bfs_generation, vertex_count);
        bfs_distance.resize(vertex_count);
        bfs_queue.resize(vertex_count);
        int head = 0, tail = 0;
        bfs_seen[source] = generation;
        bfs_distance[source] = 0;
        bfs_queue[tail++] = source;
        while (head < tail) {
            int v = bfs_queue[head++];
            if (v == target) return bfs_distance[v];
            for (auto [to, eid] : graph[v]) {
                int g = doors[eid];
                if (g != -1 && ((g & 1) != ((mask >> (g / 2)) & 1))) continue;
                if (bfs_seen[to] == generation) continue;
                bfs_seen[to] = generation;
                bfs_distance[to] = bfs_distance[v] + 1;
                bfs_queue[tail++] = to;
            }
        }
        return -1;
    }

    int estimate(const Candidate& candidate) const {
        int bits = candidate.bits;
        int total = (1 << bits) - 1;
        int initial_distance = distance_with_mask(start_vertex, candidate.switch_cell[0], 0,
                                                  candidate.doors);
        if (initial_distance < 0) return -1;
        total += initial_distance;
        for (int type = 1; type < bits; ++type) {
            int mask = 1 << (type - 1);
            int d = distance_with_mask(candidate.switch_cell[0], candidate.switch_cell[type], mask,
                                       candidate.doors);
            if (d < 0) return -1;
            total += 2 * (1 << (bits - type - 1)) * d;
        }
        int goal_distance = distance_with_mask(candidate.switch_cell[0], goal_vertex,
                                               1 << (bits - 1),
                                               candidate.doors);
        if (goal_distance < 0) return -1;
        return total + goal_distance;
    }

    void add_elite(vector<Candidate>& elite, const Candidate& candidate) {
        elite.push_back(candidate);
        sort(elite.begin(), elite.end(),
             [](const Candidate& a, const Candidate& b) { return a.estimate > b.estimate; });
        if (elite.size() > 16) elite.resize(16);
    }

    vector<int> random_static_edges() {
        vector<int> order(blocking_pairs.size());
        iota(order.begin(), order.end(), 0);
        shuffle(order.begin(), order.end(), rng);
        vector<unsigned char> used(edges.size(), 0);
        vector<int> result;
        for (int index : order) {
            auto [a, b] = blocking_pairs[index];
            if (used[a] || used[b]) continue;
            used[a] = used[b] = 1;
            result.push_back(a);
            result.push_back(b);
            if (result.size() == 10) break;
        }
        return result;
    }

    bool build_from_static_edges(const vector<int>& static_edges, Candidate& candidate) {
        constexpr int BITS = 10;
        constexpr int SHARED_LEVELS = 2;
        constexpr int STATIC_DOORS = 10;
        if (static_edges.size() != STATIC_DOORS || k < BITS || m < 50) return false;

        vector<unsigned char> blocked(edges.size(), 0);
        for (int eid : static_edges) {
            if (eid < 0 || eid >= static_cast<int>(edges.size()) || blocked[eid]) return false;
            blocked[eid] = 1;
        }

        int vertex_count = static_cast<int>(position.size());
        vector<int> order(vertex_count, -1), low(vertex_count, -1);
        vector<unsigned char> local_bridge(edges.size(), 0);
        int timer = 0;
        function<void(int, int)> dfs = [&](int v, int parent_edge) {
            order[v] = low[v] = timer++;
            for (auto [to, eid] : graph[v]) {
                if (blocked[eid] || eid == parent_edge) continue;
                if (order[to] != -1) {
                    low[v] = min(low[v], order[to]);
                    continue;
                }
                dfs(to, eid);
                low[v] = min(low[v], low[to]);
                if (low[to] > order[v]) local_bridge[eid] = 1;
            }
        };
        for (int v = 0; v < vertex_count; ++v) {
            if (order[v] == -1) dfs(v, -1);
        }

        vector<int> local_component(vertex_count, -1);
        vector<vector<int>> local_members;
        for (int root = 0; root < vertex_count; ++root) {
            if (local_component[root] != -1) continue;
            int cid = static_cast<int>(local_members.size());
            local_members.push_back({});
            queue<int> que;
            que.push(root);
            local_component[root] = cid;
            while (!que.empty()) {
                int v = que.front();
                que.pop();
                local_members[cid].push_back(v);
                for (auto [to, eid] : graph[v]) {
                    if (blocked[eid] || local_bridge[eid] || local_component[to] != -1) continue;
                    local_component[to] = cid;
                    que.push(to);
                }
            }
        }

        int component_count = static_cast<int>(local_members.size());
        vector<vector<pair<int, int>>> tree(component_count);
        for (int eid = 0; eid < static_cast<int>(edges.size()); ++eid) {
            if (!local_bridge[eid]) continue;
            int a = local_component[edges[eid].u];
            int b = local_component[edges[eid].v];
            tree[a].push_back({b, eid});
            tree[b].push_back({a, eid});
        }

        int root = local_component[start_vertex];
        int goal = local_component[goal_vertex];
        vector<int> parent(component_count, -1), parent_edge(component_count, -1);
        vector<int> depth(component_count, -1);
        queue<int> que;
        que.push(root);
        depth[root] = 0;
        while (!que.empty()) {
            int v = que.front();
            que.pop();
            for (auto [to, eid] : tree[v]) {
                if (depth[to] != -1) continue;
                depth[to] = depth[v] + 1;
                parent[to] = v;
                parent_edge[to] = eid;
                que.push(to);
            }
        }
        constexpr int GOAL_SUFFIX = BITS - SHARED_LEVELS;
        if (depth[goal] < BITS) return false;

        auto local_path = [&](int terminal, int length) {
            vector<int> path(length + 1, -1);
            int v = terminal;
            for (int i = length; i >= 0; --i) {
                if (v == -1) return vector<int>{};
                path[i] = v;
                v = parent[v];
            }
            return path;
        };

        vector<int> route;
        for (int v = goal; v != -1; v = parent[v]) route.push_back(v);
        reverse(route.begin(), route.end());
        int route_length = static_cast<int>(route.size()) - 1;
        vector<int> route_edge(route.size(), -1);
        for (int i = 1; i < static_cast<int>(route.size()); ++i) {
            route_edge[i] = parent_edge[route[i]];
        }
        vector<vector<int>> side_children(route.size());
        for (int i = 0; i < static_cast<int>(route.size()); ++i) {
            int next = i + 1 < static_cast<int>(route.size()) ? route[i + 1] : -1;
            for (auto [to, eid] : tree[route[i]]) {
                if (parent[to] == route[i] && to != next) side_children[i].push_back(to);
            }
        }

        struct PrefixChoice {
            int d0_index, d2_index;
            vector<int> s1_children, s2_children;
        };
        vector<PrefixChoice> prefix_choices;
        int suffix_start = route_length - GOAL_SUFFIX;
        for (int d0_index = 1; d0_index < suffix_start; ++d0_index) {
            vector<int> s1_children;
            for (int i = 0; i < d0_index; ++i) {
                s1_children.insert(s1_children.end(), side_children[i].begin(),
                                   side_children[i].end());
            }
            if (s1_children.empty()) continue;
            for (int d2_index = d0_index + 1; d2_index <= suffix_start; ++d2_index) {
                vector<int> s2_children;
                for (int i = d0_index; i < d2_index; ++i) {
                    s2_children.insert(s2_children.end(), side_children[i].begin(),
                                       side_children[i].end());
                }
                if (!s2_children.empty()) {
                    prefix_choices.push_back(
                        {d0_index, d2_index, s1_children, s2_children});
                }
            }
        }
        if (prefix_choices.empty()) return false;
        PrefixChoice prefix =
            prefix_choices[random_int(static_cast<int>(prefix_choices.size()))];
        int s1_child = prefix.s1_children[
            random_int(static_cast<int>(prefix.s1_children.size()))];
        int s2_child = prefix.s2_children[
            random_int(static_cast<int>(prefix.s2_children.size()))];
        int higher_root = route[prefix.d2_index];

        vector<int> goal_path = local_path(goal, GOAL_SUFFIX);
        vector<unsigned char> reserved(component_count, 0), protected_core(component_count, 0);
        for (int i = 1; i <= GOAL_SUFFIX; ++i) reserved[goal_path[i]] = 1;
        auto protect = [&](int v) {
            while (true) {
                protected_core[v] = 1;
                if (v == higher_root) return true;
                v = parent[v];
                if (v == -1) return false;
            }
        };
        if (!protect(goal_path[0])) return false;

        vector<vector<int>> paths(BITS);
        vector<int> terminals(BITS, root);
        terminals[1] = s1_child;
        terminals[2] = s2_child;
        for (int type = BITS - 1; type > SHARED_LEVELS; --type) {
            int length = type - SHARED_LEVELS;
            vector<int> choices;
            for (int terminal = 0; terminal < component_count; ++terminal) {
                if (depth[terminal] - depth[higher_root] < length) continue;
                vector<int> path = local_path(terminal, length);
                int ancestor = path.empty() ? -1 : path[0];
                while (ancestor != -1 && ancestor != higher_root) ancestor = parent[ancestor];
                bool inside = ancestor == higher_root;
                bool valid = inside && !reserved[path[0]];
                for (int i = 1; valid && i <= length; ++i) {
                    valid = !reserved[path[i]] && !protected_core[path[i]];
                }
                for (int v = path.empty() ? -1 : path[0]; valid; v = parent[v]) {
                    if (reserved[v]) valid = false;
                    if (v == higher_root) break;
                }
                if (valid) choices.push_back(terminal);
            }
            if (choices.empty()) return false;
            int terminal = choices[random_int(static_cast<int>(choices.size()))];
            terminals[type] = terminal;
            paths[type] = local_path(terminal, length);
            for (int i = 1; i <= length; ++i) reserved[paths[type][i]] = 1;
            if (!protect(paths[type][0])) return false;
        }

        candidate = Candidate{};
        candidate.bits = BITS;
        candidate.static_edges = static_edges;
        candidate.doors.assign(edges.size(), -1);
        for (int i = 0; i < STATIC_DOORS / 2; ++i) {
            candidate.doors[static_edges[2 * i]] = 18;
            candidate.doors[static_edges[2 * i + 1]] = 19;
        }
        candidate.doors[route_edge[prefix.d0_index]] = 0;
        candidate.doors[parent_edge[s1_child]] = 1;
        candidate.doors[route_edge[prefix.d2_index]] = 2;
        candidate.doors[parent_edge[s2_child]] = 3;

        for (int type = SHARED_LEVELS + 1; type < BITS; ++type) {
            int length = type - SHARED_LEVELS;
            for (int step = 0; step < length; ++step) {
                int child = paths[type][step + 1];
                int eid = parent_edge[child];
                int g = (step + 1 == length ? 2 * type - 1
                                             : 2 * (SHARED_LEVELS + step));
                if (eid == -1 || candidate.doors[eid] != -1) return false;
                candidate.doors[eid] = g;
            }
        }
        for (int step = 0; step < GOAL_SUFFIX; ++step) {
            int child = goal_path[step + 1];
            int eid = parent_edge[child];
            int g = (step + 1 == GOAL_SUFFIX ? 19
                                              : 2 * (SHARED_LEVELS + step));
            if (eid == -1 || candidate.doors[eid] != -1) return false;
            candidate.doors[eid] = g;
        }

        vector<int> region(component_count, -1), region_queue(component_count);
        vector<vector<int>> region_cells;
        for (int seed = 0; seed < component_count; ++seed) {
            if (region[seed] != -1) continue;
            int rid = static_cast<int>(region_cells.size());
            region_cells.push_back({});
            int head = 0, tail = 0;
            region[seed] = rid;
            region_queue[tail++] = seed;
            while (head < tail) {
                int v = region_queue[head++];
                region_cells[rid].insert(region_cells[rid].end(), local_members[v].begin(),
                                         local_members[v].end());
                for (auto [to, eid] : tree[v]) {
                    if (candidate.doors[eid] != -1 || region[to] != -1) continue;
                    region[to] = rid;
                    region_queue[tail++] = to;
                }
            }
        }
        candidate.switch_cell.assign(BITS, start_vertex);
        candidate.switch_candidates.resize(BITS);
        candidate.switch_candidates[0] = region_cells[region[root]];
        for (int type = 1; type < BITS; ++type) {
            candidate.switch_candidates[type] = region_cells[region[terminals[type]]];
        }
        for (int type = 0; type < BITS; ++type) {
            const vector<int>& cells = candidate.switch_candidates[type];
            if (cells.empty()) return false;
            candidate.switch_cell[type] =
                cells[random_int(static_cast<int>(cells.size()))];
        }

        candidate.estimate = estimate(candidate);
        return candidate.estimate >= 0;
    }

    void initialize(Candidate& best, vector<Candidate>& elite) {
        if (k < 10 || blocking_pairs.size() < 5) return;
        while (elapsed_ms() < scaled_deadline(750.0) ||
               (best.estimate < 0 && elapsed_ms() < scaled_deadline(1725.0))) {
            Candidate candidate;
            if (!build_from_static_edges(random_static_edges(), candidate)) continue;
            if (candidate.estimate > best.estimate) best = candidate;
            add_elite(elite, candidate);
        }
    }

    void anneal(Candidate& best, vector<Candidate>& elite) {
        Candidate current = best;
        int current_score = current.estimate;
        const double anneal_deadline = scaled_deadline(1450.0);
        while (elapsed_ms() < anneal_deadline) {
            Candidate candidate;
            int operation = random_int(100);
            int moved_switch = -1;
            if (operation < 25) {
                moved_switch = 0;
            } else if (operation < 60) {
                moved_switch = 1 + random_int(current.bits - 1);
            }
            if (moved_switch != -1 &&
                current.switch_candidates[moved_switch].size() >= 2) {
                candidate = current;
                const vector<int>& cells = candidate.switch_candidates[moved_switch];
                int index = random_int(static_cast<int>(cells.size()));
                if (cells[index] == candidate.switch_cell[moved_switch]) {
                    index = (index + 1) % static_cast<int>(cells.size());
                }
                candidate.switch_cell[moved_switch] = cells[index];
                candidate.estimate = estimate(candidate);
            } else {
                vector<int> static_edges;
                if (random_int(100) < 12) {
                    static_edges = random_static_edges();
                } else {
                    static_edges = current.static_edges;
                    if (blocking_pairs.empty()) continue;
                    int pair_index = random_int(5);
                    auto [a, b] = blocking_pairs[
                        random_int(static_cast<int>(blocking_pairs.size()))];
                    static_edges[2 * pair_index] = a;
                    static_edges[2 * pair_index + 1] = b;
                }
                if (!build_from_static_edges(static_edges, candidate)) continue;
            }
            if (candidate.estimate < 0) continue;

            double progress = min(1.0, elapsed_ms() / anneal_deadline);
            double temperature = 300.0 * pow(0.2 / 300.0, progress);
            bool accept = candidate.estimate >= current_score;
            if (!accept) {
                double probability = exp(static_cast<double>(candidate.estimate - current_score) /
                                         temperature);
                accept = generate_canonical<double, 32>(rng) < probability;
            }
            if (!accept) continue;
            current = move(candidate);
            current_score = current.estimate;
            if (current_score > best.estimate) {
                best = current;
                add_elite(elite, best);
            }
        }
    }

    int exact_score(const Candidate& candidate) const {
        int bits = candidate.bits;
        int vertex_count = static_cast<int>(position.size());
        int state_count = (1 << bits) * vertex_count;
        int switch_generation =
            begin_search(exact_switch_seen, exact_switch_generation, vertex_count);
        exact_switch_type.resize(vertex_count);
        for (int type = 0; type < bits; ++type) {
            int v = candidate.switch_cell[type];
            exact_switch_seen[v] = switch_generation;
            exact_switch_type[v] = type;
        }

        int generation = begin_search(exact_seen, exact_generation, state_count);
        exact_distance.resize(state_count);
        exact_queue.resize(state_count);
        int head = 0, tail = 0;
        exact_seen[start_vertex] = generation;
        exact_distance[start_vertex] = 0;
        exact_queue[tail++] = start_vertex;
        while (head < tail) {
            int state = exact_queue[head++];
            int mask = state / vertex_count;
            int v = state % vertex_count;
            if (v == goal_vertex) return exact_distance[state];
            for (auto [to, eid] : graph[v]) {
                int g = candidate.doors[eid];
                if (g != -1 && ((g & 1) != ((mask >> (g / 2)) & 1))) continue;
                int next = mask * vertex_count + to;
                if (exact_seen[next] == generation) continue;
                exact_seen[next] = generation;
                exact_distance[next] = exact_distance[state] + 1;
                exact_queue[tail++] = next;
            }
            int type = exact_switch_seen[v] == switch_generation ? exact_switch_type[v] : -1;
            if (type != -1) {
                int next = (mask ^ (1 << type)) * vertex_count + v;
                if (exact_seen[next] != generation) {
                    exact_seen[next] = generation;
                    exact_distance[next] = exact_distance[state] + 1;
                    exact_queue[tail++] = next;
                }
            }
        }
        return -1;
    }

    Candidate select_exact_best(const Candidate& fallback, const vector<Candidate>& elite) const {
        Candidate best = fallback;
        int best_score = exact_score(best);
        for (const Candidate& candidate : elite) {
            int score = exact_score(candidate);
            if (score > best_score) {
                best_score = score;
                best = candidate;
            }
        }
        return best;
    }

    int door_count(const Candidate& candidate) const {
        int result = 0;
        for (int g : candidate.doors) result += (g != -1);
        return result;
    }

    void print_answer(const Candidate& candidate) const {
        cout << door_count(candidate) << '\n';
        for (int eid = 0; eid < static_cast<int>(edges.size()); ++eid) {
            int g = candidate.doors[eid];
            if (g == -1) continue;
            auto [r1, c1] = position[edges[eid].u];
            auto [r2, c2] = position[edges[eid].v];
            if (r1 != r2) {
                cout << 0 << ' ' << min(r1, r2) << ' ' << c1 << ' ' << g << '\n';
            } else {
                cout << 1 << ' ' << r1 << ' ' << min(c1, c2) << ' ' << g << '\n';
            }
        }
        cout << candidate.bits << '\n';
        for (int type = 0; type < candidate.bits; ++type) {
            auto [r, c] = position[candidate.switch_cell[type]];
            cout << r << ' ' << c << ' ' << type << '\n';
        }
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Solver solver;
    solver.solve();
    return 0;
}
