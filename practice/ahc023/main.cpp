#include <algorithm>
#include <deque>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

using namespace std;

// AHC023: keep a connected set of cells permanently empty as passages.
// Every other cell touches a passage, so planting and harvesting there is safe.

struct Edge {
    int to;
    int reverse_index;
    int capacity;
    int cost;
};

void add_edge(vector<vector<Edge>>& graph, int from, int to, int capacity, int cost) {
    int from_index = static_cast<int>(graph[from].size());
    int to_index = static_cast<int>(graph[to].size());
    graph[from].push_back({to, to_index, capacity, cost});
    graph[to].push_back({from, from_index, 0, -cost});
}

bool is_valid_passage(const vector<char>& passage, int entrance,
                      const vector<vector<int>>& adjacent) {
    if (!passage[entrance]) return false;

    const int n = static_cast<int>(passage.size());
    vector<char> reached(n, false);
    queue<int> que;
    reached[entrance] = true;
    que.push(entrance);

    while (!que.empty()) {
        int v = que.front();
        que.pop();
        for (int next : adjacent[v]) {
            if (passage[next] && !reached[next]) {
                reached[next] = true;
                que.push(next);
            }
        }
    }

    for (int v = 0; v < n; ++v) {
        if (passage[v] && !reached[v]) return false;
        if (!passage[v]) {
            bool touches_passage = false;
            for (int next : adjacent[v]) {
                if (passage[next]) touches_passage = true;
            }
            if (!touches_passage) return false;
        }
    }
    return true;
}

void remove_redundant_passages(vector<char>& passage, int entrance,
                               const vector<vector<int>>& adjacent,
                               mt19937& random_engine, int protected_cell = -1) {
    vector<int> order(passage.size());
    iota(order.begin(), order.end(), 0);

    bool changed = true;
    while (changed) {
        changed = false;
        shuffle(order.begin(), order.end(), random_engine);
        for (int v : order) {
            if (v == entrance || v == protected_cell || !passage[v]) continue;
            passage[v] = false;
            if (is_valid_passage(passage, entrance, adjacent)) {
                changed = true;
            } else {
                passage[v] = true;
            }
        }
    }
}

vector<char> make_greedy_passage(int entrance, const vector<vector<int>>& adjacent,
                                 mt19937& random_engine, bool allow_noisy_choice) {
    const int n = static_cast<int>(adjacent.size());
    vector<char> passage(n, false);
    vector<char> dominated(n, false);
    passage[entrance] = true;

    auto dominate_from = [&](int v) {
        dominated[v] = true;
        for (int next : adjacent[v]) dominated[next] = true;
    };
    dominate_from(entrance);

    while (count(dominated.begin(), dominated.end(), true) < n) {
        int chosen = -1;
        int best_value = numeric_limits<int>::min();

        for (int v = 0; v < n; ++v) {
            if (passage[v]) continue;

            int passage_neighbors = 0;
            for (int next : adjacent[v]) passage_neighbors += passage[next];
            if (passage_neighbors == 0) continue;

            int gain = !dominated[v];
            for (int next : adjacent[v]) gain += !dominated[next];

            // Usually prefer the largest immediate gain. Some restarts allow a
            // slightly worse step, which produces a different passage shape.
            int noise = allow_noisy_choice
                            ? static_cast<int>(random_engine() % 1801)
                            : static_cast<int>(random_engine() % 1000);
            int gain_scale = allow_noisy_choice ? 1000 : 10000;
            int value = gain * gain_scale - passage_neighbors * 20 + noise;
            if (value > best_value) {
                best_value = value;
                chosen = v;
            }
        }

        // The whole field is connected, so a frontier cell always exists.
        if (chosen == -1) break;
        passage[chosen] = true;
        dominate_from(chosen);
    }

    remove_redundant_passages(passage, entrance, adjacent, random_engine);
    return passage;
}

vector<char> make_bfs_tree_passage(int entrance, const vector<vector<int>>& adjacent) {
    const int n = static_cast<int>(adjacent.size());
    vector<int> parent(n, -1);
    vector<int> child_count(n, 0);
    queue<int> que;
    parent[entrance] = entrance;
    que.push(entrance);

    while (!que.empty()) {
        int v = que.front();
        que.pop();
        for (int next : adjacent[v]) {
            if (parent[next] != -1) continue;
            parent[next] = v;
            ++child_count[v];
            que.push(next);
        }
    }

    vector<char> passage(n, false);
    for (int v = 0; v < n; ++v) {
        if (v == entrance || child_count[v] > 0) passage[v] = true;
    }
    return passage;
}

vector<int> choose_crops_exactly(const vector<int>& start_month,
                                 const vector<int>& harvest_month,
                                 int number_of_cells, int number_of_months) {
    vector<vector<Edge>> graph(number_of_months + 1);
    for (int month = 0; month < number_of_months; ++month) {
        add_edge(graph, month, month + 1, number_of_cells, 0);
    }

    const int number_of_crops = static_cast<int>(start_month.size());
    vector<pair<int, int>> crop_edge(number_of_crops);
    for (int crop = 0; crop < number_of_crops; ++crop) {
        int from = start_month[crop] - 1;
        crop_edge[crop] = {from, static_cast<int>(graph[from].size())};
        int value = harvest_month[crop] - start_month[crop] + 1;
        add_edge(graph, from, harvest_month[crop], 1, -value);
    }

    const int source = 0;
    const int sink = number_of_months;
    int used_cells = 0;

    while (used_cells < number_of_cells) {
        const int infinity = numeric_limits<int>::max() / 4;
        vector<int> distance(number_of_months + 1, infinity);
        vector<int> previous_vertex(number_of_months + 1, -1);
        vector<int> previous_edge(number_of_months + 1, -1);
        vector<char> in_queue(number_of_months + 1, false);
        deque<int> que;

        distance[source] = 0;
        que.push_back(source);
        in_queue[source] = true;

        // Only 101 vertices are used, so this simple shortest-path routine is
        // fast enough even though residual edges may have negative costs.
        while (!que.empty()) {
            int v = que.front();
            que.pop_front();
            in_queue[v] = false;
            for (int edge_index = 0; edge_index < static_cast<int>(graph[v].size());
                 ++edge_index) {
                const Edge& edge = graph[v][edge_index];
                if (edge.capacity == 0) continue;
                if (distance[edge.to] > distance[v] + edge.cost) {
                    distance[edge.to] = distance[v] + edge.cost;
                    previous_vertex[edge.to] = v;
                    previous_edge[edge.to] = edge_index;
                    if (!in_queue[edge.to]) {
                        in_queue[edge.to] = true;
                        que.push_back(edge.to);
                    }
                }
            }
        }

        // A zero-cost path contains no crop, so adding it cannot improve score.
        if (distance[sink] >= 0 || distance[sink] == infinity) break;

        int v = sink;
        while (v != source) {
            int from = previous_vertex[v];
            int edge_index = previous_edge[v];
            Edge& edge = graph[from][edge_index];
            --edge.capacity;
            ++graph[v][edge.reverse_index].capacity;
            v = from;
        }
        ++used_cells;
    }

    vector<int> selected;
    for (int crop = 0; crop < number_of_crops; ++crop) {
        auto [from, edge_index] = crop_edge[crop];
        if (graph[from][edge_index].capacity == 0) selected.push_back(crop);
    }
    return selected;
}

vector<int> choose_crops_greedily(const vector<int>& start_month,
                                  const vector<int>& harvest_month,
                                  int number_of_cells) {
    vector<int> order(start_month.size());
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int left, int right) {
        if (harvest_month[left] != harvest_month[right]) {
            return harvest_month[left] < harvest_month[right];
        }
        return start_month[left] < start_month[right];
    });

    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> cells;
    for (int cell = 0; cell < number_of_cells; ++cell) cells.push({0, cell});

    vector<int> selected;
    for (int crop : order) {
        auto [last_harvest, cell] = cells.top();
        if (last_harvest < start_month[crop]) {
            cells.pop();
            cells.push({harvest_month[crop], cell});
            selected.push_back(crop);
        }
    }
    return selected;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int T, H, W, entrance_row;
    cin >> T >> H >> W >> entrance_row;

    vector<string> horizontal_waterways(H - 1);
    vector<string> vertical_waterways(H);
    for (string& row : horizontal_waterways) cin >> row;
    for (string& row : vertical_waterways) cin >> row;

    int K;
    cin >> K;
    vector<int> S(K), D(K);
    for (int crop = 0; crop < K; ++crop) cin >> S[crop] >> D[crop];

    const int number_of_blocks = H * W;
    vector<vector<int>> adjacent(number_of_blocks);
    auto id = [&](int row, int column) { return row * W + column; };

    for (int row = 0; row < H; ++row) {
        for (int column = 0; column < W; ++column) {
            if (row + 1 < H && horizontal_waterways[row][column] == '0') {
                int a = id(row, column);
                int b = id(row + 1, column);
                adjacent[a].push_back(b);
                adjacent[b].push_back(a);
            }
            if (column + 1 < W && vertical_waterways[row][column] == '0') {
                int a = id(row, column);
                int b = id(row, column + 1);
                adjacent[a].push_back(b);
                adjacent[b].push_back(a);
            }
        }
    }

    const int entrance = id(entrance_row, 0);
    mt19937 random_engine(20230819);

#ifdef AHC023_SIMPLE_BASELINE
    vector<char> passage = make_bfs_tree_passage(entrance, adjacent);
#else
    vector<char> passage(number_of_blocks, true);
    int best_passage_count = number_of_blocks;

#ifdef AHC023_SHORT_VERIFY
    const int construction_attempts = 24;
    const int replacement_attempts = 80;
#else
    const int construction_attempts = 320;
    const int replacement_attempts = 1500;
#endif

    // Randomized restarts are cheap on a 20 x 20 grid.
    for (int attempt = 0; attempt < construction_attempts; ++attempt) {
        vector<char> candidate = make_greedy_passage(
            entrance, adjacent, random_engine, attempt * 8 >= construction_attempts * 3);
        int candidate_count = static_cast<int>(
            count(candidate.begin(), candidate.end(), true));
        if (candidate_count < best_passage_count) {
            best_passage_count = candidate_count;
            passage = move(candidate);
        }
    }

    // Adding one temporary passage can make several old passages redundant.
    for (int attempt = 0; attempt < replacement_attempts; ++attempt) {
        vector<int> crop_candidates;
        for (int v = 0; v < number_of_blocks; ++v) {
            if (!passage[v]) crop_candidates.push_back(v);
        }
        if (crop_candidates.empty()) break;

        int added = crop_candidates[random_engine() % crop_candidates.size()];
        vector<char> candidate = passage;
        candidate[added] = true;
        remove_redundant_passages(candidate, entrance, adjacent, random_engine, added);
        remove_redundant_passages(candidate, entrance, adjacent, random_engine);

        int candidate_count = static_cast<int>(
            count(candidate.begin(), candidate.end(), true));
        // Equal-sized shapes are also useful: the next replacement attempt can
        // escape from a different local minimum.
        if (candidate_count <= best_passage_count) {
            best_passage_count = candidate_count;
            passage = move(candidate);
        }
    }
#endif

    vector<int> crop_cells;
    for (int v = 0; v < number_of_blocks; ++v) {
        if (!passage[v]) crop_cells.push_back(v);
    }

#ifdef AHC023_GREEDY_SCHEDULE
    vector<int> selected = choose_crops_greedily(
        S, D, static_cast<int>(crop_cells.size()));
#else
    vector<int> selected = choose_crops_exactly(
        S, D, static_cast<int>(crop_cells.size()), T);
#endif

    sort(selected.begin(), selected.end(), [&](int left, int right) {
        if (S[left] != S[right]) return S[left] < S[right];
        return D[left] < D[right];
    });

    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> free_cells;
    for (int cell = 0; cell < static_cast<int>(crop_cells.size()); ++cell) {
        free_cells.push({0, cell});
    }

    vector<tuple<int, int, int, int>> answer;
    for (int crop : selected) {
        auto [last_harvest, cell_index] = free_cells.top();
        free_cells.pop();
        if (last_harvest >= S[crop]) {
            // This should be impossible: interval flow guarantees colorability.
            continue;
        }
        int block = crop_cells[cell_index];
        answer.push_back({crop + 1, block / W, block % W, S[crop]});
        free_cells.push({D[crop], cell_index});
    }

    cout << answer.size() << '\n';
    for (auto [crop, row, column, start] : answer) {
        cout << crop << ' ' << row << ' ' << column << ' ' << start << '\n';
    }
    return 0;
}
