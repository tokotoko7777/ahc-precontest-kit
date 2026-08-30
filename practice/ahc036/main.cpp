#include <algorithm>
#include <bitset>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <random>
#include <tuple>
#include <utility>
#include <vector>
using namespace std;

struct Grouping {
    vector<vector<int>> groups;
    vector<int> group_of;
    long long estimated_signals = (1LL << 60);
};

long long hilbert_index(int x, int y) {
    long long answer = 0;
    for (int side = 512; side > 0; side >>= 1) {
        int right = (x & side) != 0;
        int top = (y & side) != 0;
        answer += 1LL * side * side * ((3 * right) ^ top);
        if (top == 0) {
            if (right == 1) {
                x = 1023 - x;
                y = 1023 - y;
            }
            swap(x, y);
        }
    }
    return answer;
}

// Grow compact connected regions.  The supplied order decides both the next
// seed and which nearby frontier vertex is preferred.
Grouping make_growing_groups(const vector<vector<int>>& graph,
                             const vector<int>& order, int limit) {
    const int n = (int)graph.size();
    vector<int> rank(n);
    for (int i = 0; i < n; ++i) rank[order[i]] = i;
    vector<char> used(n, false);
    vector<int> seen(n, -1);
    vector<vector<int>> groups;
    int stamp = 0;

    for (int seed : order) {
        if (used[seed]) continue;
        vector<int> group;
        priority_queue<pair<int, int>, vector<pair<int, int>>,
                       greater<pair<int, int>>>
            frontier;
        frontier.push({0, seed});
        seen[seed] = stamp;
        while (!frontier.empty() && (int)group.size() < limit) {
            int v = frontier.top().second;
            frontier.pop();
            if (used[v]) continue;
            used[v] = true;
            group.push_back(v);
            for (int to : graph[v]) {
                if (used[to] || seen[to] == stamp) continue;
                seen[to] = stamp;
                frontier.push({abs(rank[to] - rank[seed]), to});
            }
        }
        groups.push_back(move(group));
        ++stamp;
    }

    Grouping result;
    result.groups = move(groups);
    result.group_of.assign(n, -1);
    for (int g = 0; g < (int)result.groups.size(); ++g) {
        for (int v : result.groups[g]) result.group_of[v] = g;
    }
    return result;
}

// Make connected groups from a BFS tree.  Every unfinished child part is
// connected, so merging child parts through their parent keeps it connected.
Grouping make_tree_groups(const vector<vector<int>>& graph, int root, int limit,
                          int order_mode, uint32_t random_seed) {
    const int n = (int)graph.size();
    vector<int> parent(n, -1), bfs_order;
    queue<int> que;
    parent[root] = root;
    que.push(root);

    mt19937 rng(random_seed);
    while (!que.empty()) {
        int v = que.front();
        que.pop();
        bfs_order.push_back(v);

        vector<int> next = graph[v];
        if (order_mode == 1) reverse(next.begin(), next.end());
        if (order_mode >= 2) shuffle(next.begin(), next.end(), rng);
        for (int to : next) {
            if (parent[to] != -1) continue;
            parent[to] = v;
            que.push(to);
        }
    }

    vector<vector<int>> children(n);
    for (int v = 0; v < n; ++v) {
        if (v != root) children[parent[v]].push_back(v);
    }

    vector<vector<int>> pending(n);
    vector<vector<int>> groups;
    for (int oi = n - 1; oi >= 0; --oi) {
        int v = bfs_order[oi];
        pending[v].push_back(v);

        vector<int> cs = children[v];
        if (order_mode == 0) {
            sort(cs.begin(), cs.end(), [&](int a, int b) {
                return pending[a].size() < pending[b].size();
            });
        } else if (order_mode == 1) {
            sort(cs.begin(), cs.end(), [&](int a, int b) {
                return pending[a].size() > pending[b].size();
            });
        } else {
            shuffle(cs.begin(), cs.end(), rng);
        }

        for (int c : cs) {
            if ((int)pending[v].size() + (int)pending[c].size() <= limit) {
                pending[v].insert(pending[v].end(), pending[c].begin(),
                                  pending[c].end());
            } else {
                groups.push_back(move(pending[c]));
            }
        }
    }
    groups.push_back(move(pending[root]));

    Grouping result;
    result.groups = move(groups);
    result.group_of.assign(n, -1);
    for (int g = 0; g < (int)result.groups.size(); ++g) {
        for (int v : result.groups[g]) result.group_of[v] = g;
    }
    return result;
}

// The score estimate is the number of group borders on shortest paths in the
// contracted graph.  It is fast enough to compare many different groupings.
long long estimate(Grouping& grouping, const vector<pair<int, int>>& edges,
                   const vector<int>& targets) {
    const int k = (int)grouping.groups.size();
    vector<vector<int>> group_graph(k);
    for (auto [u, v] : edges) {
        int a = grouping.group_of[u];
        int b = grouping.group_of[v];
        if (a == b) continue;
        group_graph[a].push_back(b);
        group_graph[b].push_back(a);
    }
    for (auto& next : group_graph) {
        sort(next.begin(), next.end());
        next.erase(unique(next.begin(), next.end()), next.end());
    }

    vector<vector<int>> dist(k, vector<int>(k, 1'000'000));
    for (int s = 0; s < k; ++s) {
        queue<int> que;
        dist[s][s] = 0;
        que.push(s);
        while (!que.empty()) {
            int v = que.front();
            que.pop();
            for (int to : group_graph[v]) {
                if (dist[s][to] != 1'000'000) continue;
                dist[s][to] = dist[s][v] + 1;
                que.push(to);
            }
        }
    }

    long long score = 0;
    int current = grouping.group_of[0];
    for (int target : targets) {
        int next = grouping.group_of[target];
        score += dist[current][next];
        current = next;
    }
    grouping.estimated_signals = score;
    return score;
}

// Find a path that first minimizes signal changes and then minimizes moves.
vector<int> low_signal_path(int start, int goal,
                            const vector<vector<int>>& graph,
                            const vector<int>& group_of) {
    const int n = (int)graph.size();
    const long long SIGNAL_COST = 1'000'000;
    const long long INF = (1LL << 60);
    vector<long long> dist(n, INF);
    vector<int> parent(n, -1);
    priority_queue<pair<long long, int>, vector<pair<long long, int>>,
                   greater<pair<long long, int>>>
        que;
    dist[start] = 0;
    que.push({0, start});

    while (!que.empty()) {
        auto [d, v] = que.top();
        que.pop();
        if (d != dist[v]) continue;
        if (v == goal) break;
        for (int to : graph[v]) {
            long long nd = d + 1;
            if (group_of[v] != group_of[to]) nd += SIGNAL_COST;
            if (nd >= dist[to]) continue;
            dist[to] = nd;
            parent[to] = v;
            que.push({nd, to});
        }
    }

    vector<int> path;
    for (int v = goal; v != start; v = parent[v]) path.push_back(v);
    reverse(path.begin(), path.end());
    return path;
}

vector<int> shortest_path(int start, int goal,
                          const vector<vector<int>>& graph) {
    const int n = (int)graph.size();
    vector<int> parent(n, -1);
    queue<int> que;
    parent[start] = start;
    que.push(start);
    while (!que.empty() && parent[goal] == -1) {
        int v = que.front();
        que.pop();
        for (int to : graph[v]) {
            if (parent[to] != -1) continue;
            parent[to] = v;
            que.push(to);
        }
    }
    vector<int> path;
    for (int v = goal; v != start; v = parent[v]) path.push_back(v);
    reverse(path.begin(), path.end());
    return path;
}

vector<int> make_a(const Grouping& grouping, const vector<int>& group_order,
                   int length_a) {
    vector<int> a;
    for (int g : group_order) {
        a.insert(a.end(), grouping.groups[g].begin(),
                 grouping.groups[g].end());
    }
    while ((int)a.size() < length_a) a.push_back(0);
    return a;
}

struct WindowPlan {
    int signal_count = numeric_limits<int>::max();
    vector<int> chosen_window;
};

// Whenever the next city is not green, choose the A window that keeps the
// longest following prefix green.  The order inside B is irrelevant: only
// membership matters.
WindowPlan plan_windows(const vector<int>& a, const vector<int>& route,
                        int length_b) {
    const int window_count = (int)a.size() - length_b + 1;
    vector<bitset<600>> member(window_count);
    vector<vector<int>> containing(600);
    for (int p = 0; p < window_count; ++p) {
        for (int j = 0; j < length_b; ++j) member[p].set(a[p + j]);
        for (int v = 0; v < 600; ++v) {
            if (member[p].test(v)) containing[v].push_back(p);
        }
    }

    WindowPlan plan;
    plan.signal_count = 0;
    plan.chosen_window.assign(route.size(), -1);
    bitset<600> active;
    for (int i = 0; i < (int)route.size(); ++i) {
        int next = route[i];
        if (active.test(next)) continue;

        int best_p = containing[next][0];
        int best_end = i;
        for (int p : containing[next]) {
            int end = i;
            while (end < (int)route.size() && member[p].test(route[end])) ++end;
            if (end > best_end) {
                best_end = end;
                best_p = p;
            }
        }
        active = member[best_p];
        plan.chosen_window[i] = best_p;
        ++plan.signal_count;
    }
    return plan;
}

// Use the L_A-N spare cells for route fragments that currently need several
// signals.  Equal vertex sets are combined, so recurring bottlenecks are
// preferred over one-off fragments.
vector<int> add_route_windows(vector<int> a, const vector<int>& route,
                              const WindowPlan& current_plan, int used_length,
                              int length_b) {
    int whole_bags = ((int)a.size() - used_length) / length_b;
    if (whole_bags == 0 || route.empty()) return a;

    vector<int> signal_prefix(route.size() + 1);
    for (int i = 0; i < (int)route.size(); ++i) {
        signal_prefix[i + 1] =
            signal_prefix[i] + (current_plan.chosen_window[i] != -1);
    }

    struct BagCandidate {
        vector<int> vertices;
        int saved_signals = 0;
        int occurrences = 0;
    };
    vector<BagCandidate> candidates;
    map<vector<int>, int> candidate_id;
    for (int begin = 0; begin < (int)route.size(); ++begin) {
        if (current_plan.chosen_window[begin] == -1) continue;
        bitset<600> member;
        vector<int> vertices;
        int end = begin;
        while (end < (int)route.size()) {
            int v = route[end];
            if (!member.test(v) && (int)vertices.size() == length_b) break;
            if (!member.test(v)) {
                member.set(v);
                vertices.push_back(v);
            }
            ++end;
        }
        while (end < (int)route.size() && member.test(route[end])) ++end;

        vector<int> key = vertices;
        sort(key.begin(), key.end());
        int id;
        auto found = candidate_id.find(key);
        if (found == candidate_id.end()) {
            id = (int)candidates.size();
            candidate_id[key] = id;
            candidates.push_back({vertices, 0, 0});
        } else {
            id = found->second;
        }
        int old_signals = signal_prefix[end] - signal_prefix[begin];
        candidates[id].saved_signals += max(0, old_signals - 1);
        ++candidates[id].occurrences;
    }

    sort(candidates.begin(), candidates.end(), [](const BagCandidate& x,
                                                   const BagCandidate& y) {
        return make_tuple(x.saved_signals, x.occurrences, x.vertices.size()) >
               make_tuple(y.saved_signals, y.occurrences, y.vertices.size());
    });
    whole_bags = min(whole_bags, (int)candidates.size());
    int write = used_length;
    for (int i = 0; i < whole_bags; ++i) {
        const vector<int>& vertices = candidates[i].vertices;
        for (int j = 0; j < length_b; ++j) {
            a[write++] = vertices[min(j, (int)vertices.size() - 1)];
        }
    }
    return a;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, m, t, length_a, length_b;
    cin >> n >> m >> t >> length_a >> length_b;
    vector<vector<int>> graph(n);
    vector<pair<int, int>> edges(m);
    for (auto& [u, v] : edges) {
        cin >> u >> v;
        graph[u].push_back(v);
        graph[v].push_back(u);
    }
    vector<int> targets(t);
    for (int& v : targets) cin >> v;
    vector<pair<int, int>> coordinate(n);
    for (auto& [x, y] : coordinate) cin >> x >> y;

#ifdef SIMPLE_BASELINE
    vector<int> baseline_a(length_a, 0);
    iota(baseline_a.begin(), baseline_a.begin() + n, 0);
    for (int i = 0; i < length_a; ++i) {
        if (i) cout << ' ';
        cout << baseline_a[i];
    }
    cout << '\n';
    int baseline_current = 0;
    for (int target : targets) {
        vector<int> path = shortest_path(baseline_current, target, graph);
        for (int next : path) {
            cout << "s 1 " << next << " 0\n";
            cout << "m " << next << '\n';
        }
        baseline_current = target;
    }
    return 0;
#endif

    // Different roots and child orders produce different connected regions.
    // Keep the one whose contracted graph best matches the target sequence.
    vector<int> roots = {0, n - 1};
    for (int i = 0; i < t; i += max(1, t / 18)) roots.push_back(targets[i]);
    for (int i = 0; i < n; i += max(1, n / 18)) roots.push_back(i);
    sort(roots.begin(), roots.end());
    roots.erase(unique(roots.begin(), roots.end()), roots.end());

    Grouping best;
    int trial = 0;
    for (int root : roots) {
        for (int mode = 0; mode < 5; ++mode) {
            Grouping candidate =
                make_tree_groups(graph, root, length_b, mode,
                                 123456789u + 1009u * trial++);
            estimate(candidate, edges, targets);
            if (make_pair(candidate.estimated_signals,
                          candidate.groups.size()) <
                make_pair(best.estimated_signals, best.groups.size())) {
                best = move(candidate);
            }
        }
    }

    // Hilbert and stripe orders give the region grower several spatial views.
    // The graph test inside make_growing_groups still guarantees connectivity.
    for (int transform = 0; transform < 12; ++transform) {
        vector<int> order(n);
        iota(order.begin(), order.end(), 0);
        sort(order.begin(), order.end(), [&](int a, int b) {
            auto transformed_key = [&](int v) {
                int x = coordinate[v].first;
                int y = coordinate[v].second;
                if (transform & 1) swap(x, y);
                if (transform & 2) x = 1000 - x;
                if (transform & 4) y = 1000 - y;
                if (transform < 8) return hilbert_index(x, y);
                if (transform == 8) return 2000LL * x + y;
                if (transform == 9) return 2000LL * y + x;
                if (transform == 10) return 2000LL * (x + y) + x;
                return 2000LL * (x - y + 1000) + x;
            };
            return make_pair(transformed_key(a), a) <
                   make_pair(transformed_key(b), b);
        });
        Grouping candidate = make_growing_groups(graph, order, length_b);
        estimate(candidate, edges, targets);
        if (make_pair(candidate.estimated_signals, candidate.groups.size()) <
            make_pair(best.estimated_signals, best.groups.size())) {
            best = move(candidate);
        }
        reverse(order.begin(), order.end());
        candidate = make_growing_groups(graph, order, length_b);
        estimate(candidate, edges, targets);
        if (make_pair(candidate.estimated_signals, candidate.groups.size()) <
            make_pair(best.estimated_signals, best.groups.size())) {
            best = move(candidate);
        }
    }

    // Prepare both a signal-first route and a shortest-move route.  The actual
    // A windows decide which one is better for this input.
    vector<int> low_signal_route, shortest_route;
    int current = 0;
    for (int target : targets) {
        vector<int> path =
            low_signal_path(current, target, graph, best.group_of);
        low_signal_route.insert(low_signal_route.end(), path.begin(), path.end());
        path = shortest_path(current, target, graph);
        shortest_route.insert(shortest_route.end(), path.begin(), path.end());
        current = target;
    }

    const int k = (int)best.groups.size();
    vector<vector<int>> transition_weight(k, vector<int>(k));
    int previous = 0;
    for (int v : low_signal_route) {
        int a_group = best.group_of[previous];
        int b_group = best.group_of[v];
        if (a_group != b_group) {
            ++transition_weight[a_group][b_group];
            ++transition_weight[b_group][a_group];
        }
        previous = v;
    }

    vector<vector<int>> group_orders;
    vector<int> original_order(k);
    iota(original_order.begin(), original_order.end(), 0);
    group_orders.push_back(original_order);
    reverse(original_order.begin(), original_order.end());
    group_orders.push_back(original_order);

    // Insert groups one by one where frequently crossed borders become
    // neighbours in A.  Such neighbours can share one length-L_B window.
    int first = 0, second = min(1, k - 1);
    for (int a_group = 0; a_group < k; ++a_group) {
        for (int b_group = a_group + 1; b_group < k; ++b_group) {
            if (transition_weight[a_group][b_group] >
                transition_weight[first][second]) {
                first = a_group;
                second = b_group;
            }
        }
    }
    vector<int> inserted = {first};
    vector<char> placed(k, false);
    placed[first] = true;
    if (second != first) {
        inserted.push_back(second);
        placed[second] = true;
    }
    while ((int)inserted.size() < k) {
        int best_group = -1, best_position = 0;
        int best_gain = numeric_limits<int>::min();
        for (int g = 0; g < k; ++g) {
            if (placed[g]) continue;
            for (int p = 0; p <= (int)inserted.size(); ++p) {
                int gain = 0;
                if (p > 0) gain += transition_weight[inserted[p - 1]][g];
                if (p < (int)inserted.size())
                    gain += transition_weight[g][inserted[p]];
                if (p > 0 && p < (int)inserted.size())
                    gain -= transition_weight[inserted[p - 1]][inserted[p]];
                if (gain > best_gain) {
                    best_gain = gain;
                    best_group = g;
                    best_position = p;
                }
            }
        }
        inserted.insert(inserted.begin() + best_position, best_group);
        placed[best_group] = true;
    }
    group_orders.push_back(inserted);
    reverse(inserted.begin(), inserted.end());
    group_orders.push_back(inserted);

    vector<int> best_a, best_route;
    WindowPlan best_plan;
    for (const vector<int>& order : group_orders) {
        vector<int> candidate_a = make_a(best, order, length_a);
        for (const vector<int>* route : {&low_signal_route, &shortest_route}) {
            WindowPlan plan = plan_windows(candidate_a, *route, length_b);
            if (make_pair(plan.signal_count, route->size()) <
                make_pair(best_plan.signal_count, best_route.size())) {
                best_plan = plan;
                best_a = candidate_a;
                best_route = *route;
            }

            vector<int> enhanced_a = add_route_windows(
                candidate_a, *route, plan, n, length_b);
            WindowPlan enhanced_plan =
                plan_windows(enhanced_a, *route, length_b);
            if (make_pair(enhanced_plan.signal_count, route->size()) <
                make_pair(best_plan.signal_count, best_route.size())) {
                best_plan = move(enhanced_plan);
                best_a = move(enhanced_a);
                best_route = *route;
            }
        }
    }

#ifdef LOCAL
    int smallest_group = n;
    int largest_group = 0;
    for (const auto& group : best.groups) {
        smallest_group = min(smallest_group, (int)group.size());
        largest_group = max(largest_group, (int)group.size());
    }
    cerr << "groups=" << best.groups.size()
         << " estimated_signals=" << best.estimated_signals
         << " window_signals=" << best_plan.signal_count
         << " moves=" << best_route.size() << " size_range="
         << smallest_group << ".." << largest_group << '\n';
#endif

    for (int i = 0; i < length_a; ++i) {
        if (i) cout << ' ';
        cout << best_a[i];
    }
    cout << '\n';
    for (int i = 0; i < (int)best_route.size(); ++i) {
        if (best_plan.chosen_window[i] != -1) {
            cout << "s " << length_b << ' ' << best_plan.chosen_window[i]
                 << " 0\n";
        }
        cout << "m " << best_route[i] << '\n';
    }
}
