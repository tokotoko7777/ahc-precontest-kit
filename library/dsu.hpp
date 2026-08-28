#include <algorithm>
#include <cassert>
#include <vector>

// Union-Find。
// 使い方:
// Dsu dsu(n);
// dsu.unite(a, b);
// if (dsu.same(a, b)) { ... }
struct Dsu {
  std::vector<int> parent_or_size;

  explicit Dsu(int n) : parent_or_size(n, -1) {}

  int leader(int vertex) {
    assert(0 <= vertex && vertex < static_cast<int>(parent_or_size.size()));
    if (parent_or_size[vertex] < 0) return vertex;
    return parent_or_size[vertex] = leader(parent_or_size[vertex]);
  }

  bool unite(int a, int b) {
    a = leader(a);
    b = leader(b);
    if (a == b) return false;
    if (-parent_or_size[a] < -parent_or_size[b]) std::swap(a, b);
    parent_or_size[a] += parent_or_size[b];
    parent_or_size[b] = a;
    return true;
  }

  bool same(int a, int b) { return leader(a) == leader(b); }

  int size(int vertex) { return -parent_or_size[leader(vertex)]; }
};
