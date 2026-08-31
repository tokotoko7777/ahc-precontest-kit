#include <algorithm>
#include <cassert>
#include <functional>
#include <utility>
#include <vector>

struct BridgeTreeResult {
  // is_bridge[edge_id] is true exactly when removing that edge disconnects
  // its original connected component.
  std::vector<char> is_bridge;

  // Vertices joined without crossing a bridge have the same component_id.
  std::vector<int> component_id;
  std::vector<std::vector<int>> groups;

  // tree[component] contains {next_component, original_edge_id}.
  // A disconnected input produces a forest.
  std::vector<std::vector<std::pair<int, int>>> tree;

  int component_count() const { return static_cast<int>(groups.size()); }

  bool same_without_bridges(int a, int b) const {
    assert(0 <= a && a < static_cast<int>(component_id.size()));
    assert(0 <= b && b < static_cast<int>(component_id.size()));
    return component_id[a] == component_id[b];
  }
};

// 無向グラフの橋を求め、橋を渡らず行き来できる頂点をまとめる。
// 計算量は O(N + M)。多重辺と自己ループにも対応する。
//
// 使い方:
// vector<pair<int, int>> edges = {{0, 1}, {1, 2}, {2, 0}, {2, 3}};
// auto result = build_bridge_tree(4, edges);
// if (result.is_bridge[3]) cout << "edge 3 is a bridge\n";
// int component = result.component_id[2];
// for (auto [next, edge_id] : result.tree[component]) { ... }
//
// 分かりやすさを優先して深さ優先探索に再帰を使う。非常に長い一本道を
// 扱う場合は、実行環境の再帰スタック上限に注意する。
inline BridgeTreeResult build_bridge_tree(
    int vertex_count, const std::vector<std::pair<int, int>>& edges) {
  assert(vertex_count >= 0);
  const int edge_count = static_cast<int>(edges.size());
  std::vector<std::vector<std::pair<int, int>>> graph(vertex_count);
  for (int edge_id = 0; edge_id < edge_count; ++edge_id) {
    const auto [a, b] = edges[edge_id];
    assert(0 <= a && a < vertex_count);
    assert(0 <= b && b < vertex_count);
    graph[a].push_back({b, edge_id});
    graph[b].push_back({a, edge_id});
  }

  BridgeTreeResult result;
  result.is_bridge.assign(edge_count, false);
  std::vector<int> visit_order(vertex_count, -1);
  std::vector<int> lowest_order(vertex_count, -1);
  int timer = 0;

  std::function<void(int, int)> find_bridges =
      [&](int vertex, int parent_edge) {
        visit_order[vertex] = lowest_order[vertex] = timer++;
        for (const auto& [next, edge_id] : graph[vertex]) {
          if (edge_id == parent_edge) continue;
          if (visit_order[next] != -1) {
            lowest_order[vertex] =
                std::min(lowest_order[vertex], visit_order[next]);
            continue;
          }
          find_bridges(next, edge_id);
          lowest_order[vertex] =
              std::min(lowest_order[vertex], lowest_order[next]);
          if (visit_order[vertex] < lowest_order[next]) {
            result.is_bridge[edge_id] = true;
          }
        }
      };

  for (int start = 0; start < vertex_count; ++start) {
    if (visit_order[start] == -1) find_bridges(start, -1);
  }

  result.component_id.assign(vertex_count, -1);
  std::vector<int> stack;
  stack.reserve(vertex_count);
  for (int start = 0; start < vertex_count; ++start) {
    if (result.component_id[start] != -1) continue;
    const int component = result.component_count();
    result.groups.push_back({});
    result.component_id[start] = component;
    stack.push_back(start);
    while (!stack.empty()) {
      const int vertex = stack.back();
      stack.pop_back();
      result.groups[component].push_back(vertex);
      for (const auto& [next, edge_id] : graph[vertex]) {
        if (result.is_bridge[edge_id] ||
            result.component_id[next] != -1) {
          continue;
        }
        result.component_id[next] = component;
        stack.push_back(next);
      }
    }
  }

  result.tree.resize(result.component_count());
  for (int edge_id = 0; edge_id < edge_count; ++edge_id) {
    if (!result.is_bridge[edge_id]) continue;
    const auto [a, b] = edges[edge_id];
    const int component_a = result.component_id[a];
    const int component_b = result.component_id[b];
    result.tree[component_a].push_back({component_b, edge_id});
    result.tree[component_b].push_back({component_a, edge_id});
  }
  return result;
}
