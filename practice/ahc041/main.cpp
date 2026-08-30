#include <algorithm>
#include <cstdint>
#include <iostream>
#include <queue>
#include <utility>
#include <vector>

using namespace std;

struct Random {
    uint64_t state;

    explicit Random(uint64_t seed) : state(seed) {}

    uint32_t next_u32() {
        state ^= state << 7;
        state ^= state >> 9;
        return static_cast<uint32_t>(state);
    }

    int next_int(int upper) {
        return static_cast<int>(next_u32() % static_cast<uint32_t>(upper));
    }
};

struct Solution {
    long long score = 0;
    vector<int> parent;
};

class Solver {
  public:
    void run() {
        read_input();

#ifdef AHC041_BASELINE
        const Solution answer = make_bfs_baseline();
#else
        Solution answer = search();
#endif

        for (int vertex = 0; vertex < n_; ++vertex) {
            if (vertex != 0) cout << ' ';
            cout << answer.parent[vertex];
        }
        cout << '\n';
    }

  private:
    int n_ = 0;
    int m_ = 0;
    int height_limit_ = 0;
    vector<int> beauty_;
    vector<vector<int>> graph_;

    void read_input() {
        cin >> n_ >> m_ >> height_limit_;
        beauty_.resize(n_);
        for (int& value : beauty_) cin >> value;

        graph_.assign(n_, {});
        for (int edge = 0; edge < m_; ++edge) {
            int left, right;
            cin >> left >> right;
            graph_[left].push_back(right);
            graph_[right].push_back(left);
        }

        // Coordinates are useful to the visualizer, but this construction only
        // needs the graph itself.
        for (int vertex = 0; vertex < n_; ++vertex) {
            int x, y;
            cin >> x >> y;
        }
    }

    long long calculate_score(const vector<int>& depth) const {
        long long score = 1;
        for (int vertex = 0; vertex < n_; ++vertex) {
            score += static_cast<long long>(depth[vertex] + 1) * beauty_[vertex];
        }
        return score;
    }

    Solution make_bfs_baseline() const {
        vector<int> bfs_parent(n_, -1);
        vector<int> distance(n_, -1);
        queue<int> que;
        distance[0] = 0;
        que.push(0);
        while (!que.empty()) {
            const int vertex = que.front();
            que.pop();
            for (int next : graph_[vertex]) {
                if (distance[next] != -1) continue;
                distance[next] = distance[vertex] + 1;
                bfs_parent[next] = vertex;
                que.push(next);
            }
        }

        vector<int> parent(n_, -1);
        vector<int> depth(n_, 0);
        for (int vertex = 0; vertex < n_; ++vertex) {
            depth[vertex] = distance[vertex] % (height_limit_ + 1);
            if (depth[vertex] != 0) parent[vertex] = bfs_parent[vertex];
        }
        return {calculate_score(depth), parent};
    }

    Solution make_depth_limited_dfs(Random& random, int noise,
                                    int degree_bonus) const {
        vector<int> parent(n_, -1);
        vector<int> depth(n_, -1);

        vector<pair<int, int>> root_order;
        root_order.reserve(n_);
        for (int vertex = 0; vertex < n_; ++vertex) {
            const int jitter =
                noise == 0 ? 0 : random.next_int(2 * noise + 1) - noise;
            const int key =
                beauty_[vertex] * 100 -
                static_cast<int>(graph_[vertex].size()) * degree_bonus + jitter;
            root_order.push_back({key, vertex});
        }
        sort(root_order.begin(), root_order.end());

        auto visit = [&](auto&& self, int vertex) -> void {
            if (depth[vertex] == height_limit_) return;

            vector<pair<int, int>> order;
            order.reserve(graph_[vertex].size());
            for (int next : graph_[vertex]) {
                if (depth[next] != -1) continue;
                const int jitter =
                    noise == 0 ? 0 : random.next_int(2 * noise + 1) - noise;
                int key;
                if (depth[vertex] + 1 == height_limit_) {
                    // The last layer cannot support more vertices, so put the
                    // beautiful vertices there first.
                    key = -beauty_[next] * 100 + jitter;
                } else {
                    // Shallow vertices are supports.  Prefer cheap, well
                    // connected vertices for these positions.
                    key = beauty_[next] * 100 -
                          static_cast<int>(graph_[next].size()) * degree_bonus +
                          jitter;
                }
                order.push_back({key, next});
            }
            sort(order.begin(), order.end());

            for (const auto& [key, next] : order) {
                (void)key;
                if (depth[next] != -1) continue;
                parent[next] = vertex;
                depth[next] = depth[vertex] + 1;
                self(self, next);
            }
        };

        for (const auto& [key, root] : root_order) {
            (void)key;
            if (depth[root] != -1) continue;
            depth[root] = 0;
            visit(visit, root);
        }

        return {calculate_score(depth), parent};
    }

    struct ForestData {
        vector<int> depth;
        vector<int> subtree_height;
        vector<long long> subtree_beauty;
        vector<int> entry_time;
        vector<int> exit_time;
    };

    ForestData inspect_forest(const vector<int>& parent) const {
        vector<vector<int>> children(n_);
        for (int vertex = 0; vertex < n_; ++vertex) {
            if (parent[vertex] != -1) children[parent[vertex]].push_back(vertex);
        }

        ForestData data;
        data.depth.assign(n_, 0);
        data.subtree_height.assign(n_, 0);
        data.subtree_beauty.assign(n_, 0);
        data.entry_time.resize(n_);
        data.exit_time.resize(n_);
        int timer = 0;

        auto dfs = [&](auto&& self, int vertex, int depth) -> void {
            data.depth[vertex] = depth;
            data.entry_time[vertex] = timer++;
            data.subtree_beauty[vertex] = beauty_[vertex];
            for (int child : children[vertex]) {
                self(self, child, depth + 1);
                data.subtree_height[vertex] =
                    max(data.subtree_height[vertex],
                        data.subtree_height[child] + 1);
                data.subtree_beauty[vertex] += data.subtree_beauty[child];
            }
            data.exit_time[vertex] = timer;
        };

        for (int vertex = 0; vertex < n_; ++vertex) {
            if (parent[vertex] == -1) dfs(dfs, vertex, 0);
        }
        return data;
    }

    void deepen_subtrees(Solution& solution) const {
        // Reattach one whole subtree at a time.  Moving it d levels deeper
        // gains d times the sum of beauty values in that subtree.
        while (true) {
            const ForestData data = inspect_forest(solution.parent);
            long long best_gain = 0;
            int best_vertex = -1;
            int best_parent = -1;

            for (int vertex = 0; vertex < n_; ++vertex) {
                for (int next : graph_[vertex]) {
                    const int new_depth = data.depth[next] + 1;
                    if (new_depth <= data.depth[vertex]) continue;
                    if (new_depth + data.subtree_height[vertex] > height_limit_) {
                        continue;
                    }

                    const bool next_is_in_subtree =
                        data.entry_time[vertex] <= data.entry_time[next] &&
                        data.entry_time[next] < data.exit_time[vertex];
                    if (next_is_in_subtree) continue;

                    const long long gain =
                        static_cast<long long>(new_depth - data.depth[vertex]) *
                        data.subtree_beauty[vertex];
                    if (gain > best_gain) {
                        best_gain = gain;
                        best_vertex = vertex;
                        best_parent = next;
                    }
                }
            }

            if (best_vertex == -1) {
                solution.score = calculate_score(data.depth);
                return;
            }
            solution.parent[best_vertex] = best_parent;
        }
    }

    Solution search() const {
        Random random(0x6a09e667f3bcc909ULL);
        Solution best = make_bfs_baseline();

#ifdef LOCAL_SHORT_TIME
        constexpr int TRIALS = 40;
#elif defined(AHC041_TRIALS)
        constexpr int TRIALS = AHC041_TRIALS;
#else
        constexpr int TRIALS = 15000;
#endif

        constexpr int FINALIST_COUNT = 6;
        vector<Solution> finalists;
        finalists.reserve(FINALIST_COUNT);
        for (int trial = 0; trial < TRIALS; ++trial) {
            static constexpr int NOISE_LEVELS[] = {0, 50, 150, 400, 1000, 2500};
            const int noise = NOISE_LEVELS[trial % 6];
            const int degree_bonus =
                trial < 12000 ? 10 + random.next_int(81)
                              : 91 + random.next_int(410);
            Solution candidate =
                make_depth_limited_dfs(random, noise, degree_bonus);

            finalists.push_back(std::move(candidate));
            sort(finalists.begin(), finalists.end(),
                 [](const Solution& left, const Solution& right) {
                     return left.score > right.score;
                 });
            if (static_cast<int>(finalists.size()) > FINALIST_COUNT) {
                finalists.pop_back();
            }
        }

        for (Solution& candidate : finalists) {
            deepen_subtrees(candidate);
            if (candidate.score > best.score) best = std::move(candidate);
        }

        return best;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Solver solver;
    solver.run();
    return 0;
}
