#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

using namespace std;

#ifndef MAX_POINT_COUNT
#define MAX_POINT_COUNT 5001
#endif

#ifndef REFINE_ROUNDS
#define REFINE_ROUNDS 5
#endif

#ifndef MERGE_LOOKAHEAD
#define MERGE_LOOKAHEAD 3
#endif

#ifndef TREE_LOCAL_PASSES
#define TREE_LOCAL_PASSES 30
#endif

#ifndef LOOKAHEAD_FUTURE_PERCENT
#define LOOKAHEAD_FUTURE_PERCENT 50
#endif

using int64 = long long;

struct Point {
    int64 x;
    int64 y;
    bool is_target;
};

struct Operation {
    int64 from_x;
    int64 from_y;
    int64 to_x;
    int64 to_y;
};

int64 operation_cost(const vector<Operation>& operations) {
    int64 cost = 0;
    for (const Operation& operation : operations) {
        cost += (operation.to_x - operation.from_x) +
                (operation.to_y - operation.from_y);
    }
    return cost;
}

int64 point_sum(const Point& point) {
    return point.x + point.y;
}

bool can_reach(const Point& from, const Point& to) {
    return from.x <= to.x && from.y <= to.y;
}

// 各点を、到達できる南西側の点のうち x+y が最大の点へつなぎます。
// 点集合が固定なら、これが各辺のコストを最小にする親です。
vector<int> choose_nearest_parents(const vector<Point>& points) {
    int point_count = static_cast<int>(points.size());
    vector<int> parent(point_count, -1);

    for (int child = 1; child < point_count; ++child) {
        int best_parent = 0;
        int64 best_sum = 0;
        for (int candidate = 0; candidate < point_count; ++candidate) {
            if (candidate == child) continue;
            if (!can_reach(points[candidate], points[child])) continue;

            int64 candidate_sum = point_sum(points[candidate]);
            if (candidate_sum > best_sum ||
                (candidate_sum == best_sum && candidate < best_parent)) {
                best_sum = candidate_sum;
                best_parent = candidate;
            }
        }
        parent[child] = best_parent;
    }
    return parent;
}

// 中継点は、子を0個または1個しか持たないなら消してもコストが増えません。
// 例えば p -> s -> c は p -> c に縮めても長さが同じです。
void remove_useless_steiner_points(vector<Point>& points, vector<int>& parent) {
    int point_count = static_cast<int>(points.size());
    vector<set<int>> children(point_count);
    for (int child = 1; child < point_count; ++child) {
        children[parent[child]].insert(child);
    }

    vector<char> active(point_count, true);
    queue<int> candidates;
    for (int node = 1; node < point_count; ++node) {
        if (!points[node].is_target && children[node].size() <= 1) {
            candidates.push(node);
        }
    }

    while (!candidates.empty()) {
        int node = candidates.front();
        candidates.pop();
        if (!active[node] || points[node].is_target || children[node].size() > 1) {
            continue;
        }

        int old_parent = parent[node];
        children[old_parent].erase(node);

        if (children[node].size() == 1) {
            int only_child = *children[node].begin();
            children[node].erase(only_child);
            parent[only_child] = old_parent;
            children[old_parent].insert(only_child);
        }

        active[node] = false;
        if (old_parent != 0 && !points[old_parent].is_target &&
            children[old_parent].size() <= 1) {
            candidates.push(old_parent);
        }
    }

    vector<int> new_index(point_count, -1);
    vector<Point> compact_points;
    compact_points.reserve(point_count);
    for (int old = 0; old < point_count; ++old) {
        if (active[old]) {
            new_index[old] = static_cast<int>(compact_points.size());
            compact_points.push_back(points[old]);
        }
    }

    vector<int> compact_parent(compact_points.size(), -1);
    for (int old = 1; old < point_count; ++old) {
        if (active[old]) {
            compact_parent[new_index[old]] = new_index[parent[old]];
        }
    }

    points.swap(compact_points);
    parent.swap(compact_parent);
}

// 同じ親から伸びる2本の枝は、座標ごとの小さい方まで幹を共有できます。
// その共有点を次の親候補として集めます。
vector<pair<int64, int64>> make_steiner_candidates(
    const vector<Point>& points, const vector<int>& parent, int limit) {
    int point_count = static_cast<int>(points.size());
    vector<vector<int>> children(point_count);
    for (int child = 1; child < point_count; ++child) {
        children[parent[child]].push_back(child);
    }

    map<pair<int64, int64>, int64> gain_by_position;
    set<pair<int64, int64>> already_exists;
    for (const Point& point : points) {
        already_exists.insert({point.x, point.y});
    }

    for (int branch = 0; branch < point_count; ++branch) {
        const vector<int>& next_nodes = children[branch];
        for (int left_index = 0; left_index < static_cast<int>(next_nodes.size()); ++left_index) {
            for (int right_index = left_index + 1;
                 right_index < static_cast<int>(next_nodes.size()); ++right_index) {
                const Point& left = points[next_nodes[left_index]];
                const Point& right = points[next_nodes[right_index]];
                int64 x = min(left.x, right.x);
                int64 y = min(left.y, right.y);
                pair<int64, int64> position = {x, y};
                if (already_exists.count(position)) continue;

                int64 gain = (x - points[branch].x) + (y - points[branch].y);
                if (gain <= 0) continue;
                auto found = gain_by_position.find(position);
                if (found == gain_by_position.end() || found->second < gain) {
                    gain_by_position[position] = gain;
                }
            }
        }
    }

    vector<tuple<int64, int64, int64>> ranked;
    ranked.reserve(gain_by_position.size());
    for (const auto& entry : gain_by_position) {
        ranked.push_back({entry.second, entry.first.first, entry.first.second});
    }
    sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
        if (get<0>(left) != get<0>(right)) return get<0>(left) > get<0>(right);
        if (get<1>(left) != get<1>(right)) return get<1>(left) < get<1>(right);
        return get<2>(left) < get<2>(right);
    });

    if (static_cast<int>(ranked.size()) > limit) ranked.resize(limit);
    vector<pair<int64, int64>> result;
    result.reserve(ranked.size());
    for (const auto& item : ranked) {
        result.push_back({get<1>(item), get<2>(item)});
    }
    return result;
}

pair<vector<Point>, vector<int>> build_solution_points(
    const vector<pair<int64, int64>>& targets,
    const vector<Operation>& extra_edges = {}) {
    vector<Point> points;
    points.push_back({0, 0, false});
    map<pair<int64, int64>, int> index_by_position;
    index_by_position[{0, 0}] = 0;

    for (const auto& target : targets) {
        auto found = index_by_position.find(target);
        if (found == index_by_position.end()) {
            int index = static_cast<int>(points.size());
            index_by_position[target] = index;
            points.push_back({target.first, target.second, true});
        } else {
            points[found->second].is_target = true;
        }
    }

    // 別の構築法が見つけた分岐点も、親候補として再利用できます。
    for (const Operation& edge : extra_edges) {
        pair<int64, int64> position = {edge.to_x, edge.to_y};
        if (index_by_position.count(position) == 0) {
            int index = static_cast<int>(points.size());
            index_by_position[position] = index;
            points.push_back({position.first, position.second, false});
        }
    }

    vector<int> parent = choose_nearest_parents(points);

#ifndef TARGET_ONLY
    for (int round = 0; round < REFINE_ROUNDS; ++round) {
        remove_useless_steiner_points(points, parent);
        int room = MAX_POINT_COUNT - static_cast<int>(points.size());
        if (room <= 0) break;

        vector<pair<int64, int64>> candidates =
            make_steiner_candidates(points, parent, room);
        if (candidates.empty()) break;

        for (const auto& candidate : candidates) {
            points.push_back({candidate.first, candidate.second, false});
        }
        parent = choose_nearest_parents(points);
    }
    remove_useless_steiner_points(points, parent);
#endif

    return {points, parent};
}

vector<Operation> make_operations(const vector<Point>& points, const vector<int>& parent) {
    vector<int> order(points.size() - 1);
    iota(order.begin(), order.end(), 1);
    sort(order.begin(), order.end(), [&](int left, int right) {
        int64 left_sum = point_sum(points[left]);
        int64 right_sum = point_sum(points[right]);
        if (left_sum != right_sum) return left_sum < right_sum;
        if (points[left].x != points[right].x) return points[left].x < points[right].x;
        return points[left].y < points[right].y;
    });

    vector<Operation> operations;
    operations.reserve(order.size());
    for (int node : order) {
        const Point& from = points[parent[node]];
        const Point& to = points[node];
        operations.push_back({from.x, from.y, to.x, to.y});
    }
    return operations;
}

// もう一つの独立した案です。
// 共通して進める距離が長い2つの部分木から順にまとめ、二分木を作ります。
vector<Operation> make_agglomerative_operations(
    const vector<pair<int64, int64>>& targets,
    int lookahead_width = MERGE_LOOKAHEAD,
    int future_percent = LOOKAHEAD_FUTURE_PERCENT) {
    struct Cluster {
        int64 x;
        int64 y;
        int left;
        int right;
    };

    int leaf_count = static_cast<int>(targets.size());
    vector<Cluster> clusters;
    clusters.reserve(2 * leaf_count);
    for (const auto& target : targets) {
        clusters.push_back({target.first, target.second, -1, -1});
    }

    vector<char> active(2 * leaf_count, false);
    for (int index = 0; index < leaf_count; ++index) active[index] = true;

    using Merge = tuple<int64, int, int>;
    priority_queue<Merge> possible_merges;
    for (int left = 0; left < leaf_count; ++left) {
        for (int right = left + 1; right < leaf_count; ++right) {
            int64 shared = min(clusters[left].x, clusters[right].x) +
                           min(clusters[left].y, clusters[right].y);
            possible_merges.push({shared, left, right});
        }
    }

    int root = 0;
    for (int merge_count = 0; merge_count + 1 < leaf_count; ++merge_count) {
        vector<Merge> choices;
        while (!possible_merges.empty() &&
               static_cast<int>(choices.size()) < lookahead_width) {
            auto [shared, candidate_left, candidate_right] = possible_merges.top();
            possible_merges.pop();
            if (active[candidate_left] && active[candidate_right]) {
                choices.push_back({shared, candidate_left, candidate_right});
            }
        }

        int selected = 0;
        int64 best_two_step = -1;
        for (int choice_index = 0; choice_index < static_cast<int>(choices.size());
             ++choice_index) {
            auto [shared, candidate_left, candidate_right] = choices[choice_index];
            int64 merged_x = min(clusters[candidate_left].x, clusters[candidate_right].x);
            int64 merged_y = min(clusters[candidate_left].y, clusters[candidate_right].y);
            int64 next_shared = 0;

            for (const Merge& other_choice : choices) {
                auto [other_shared, other_left, other_right] = other_choice;
                if (other_left != candidate_left && other_left != candidate_right &&
                    other_right != candidate_left && other_right != candidate_right) {
                    next_shared = max(next_shared, other_shared);
                }
            }
            for (int other = 0; other < static_cast<int>(clusters.size()); ++other) {
                if (!active[other] || other == candidate_left || other == candidate_right) {
                    continue;
                }
                int64 with_merged = min(merged_x, clusters[other].x) +
                                    min(merged_y, clusters[other].y);
                next_shared = max(next_shared, with_merged);
            }

            int64 two_step = 100 * shared + future_percent * next_shared;
            if (two_step > best_two_step) {
                best_two_step = two_step;
                selected = choice_index;
            }
        }

        int left = get<1>(choices[selected]);
        int right = get<2>(choices[selected]);
        for (const Merge& choice : choices) possible_merges.push(choice);

        active[left] = false;
        active[right] = false;
        int new_node = static_cast<int>(clusters.size());
        clusters.push_back({min(clusters[left].x, clusters[right].x),
                            min(clusters[left].y, clusters[right].y), left, right});
        active[new_node] = true;
        root = new_node;

        for (int other = 0; other < new_node; ++other) {
            if (!active[other]) continue;
            int64 shared = min(clusters[new_node].x, clusters[other].x) +
                           min(clusters[new_node].y, clusters[other].y);
            possible_merges.push({shared, other, new_node});
        }
    }

    auto refresh = [&](int node) {
        int left = clusters[node].left;
        int right = clusters[node].right;
        clusters[node].x = min(clusters[left].x, clusters[right].x);
        clusters[node].y = min(clusters[left].y, clusters[right].y);
    };
    auto merged_sum = [&](int left, int right) {
        return min(clusters[left].x, clusters[right].x) +
               min(clusters[left].y, clusters[right].y);
    };

    // ((A,B),C) の括り方を ((A,C),B) などへ変える局所改善です。
    // 3部分木全体の最小座標は変わらず、下側の共有幹だけが長くなります。
    for (int pass = 0; pass < TREE_LOCAL_PASSES; ++pass) {
        bool changed = false;
        for (int node = leaf_count; node < static_cast<int>(clusters.size()); ++node) {
            int left = clusters[node].left;
            int right = clusters[node].right;

            if (clusters[left].left != -1 && clusters[right].left != -1) {
                int a = clusters[left].left;
                int b = clusters[left].right;
                int c = clusters[right].left;
                int d = clusters[right].right;
                int64 current = point_sum({clusters[left].x, clusters[left].y, false}) +
                                point_sum({clusters[right].x, clusters[right].y, false});
                int64 alternative_one = merged_sum(a, c) + merged_sum(b, d);
                int64 alternative_two = merged_sum(a, d) + merged_sum(b, c);
                if (alternative_one > current && alternative_one >= alternative_two) {
                    clusters[left].left = a;
                    clusters[left].right = c;
                    clusters[right].left = b;
                    clusters[right].right = d;
                    refresh(left);
                    refresh(right);
                    refresh(node);
                    changed = true;
                } else if (alternative_two > current) {
                    clusters[left].left = a;
                    clusters[left].right = d;
                    clusters[right].left = b;
                    clusters[right].right = c;
                    refresh(left);
                    refresh(right);
                    refresh(node);
                    changed = true;
                }
                left = clusters[node].left;
                right = clusters[node].right;
            }

            if (clusters[left].left != -1) {
                int a = clusters[left].left;
                int b = clusters[left].right;
                int64 current = clusters[left].x + clusters[left].y;
                int64 pair_a = merged_sum(a, right);
                int64 pair_b = merged_sum(b, right);
                if (pair_a > current && pair_a >= pair_b) {
                    clusters[left].left = a;
                    clusters[left].right = right;
                    clusters[node].right = b;
                    refresh(left);
                    refresh(node);
                    changed = true;
                } else if (pair_b > current) {
                    clusters[left].left = b;
                    clusters[left].right = right;
                    clusters[node].right = a;
                    refresh(left);
                    refresh(node);
                    changed = true;
                }
            }

            left = clusters[node].left;
            right = clusters[node].right;
            if (clusters[right].left != -1) {
                int a = clusters[right].left;
                int b = clusters[right].right;
                int64 current = clusters[right].x + clusters[right].y;
                int64 pair_a = merged_sum(a, left);
                int64 pair_b = merged_sum(b, left);
                if (pair_a > current && pair_a >= pair_b) {
                    clusters[right].left = a;
                    clusters[right].right = left;
                    clusters[node].left = b;
                    refresh(right);
                    refresh(node);
                    changed = true;
                } else if (pair_b > current) {
                    clusters[right].left = b;
                    clusters[right].right = left;
                    clusters[node].left = a;
                    refresh(right);
                    refresh(node);
                    changed = true;
                }
            }
        }
        if (!changed) break;
    }

    vector<Operation> operations;
    operations.reserve(2 * leaf_count);
    auto visit = [&](auto&& self, int node, int64 parent_x, int64 parent_y) -> void {
        const Cluster& cluster = clusters[node];
        if (cluster.x != parent_x || cluster.y != parent_y) {
            operations.push_back({parent_x, parent_y, cluster.x, cluster.y});
        }
        if (cluster.left != -1) {
            self(self, cluster.left, cluster.x, cluster.y);
            self(self, cluster.right, cluster.x, cluster.y);
        }
    };
    visit(visit, root, 0, 0);
    return operations;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n;
    cin >> n;
    vector<pair<int64, int64>> targets(n);
    for (auto& target : targets) cin >> target.first >> target.second;

#ifdef DIRECT_BASELINE
    vector<Operation> operations;
    for (const auto& target : targets) {
        if (target != make_pair<int64, int64>(0, 0)) {
            operations.push_back({0, 0, target.first, target.second});
        }
    }
#elif defined(AGGLOMERATIVE_ONLY)
    vector<Operation> operations = make_agglomerative_operations(targets);
#else
#ifndef TARGET_ONLY
    vector<Operation> operations;
    int64 best_cost = -1;
    const vector<pair<int, int>> settings = {
        {1, LOOKAHEAD_FUTURE_PERCENT},
        {2, LOOKAHEAD_FUTURE_PERCENT},
        {3, LOOKAHEAD_FUTURE_PERCENT},
        {3, 100},
    };
    for (const auto& setting : settings) {
        vector<Operation> agglomerative =
            make_agglomerative_operations(targets, setting.first, setting.second);
        auto solution = build_solution_points(targets, agglomerative);
        vector<Operation> candidate = make_operations(solution.first, solution.second);
        if (operation_cost(agglomerative) < operation_cost(candidate)) {
            candidate.swap(agglomerative);
        }

        int64 candidate_cost = operation_cost(candidate);
        if (best_cost == -1 || candidate_cost < best_cost) {
            best_cost = candidate_cost;
            operations.swap(candidate);
        }
    }
#else
    auto solution = build_solution_points(targets);
    vector<Operation> operations = make_operations(solution.first, solution.second);
#endif
#endif

    cout << operations.size() << '\n';
    for (const Operation& operation : operations) {
        cout << operation.from_x << ' ' << operation.from_y << ' '
             << operation.to_x << ' ' << operation.to_y << '\n';
    }

    return 0;
}
