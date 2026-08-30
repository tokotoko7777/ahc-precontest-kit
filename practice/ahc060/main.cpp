#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <queue>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std;

struct RandomNumber {
    uint64_t state;

    explicit RandomNumber(uint64_t seed) : state(seed) {}

    uint64_t next() {
        state ^= state << 7;
        state ^= state >> 9;
        return state;
    }

    int next_int(int upper_bound) {
        return static_cast<int>(next() % static_cast<uint64_t>(upper_bound));
    }
};

struct Answer {
    vector<int> operations;
    int score = 0;
};

struct DeliveryCandidate {
    int shop = -1;
    int last_vertex = -1;
    int parent_node = -1;
    int depth = 0;
    string word;
};

class Solver {
  public:
    Solver(int vertex_count, int shop_count, int action_limit,
           const vector<vector<int>>& graph, vector<char> should_be_red,
           double time_limit_seconds)
        : n(vertex_count), k(shop_count), limit(action_limit), edges(graph),
          red(move(should_be_red)), flipped(vertex_count, false),
          inventory(shop_count), arrival_count(vertex_count * vertex_count, 0),
          time_limit(time_limit_seconds) {}

    Answer run() {
        start_time = chrono::steady_clock::now();
        prepare_red_trees();

        while (static_cast<int>(operations.size()) < limit && !time_is_up()) {
            vector<int> route = find_next_route();
            if (route.empty()) break;
            if (static_cast<int>(operations.size() + route.size()) > limit) break;
            for (int vertex : route) move_to(vertex, false);
        }
        return Answer{operations, score};
    }

  private:
    struct SearchNode {
        int previous;
        int current;
        int parent;
        int depth;
        string word;
    };

    int n;
    int k;
    int limit;
    const vector<vector<int>>& edges;
    vector<char> red;
    vector<char> flipped;
    vector<unordered_set<string>> inventory;
    vector<int> arrival_count;
    vector<int> operations;
    int position = 0;
    int previous = -1;
    string cone;
    int score = 0;
    double time_limit;
    chrono::steady_clock::time_point start_time{};

    bool time_is_up() const {
        return chrono::duration<double>(chrono::steady_clock::now() - start_time).count() >=
               time_limit;
    }

    bool move_to(int destination, bool flip_on_arrival) {
        if (static_cast<int>(operations.size()) >= limit) return false;
        operations.push_back(destination);
        previous = exchange(position, destination);

        if (position < k) {
            if (inventory[position].insert(cone).second) ++score;
            cone.clear();
        } else {
            cone.push_back(flipped[position] ? 'R' : 'W');
            if (flip_on_arrival && red[position] && !flipped[position] &&
                static_cast<int>(operations.size()) < limit) {
                operations.push_back(-1);
                flipped[position] = true;
            }
        }
        return true;
    }

    vector<int> route_to_nearest_unflipped_tree() const {
        struct State {
            int previous;
            int current;
            int parent;
        };

        vector<State> states;
        states.push_back(State{previous, position, -1});
        queue<int> que;
        que.push(0);
        vector<char> seen((n + 1) * n, false);
        seen[(previous + 1) * n + position] = true;
        int goal = -1;

        while (!que.empty() && goal == -1) {
            const int index = que.front();
            que.pop();
            const State state = states[index];
            for (int to : edges[state.current]) {
                if (to == state.previous) continue;
                const int key = (state.current + 1) * n + to;
                if (seen[key]) continue;
                seen[key] = true;
                states.push_back(State{state.current, to, index});
                const int next_index = static_cast<int>(states.size()) - 1;
                if (red[to] && !flipped[to]) {
                    goal = next_index;
                    break;
                }
                que.push(next_index);
            }
        }

        vector<int> route;
        while (goal > 0) {
            route.push_back(states[goal].current);
            goal = states[goal].parent;
        }
        reverse(route.begin(), route.end());
        return route;
    }

    void prepare_red_trees() {
        int remaining = 0;
        for (int vertex = k; vertex < n; ++vertex) remaining += red[vertex];

        while (remaining > 0 && static_cast<int>(operations.size()) < limit) {
            if (position >= k && red[position] && !flipped[position]) {
                operations.push_back(-1);
                flipped[position] = true;
                --remaining;
                continue;
            }

            const vector<int> route = route_to_nearest_unflipped_tree();
            if (route.empty()) break;
            for (int vertex : route) {
                const bool was_unflipped_red = vertex >= k && red[vertex] && !flipped[vertex];
                if (!move_to(vertex, true)) return;
                if (was_unflipped_red) --remaining;
            }
        }
    }

    static string state_key(int previous_vertex, int current_vertex, const string& word) {
        string key;
        key.reserve(word.size() + 2);
        key.push_back(static_cast<char>(previous_vertex + 1));
        key.push_back(static_cast<char>(current_vertex + 1));
        key += word;
        return key;
    }

    vector<int> restore_route(const vector<SearchNode>& nodes,
                              const DeliveryCandidate& candidate) const {
        vector<int> route;
        route.push_back(candidate.shop);
        int index = candidate.parent_node;
        while (nodes[index].parent != -1) {
            route.push_back(nodes[index].current);
            index = nodes[index].parent;
        }
        reverse(route.begin(), route.end());
        return route;
    }

    bool better_new_candidate(const DeliveryCandidate& left,
                              const DeliveryCandidate& right) const {
        if (right.shop == -1) return true;
        if (inventory[left.shop].size() != inventory[right.shop].size()) {
            return inventory[left.shop].size() < inventory[right.shop].size();
        }
        const int left_arrival = arrival_count[left.shop * n + left.last_vertex];
        const int right_arrival = arrival_count[right.shop * n + right.last_vertex];
        if (left_arrival != right_arrival) return left_arrival < right_arrival;
        if (left.shop != right.shop) return left.shop < right.shop;
        if (left.word != right.word) return left.word < right.word;
        return left.last_vertex < right.last_vertex;
    }

    bool better_fallback(const DeliveryCandidate& left,
                         const DeliveryCandidate& right) const {
        if (right.shop == -1) return true;
        const int left_arrival = arrival_count[left.shop * n + left.last_vertex];
        const int right_arrival = arrival_count[right.shop * n + right.last_vertex];
        if (left_arrival != right_arrival) return left_arrival < right_arrival;
        if (left.depth != right.depth) return left.depth < right.depth;
        if (inventory[left.shop].size() != inventory[right.shop].size()) {
            return inventory[left.shop].size() < inventory[right.shop].size();
        }
        return left.shop < right.shop;
    }

    vector<int> find_next_route() {
        constexpr int MAX_DEPTH = 22;
        constexpr int MAX_STATES = 30000;

        vector<SearchNode> nodes;
        nodes.reserve(4096);
        nodes.push_back(SearchNode{previous, position, -1, 0, cone});
        size_t queue_head = 0;

        unordered_set<string> seen;
        seen.reserve(8192);
        seen.insert(state_key(previous, position, cone));

        int first_new_depth = -1;
        DeliveryCandidate best_new;
        DeliveryCandidate best_fallback;

        while (queue_head < nodes.size() && static_cast<int>(nodes.size()) < MAX_STATES) {
            if ((queue_head & 1023U) == 0U && time_is_up()) break;
            const int node_index = static_cast<int>(queue_head++);
            const SearchNode node = nodes[node_index];
            if (node.depth >= MAX_DEPTH) continue;
            if (first_new_depth != -1 && node.depth + 1 > first_new_depth) break;

            for (int to : edges[node.current]) {
                if (to == node.previous) continue;
                const int next_depth = node.depth + 1;

                if (to < k) {
                    DeliveryCandidate candidate;
                    candidate.shop = to;
                    candidate.last_vertex = node.current;
                    candidate.parent_node = node_index;
                    candidate.depth = next_depth;
                    candidate.word = node.word;

                    if (better_fallback(candidate, best_fallback)) best_fallback = candidate;
                    if (inventory[to].find(node.word) == inventory[to].end()) {
                        if (first_new_depth == -1) first_new_depth = next_depth;
                        if (next_depth == first_new_depth &&
                            better_new_candidate(candidate, best_new)) {
                            best_new = candidate;
                        }
                    }
                    continue;
                }

                if (first_new_depth != -1) continue;
                string next_word = node.word;
                next_word.push_back(red[to] ? 'R' : 'W');
                string key = state_key(node.current, to, next_word);
                if (!seen.insert(move(key)).second) continue;
                nodes.push_back(SearchNode{node.current, to, node_index, next_depth,
                                           move(next_word)});
            }
        }

        const DeliveryCandidate chosen = (best_new.shop != -1 ? best_new : best_fallback);
        if (chosen.shop != -1) {
            ++arrival_count[chosen.shop * n + chosen.last_vertex];
            return restore_route(nodes, chosen);
        }

        // The depth/state cap is only a speed guard.  If it is reached before
        // finding a shop, make one legal move and search again from there.
        for (int to : edges[position]) {
            if (to != previous) return vector<int>{to};
        }
        return {};
    }
};

vector<char> make_red_selection(int n, int k, int red_count, uint64_t seed) {
    vector<int> trees;
    for (int vertex = k; vertex < n; ++vertex) trees.push_back(vertex);
    RandomNumber random(seed);
    for (int i = static_cast<int>(trees.size()) - 1; i > 0; --i) {
        swap(trees[i], trees[random.next_int(i + 1)]);
    }

    vector<char> red(n, false);
    for (int i = 0; i < red_count; ++i) red[trees[i]] = true;
    return red;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, edge_count, shop_count, action_limit;
    cin >> n >> edge_count >> shop_count >> action_limit;
    vector<vector<int>> graph(n);
    for (int i = 0; i < edge_count; ++i) {
        int first, second;
        cin >> first >> second;
        graph[first].push_back(second);
        graph[second].push_back(first);
    }
    for (vector<int>& neighbors : graph) sort(neighbors.begin(), neighbors.end());
    for (int i = 0; i < n; ++i) {
        int x, y;
        cin >> x >> y;
    }

    Answer best;

#ifdef BASELINE
    Solver solver(n, shop_count, action_limit, graph, vector<char>(n, false), 1.70);
    best = solver.run();
#else
    const array<int, 5> red_counts{25, 35, 45, 55, 65};
    for (int red_count : red_counts) {
        for (int attempt = 0; attempt < 4; ++attempt) {
            const uint64_t seed = 0x9e3779b97f4a7c15ULL *
                                  static_cast<uint64_t>(1 + attempt + red_count * 3);
            Solver solver(n, shop_count, action_limit, graph,
                          make_red_selection(n, shop_count, red_count, seed), 0.075);
            Answer candidate = solver.run();
#ifdef LOCAL_REPORT
            cerr << "red=" << red_count << " attempt=" << attempt
                 << " score=" << candidate.score
                 << " actions=" << candidate.operations.size() << '\n';
#endif
            if (candidate.score > best.score) best = move(candidate);
        }
    }
#endif

    for (int operation : best.operations) cout << operation << '\n';
    return 0;
}
