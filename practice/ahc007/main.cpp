#include <bits/stdc++.h>
using namespace std;

// library/dsu.hpp
struct Dsu {
  vector<int> parent_or_size;
  int components;

  explicit Dsu(int n) : parent_or_size(n, -1), components(n) {
    assert(n >= 0);
  }

  int leader(int vertex) {
    assert(0 <= vertex && vertex < static_cast<int>(parent_or_size.size()));
    if (parent_or_size[vertex] < 0) return vertex;
    return parent_or_size[vertex] = leader(parent_or_size[vertex]);
  }

  bool unite(int a, int b) {
    a = leader(a);
    b = leader(b);
    if (a == b) return false;
    if (-parent_or_size[a] < -parent_or_size[b]) swap(a, b);
    parent_or_size[a] += parent_or_size[b];
    parent_or_size[b] = a;
    --components;
    return true;
  }

  bool same(int a, int b) { return leader(a) == leader(b); }

  int size(int vertex) { return -parent_or_size[leader(vertex)]; }

  int component_count() const { return components; }

  vector<vector<int>> groups() {
    const int n = static_cast<int>(parent_or_size.size());
    vector<int> group_index(n, -1);
    vector<vector<int>> result;
    for (int vertex = 0; vertex < n; ++vertex) {
      const int root = leader(vertex);
      if (group_index[root] == -1) {
        group_index[root] = static_cast<int>(result.size());
        result.push_back({});
      }
      result[group_index[root]].push_back(vertex);
    }
    return result;
  }
};

constexpr int N = 400;
constexpr int M = 1995;

struct Edge {
  int from;
  int to;
  int estimated_length;
};

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  array<int, N> x;
  array<int, N> y;
  for (int vertex = 0; vertex < N; ++vertex) {
    cin >> x[vertex] >> y[vertex];
  }

  array<Edge, M> edges;
  for (Edge& edge : edges) cin >> edge.from >> edge.to;

  // 真の長さは [d, 3d] の一様分布なので、期待値は 2d。
  for (Edge& edge : edges) {
    const long long dx = x[edge.from] - x[edge.to];
    const long long dy = y[edge.from] - y[edge.to];
    const int geometric_length =
        static_cast<int>(llround(sqrt(static_cast<double>(dx * dx + dy * dy))));
    edge.estimated_length = 2 * geometric_length;
  }

  vector<int> future_order(M);
  iota(future_order.begin(), future_order.end(), 0);
  sort(future_order.begin(), future_order.end(), [&](int a, int b) {
    return edges[a].estimated_length < edges[b].estimated_length;
  });

  Dsu selected_edges(N);

  for (int current = 0; current < M; ++current) {
    int true_length;
    cin >> true_length;

    const Edge& edge = edges[current];
    bool take = false;

    if (!selected_edges.same(edge.from, edge.to)) {
      // 現在辺を使わず、未来辺を推定長の短い順に足していく。
      // 両端が初めてつながった時の長さを「代わりの辺の価格」とみなす。
      Dsu future = selected_edges;
      int replacement_cost = numeric_limits<int>::max();

      for (int edge_id : future_order) {
        if (edge_id <= current) continue;
        const Edge& future_edge = edges[edge_id];
        future.unite(future_edge.from, future_edge.to);
        if (future.same(edge.from, edge.to)) {
          replacement_cost = future_edge.estimated_length;
          break;
        }
      }

      // 代替経路より現在辺が安い、または代替経路が存在しないなら採用する。
      take = true_length <= replacement_cost;
    }

    if (take) selected_edges.unite(edge.from, edge.to);
    cout << (take ? 1 : 0) << endl;  // endlで対話出力をflushする
  }

  assert(selected_edges.component_count() == 1);
}
