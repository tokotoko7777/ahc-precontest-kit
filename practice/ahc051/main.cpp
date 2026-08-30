#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

using namespace std;

// AHC051: Probabilistic Waste Sorting
//
// 1. Grow a crossing-free binary tree by replacing one processor leaf at a
//    time with "sorter + two processor leaves".
// 2. Assign waste types and sorter kinds on that fixed tree so that the sum of
//    correct-arrival probabilities becomes large.

struct Point {
    long long x;
    long long y;
};

struct Edge {
    int from;
    int to;
};

struct Geometry {
    bool ok = false;
    int root = -1;
    vector<array<int, 2>> child;
    vector<char> used_sorter;
};

struct LogicalPlan {
    vector<int> processor_type;
    vector<int> sorter_type;
    vector<char> flipped;
    double quality = -1.0;
};

int waste_count;
int position_count;
int sorter_kind_count;
vector<Point> position_of;
vector<vector<double>> probability_to_exit1;

uint64_t random_state = 0x123456789abcdef0ULL;

uint64_t next_random() {
    random_state ^= random_state << 7;
    random_state ^= random_state >> 9;
    return random_state;
}

int random_int(int limit) {
    return static_cast<int>(next_random() % static_cast<uint64_t>(limit));
}

int inlet_id() {
    return waste_count + position_count;
}

Point get_point(int vertex) {
    if (vertex == inlet_id()) {
        return {0, 5000};
    }
    return position_of[vertex];
}

long long squared_distance(int a, int b) {
    const Point p = get_point(a);
    const Point q = get_point(b);
    const long long dx = p.x - q.x;
    const long long dy = p.y - q.y;
    return dx * dx + dy * dy;
}

long long orientation(Point a, Point b, Point c) {
    const long long cross =
        (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0) {
        return 1;
    }
    if (cross < 0) {
        return -1;
    }
    return 0;
}

bool segments_intersect(Point a, Point b, Point c, Point d) {
    if (max(a.x, b.x) < min(c.x, d.x) ||
        max(c.x, d.x) < min(a.x, b.x) ||
        max(a.y, b.y) < min(c.y, d.y) ||
        max(c.y, d.y) < min(a.y, b.y)) {
        return false;
    }
    const long long ab_c = orientation(a, b, c);
    const long long ab_d = orientation(a, b, d);
    const long long cd_a = orientation(c, d, a);
    const long long cd_b = orientation(c, d, b);
    return ab_c * ab_d <= 0 && cd_a * cd_b <= 0;
}

bool edges_cross(const Edge& a, const Edge& b) {
    if (a.from == b.from || a.from == b.to || a.to == b.from ||
        a.to == b.to) {
        return false;
    }
    return segments_intersect(get_point(a.from), get_point(a.to),
                              get_point(b.from), get_point(b.to));
}

vector<Edge> collect_edges(const Geometry& geometry) {
    vector<Edge> edges;
    edges.push_back({inlet_id(), geometry.root});
    for (int sorter = 0; sorter < position_count; ++sorter) {
        if (!geometry.used_sorter[sorter]) {
            continue;
        }
        const int vertex = waste_count + sorter;
        edges.push_back({vertex, geometry.child[sorter][0]});
        edges.push_back({vertex, geometry.child[sorter][1]});
    }
    return edges;
}

bool insertion_is_planar(const vector<Edge>& edges, int parent, int old_leaf,
                         int sorter_vertex, int new_processor) {
    const array<Edge, 3> added = {
        Edge{parent, sorter_vertex}, Edge{sorter_vertex, old_leaf},
        Edge{sorter_vertex, new_processor}};

    for (const Edge& add : added) {
        for (const Edge& old : edges) {
            if (old.from == parent && old.to == old_leaf) {
                continue;
            }
            if (edges_cross(add, old)) {
                return false;
            }
        }
    }
    return true;
}

// Every insertion replaces one leaf edge by a Y-shaped piece. Therefore the
// graph stays a tree. Exact segment tests ensure that it also stays planar.
Geometry build_geometry() {
    Geometry geometry;
    geometry.child.assign(position_count, array<int, 2>{-1, -1});
    geometry.used_sorter.assign(position_count, false);

    vector<char> used_processor(waste_count, false);
    vector<int> parent(waste_count + position_count, -1);
    vector<int> depth(waste_count, -1);

    int first_processor = 0;
    for (int processor = 1; processor < waste_count; ++processor) {
        if (squared_distance(inlet_id(), processor) <
            squared_distance(inlet_id(), first_processor)) {
            first_processor = processor;
        }
    }
    geometry.root = first_processor;
    used_processor[first_processor] = true;
    parent[first_processor] = inlet_id();
    depth[first_processor] = 0;

    for (int inserted = 1; inserted < waste_count; ++inserted) {
        const vector<Edge> edges = collect_edges(geometry);

        bool found = false;
        int best_leaf = -1;
        int best_processor = -1;
        int best_sorter = -1;
        long long best_cost = numeric_limits<long long>::max();

        int minimum_depth = waste_count;
        for (int leaf = 0; leaf < waste_count; ++leaf) {
            if (used_processor[leaf]) {
                minimum_depth = min(minimum_depth, depth[leaf]);
            }
        }

        // Prefer a shallow leaf, which keeps the classification tree balanced.
        // If geometry makes that impossible, allow the next depth.
        for (int wanted_depth = minimum_depth;
             wanted_depth <= waste_count && !found; ++wanted_depth) {
            for (int leaf = 0; leaf < waste_count; ++leaf) {
                if (!used_processor[leaf] || depth[leaf] != wanted_depth) {
                    continue;
                }
                const int old_parent = parent[leaf];

                for (int processor = 0; processor < waste_count; ++processor) {
                    if (used_processor[processor]) {
                        continue;
                    }
                    for (int sorter = 0; sorter < position_count; ++sorter) {
                        if (geometry.used_sorter[sorter]) {
                            continue;
                        }
                        const int sorter_vertex = waste_count + sorter;
                        if (!insertion_is_planar(edges, old_parent, leaf,
                                                 sorter_vertex, processor)) {
                            continue;
                        }

                        const long long cost =
                            squared_distance(old_parent, sorter_vertex) +
                            squared_distance(sorter_vertex, leaf) +
                            squared_distance(sorter_vertex, processor) +
                            2 * squared_distance(leaf, processor);
                        if (cost < best_cost) {
                            found = true;
                            best_cost = cost;
                            best_leaf = leaf;
                            best_processor = processor;
                            best_sorter = sorter;
                        }
                    }
                }
            }
        }

        if (!found) {
            return geometry;
        }

        const int old_parent = parent[best_leaf];
        const int sorter_vertex = waste_count + best_sorter;
        if (old_parent == inlet_id()) {
            geometry.root = sorter_vertex;
        } else {
            const int parent_sorter = old_parent - waste_count;
            for (int side = 0; side < 2; ++side) {
                if (geometry.child[parent_sorter][side] == best_leaf) {
                    geometry.child[parent_sorter][side] = sorter_vertex;
                }
            }
        }

        geometry.used_sorter[best_sorter] = true;
        geometry.child[best_sorter] = {best_leaf, best_processor};
        parent[sorter_vertex] = old_parent;
        parent[best_leaf] = sorter_vertex;
        parent[best_processor] = sorter_vertex;
        used_processor[best_processor] = true;
        depth[best_processor] = depth[best_leaf] + 1;
        depth[best_leaf] += 1;
    }

    geometry.ok = true;
    return geometry;
}

int count_leaves(int vertex, const Geometry& geometry,
    vector<int>& leaf_count) {
    if (vertex < waste_count) {
        leaf_count[vertex] = 1;
        return 1;
    }
    if (leaf_count[vertex] != -1) {
        return leaf_count[vertex];
    }
    const int sorter = vertex - waste_count;
    leaf_count[vertex] =
        count_leaves(geometry.child[sorter][0], geometry, leaf_count) +
        count_leaves(geometry.child[sorter][1], geometry, leaf_count);
    return leaf_count[vertex];
}

void build_paths(int vertex, const Geometry& geometry,
                 vector<pair<int, int>>& current,
                 vector<vector<pair<int, int>>>& path) {
    if (vertex < waste_count) {
        path[vertex] = current;
        return;
    }
    const int sorter = vertex - waste_count;
    for (int side = 0; side < 2; ++side) {
        current.push_back({sorter, side});
        build_paths(geometry.child[sorter][side], geometry, current, path);
        current.pop_back();
    }
}

double evaluate(const LogicalPlan& plan,
                const vector<vector<pair<int, int>>>& path) {
    double correct_sum = 0.0;
    for (int processor = 0; processor < waste_count; ++processor) {
        const int waste = plan.processor_type[processor];
        double correct_probability = 1.0;
        for (const auto& [sorter, physical_side] : path[processor]) {
            const int kind = plan.sorter_type[sorter];
            const int exit1_side = plan.flipped[sorter] ? 1 : 0;
            const double p = probability_to_exit1[kind][waste];
            correct_probability *=
                (physical_side == exit1_side) ? p : (1.0 - p);
        }
        correct_sum += correct_probability;
    }
    return correct_sum;
}

// Recursively choose a sorter whose probabilities separate the required
// number of waste types toward each physical child.
void greedy_logical_assignment(int vertex, const vector<int>& types,
                               const Geometry& geometry,
                               const vector<int>& leaf_count,
                               LogicalPlan& plan) {
    if (vertex < waste_count) {
        plan.processor_type[vertex] = types[0];
        return;
    }

    const int sorter = vertex - waste_count;
    const int left_size = leaf_count[geometry.child[sorter][0]];
    double best_value = -numeric_limits<double>::infinity();
    int best_kind = 0;
    int best_flip = 0;
    vector<pair<double, int>> best_order;

    for (int kind = 0; kind < sorter_kind_count; ++kind) {
        for (int flip = 0; flip < 2; ++flip) {
            double value = 0.0;
            vector<pair<double, int>> order;
            order.reserve(types.size());
            for (int waste : types) {
                const double p = probability_to_exit1[kind][waste];
                const double left_probability = flip ? 1.0 - p : p;
                const double right_probability = 1.0 - left_probability;
                value += log(right_probability);
                order.push_back(
                    {log(left_probability) - log(right_probability), waste});
            }
            sort(order.begin(), order.end(), greater<pair<double, int>>());
            for (int index = 0; index < left_size; ++index) {
                value += order[index].first;
            }
            if (value > best_value) {
                best_value = value;
                best_kind = kind;
                best_flip = flip;
                best_order = move(order);
            }
        }
    }

    plan.sorter_type[sorter] = best_kind;
    plan.flipped[sorter] = static_cast<char>(best_flip);

    vector<int> left_types;
    vector<int> right_types;
    for (int index = 0; index < static_cast<int>(best_order.size()); ++index) {
        if (index < left_size) {
            left_types.push_back(best_order[index].second);
        } else {
            right_types.push_back(best_order[index].second);
        }
    }
    greedy_logical_assignment(geometry.child[sorter][0], left_types, geometry,
                              leaf_count, plan);
    greedy_logical_assignment(geometry.child[sorter][1], right_types, geometry,
                              leaf_count, plan);
}

void improve_sorters(LogicalPlan& plan, const Geometry& geometry,
                     const vector<vector<pair<int, int>>>& path,
                     chrono::steady_clock::time_point deadline) {
    for (int pass = 0; pass < 4; ++pass) {
        bool changed = false;
        for (int sorter = 0; sorter < position_count; ++sorter) {
            if (!geometry.used_sorter[sorter]) {
                continue;
            }
            const int original_kind = plan.sorter_type[sorter];
            const char original_flip = plan.flipped[sorter];
            int best_kind = original_kind;
            char best_flip = original_flip;
            double best_quality = evaluate(plan, path);

            for (int kind = 0; kind < sorter_kind_count; ++kind) {
                for (int flip = 0; flip < 2; ++flip) {
                    plan.sorter_type[sorter] = kind;
                    plan.flipped[sorter] = static_cast<char>(flip);
                    const double quality = evaluate(plan, path);
                    if (quality > best_quality + 1e-14) {
                        best_quality = quality;
                        best_kind = kind;
                        best_flip = static_cast<char>(flip);
                    }
                }
            }
            plan.sorter_type[sorter] = best_kind;
            plan.flipped[sorter] = best_flip;
            changed = changed || best_kind != original_kind ||
                      best_flip != original_flip;
        }
        if (!changed || chrono::steady_clock::now() >= deadline) {
            break;
        }
    }
    plan.quality = evaluate(plan, path);
}

bool improve_one_type_swap(LogicalPlan& plan,
                           const vector<vector<pair<int, int>>>& path) {
    double best_quality = plan.quality;
    int best_a = -1;
    int best_b = -1;

    for (int a = 0; a < waste_count; ++a) {
        for (int b = a + 1; b < waste_count; ++b) {
            swap(plan.processor_type[a], plan.processor_type[b]);
            const double quality = evaluate(plan, path);
            swap(plan.processor_type[a], plan.processor_type[b]);
            if (quality > best_quality + 1e-14) {
                best_quality = quality;
                best_a = a;
                best_b = b;
            }
        }
    }

    if (best_a == -1) {
        return false;
    }
    swap(plan.processor_type[best_a], plan.processor_type[best_b]);
    plan.quality = best_quality;
    return true;
}

void local_improvement(LogicalPlan& plan, const Geometry& geometry,
                       const vector<vector<pair<int, int>>>& path,
                       chrono::steady_clock::time_point deadline,
                       int maximum_swaps) {
    improve_sorters(plan, geometry, path, deadline);
    for (int iteration = 0; iteration < maximum_swaps; ++iteration) {
        if (chrono::steady_clock::now() >= deadline ||
            !improve_one_type_swap(plan, path)) {
            break;
        }
        improve_sorters(plan, geometry, path, deadline);
    }
    plan.quality = evaluate(plan, path);
}

LogicalPlan optimize_logical_plan(const Geometry& geometry,
                                  chrono::steady_clock::time_point deadline) {
    vector<int> leaf_count(waste_count + position_count, -1);
    count_leaves(geometry.root, geometry, leaf_count);

    vector<vector<pair<int, int>>> path(waste_count);
    vector<pair<int, int>> current_path;
    build_paths(geometry.root, geometry, current_path, path);

    LogicalPlan best;
    best.processor_type.assign(waste_count, -1);
    best.sorter_type.assign(position_count, 0);
    best.flipped.assign(position_count, false);

    vector<int> all_types(waste_count);
    iota(all_types.begin(), all_types.end(), 0);
    greedy_logical_assignment(geometry.root, all_types, geometry, leaf_count,
                              best);
    best.quality = evaluate(best, path);
    local_improvement(best, geometry, path, deadline, 30);

    // Iterated local search: perturb the best leaf assignment, then repair all
    // sorter choices and climb by the best processor-type swap.
    while (chrono::steady_clock::now() < deadline) {
        LogicalPlan candidate = best;
        const int kick_count = 2 + random_int(4);
        for (int kick = 0; kick < kick_count; ++kick) {
            int a = random_int(waste_count);
            int b = random_int(waste_count - 1);
            if (b >= a) {
                ++b;
            }
            swap(candidate.processor_type[a], candidate.processor_type[b]);
        }
        candidate.quality = evaluate(candidate, path);
        local_improvement(candidate, geometry, path, deadline, 12);
        if (candidate.quality > best.quality) {
            best = move(candidate);
        }
    }
    return best;
}

void print_baseline() {
    for (int processor = 0; processor < waste_count; ++processor) {
        if (processor != 0) {
            cout << ' ';
        }
        cout << processor;
    }
    cout << '\n' << 0 << '\n';
    for (int sorter = 0; sorter < position_count; ++sorter) {
        cout << -1 << '\n';
    }
}

void print_plan(const Geometry& geometry, const LogicalPlan& plan) {
    for (int processor = 0; processor < waste_count; ++processor) {
        if (processor != 0) {
            cout << ' ';
        }
        cout << plan.processor_type[processor];
    }
    cout << '\n' << geometry.root << '\n';

    for (int sorter = 0; sorter < position_count; ++sorter) {
        if (!geometry.used_sorter[sorter]) {
            cout << -1 << '\n';
            continue;
        }
        int exit1 = geometry.child[sorter][0];
        int exit2 = geometry.child[sorter][1];
        if (plan.flipped[sorter]) {
            swap(exit1, exit2);
        }
        cout << plan.sorter_type[sorter] << ' ' << exit1 << ' ' << exit2
             << '\n';
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    const auto start_time = chrono::steady_clock::now();
    cin >> waste_count >> position_count >> sorter_kind_count;
    position_of.resize(waste_count + position_count);
    for (Point& point : position_of) {
        cin >> point.x >> point.y;
    }
    probability_to_exit1.assign(sorter_kind_count,
                                vector<double>(waste_count));
    for (auto& row : probability_to_exit1) {
        for (double& probability : row) {
            cin >> probability;
        }
    }

#ifdef SIMPLE_BASELINE
    print_baseline();
    return 0;
#endif

    Geometry geometry = build_geometry();
    if (!geometry.ok) {
        print_baseline();
        return 0;
    }

    // Keep margin for output and for the slower, heavily loaded system test.
    const auto deadline = start_time + chrono::milliseconds(1600);
    LogicalPlan plan = optimize_logical_plan(geometry, deadline);
    print_plan(geometry, plan);
    return 0;
}
