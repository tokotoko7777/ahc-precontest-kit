#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cmath>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

#include "library/batched-timer.hpp"
#include "library/axis-aligned-rectangle.hpp"
#include "library/best-keeper.hpp"
#include "library/best-by-key.hpp"
#include "library/chmin-chmax.hpp"
#include "library/coordinate-compression.hpp"
#include "library/cumulative-sum-2d.hpp"
#include "library/cumulative-sum.hpp"
#include "library/dsu.hpp"
#include "library/dijkstra.hpp"
#include "library/difference-array.hpp"
#include "library/fenwick-tree.hpp"
#include "library/fast-io.hpp"
#include "library/flat-grid.hpp"
#include "library/floyd-warshall.hpp"
#include "library/fixed-vector.hpp"
#include "library/graph-bfs.hpp"
#include "library/grid-bfs.hpp"
#include "library/kruskal.hpp"
#include "library/longest-increasing-subsequence.hpp"
#include "library/lowest-common-ancestor.hpp"
#include "library/move-statistics.hpp"
#include "library/multi-start.hpp"
#include "library/prime-table.hpp"
#include "library/random.hpp"
#include "library/radix-heap.hpp"
#include "library/range-add-range-sum.hpp"
#include "library/rollback-array.hpp"
#include "library/rollback-dsu.hpp"
#include "library/route-utils.hpp"
#include "library/rolling-hash.hpp"
#include "library/schedule.hpp"
#include "library/segment-tree.hpp"
#include "library/shared-history.hpp"
#include "library/simple-beam-search.hpp"
#include "library/simulated-annealing.hpp"
#include "library/sliding-window-minimum.hpp"
#include "library/sparse-table.hpp"
#include "library/stamp-array.hpp"
#include "library/static-mod-int.hpp"
#include "library/strongly-connected-components.hpp"
#include "library/time-based-simulated-annealing.hpp"
#include "library/timer.hpp"
#include "library/top-k.hpp"
#include "library/topological-sort.hpp"
#include "library/tree-beam-search.hpp"
#include "library/zobrist-hash.hpp"
#include "library/zero-one-bfs.hpp"
#include "library/z-algorithm.hpp"

struct BeamTestState {
  int position = 0;
  std::vector<int> actions;
};

struct TreeBeamTestState {
  int position = 0;
};

int main() {
  Random first(42);
  Random second(42);
  for (int i = 0; i < 1000; ++i) {
    assert(first.next_u64() == second.next_u64());
  }

  Random random(123);
  for (int i = 0; i < 1000; ++i) {
    const int value = random.next_int(-7, 13);
    assert(-7 <= value && value < 13);
    const long long large = random.next_int(0LL, 1LL << 40);
    assert(0 <= large && large < (1LL << 40));
    const double unit = random.next_real();
    assert(0.0 <= unit && unit < 1.0);
  }

  std::vector<int> values{0, 1, 2, 3, 4, 5, 6, 7};
  random.shuffle(values);
  std::sort(values.begin(), values.end());
  for (int i = 0; i < 8; ++i) assert(values[i] == i);
  assert(0 <= random.choice(values) && random.choice(values) < 8);
  assert(random.weighted_index(std::vector<int>{0, 0, 10}) == 2);

  Timer timer;
  assert(timer.elapsed_ms() >= 0.0);
  assert(0.0 <= timer.remaining_ms(1000.0));
  assert(0.0 <= timer.progress(1000.0));
  assert(timer.progress(0.0) == 1.0);

  SimulatedAnnealing annealing(100.0, 1.0, 987654321);
  assert(std::abs(annealing.temperature(0.0) - 100.0) < 1e-12);
  assert(std::abs(annealing.temperature(1.0) - 1.0) < 1e-12);
  assert(annealing.accept(0.0, 0.5));
  assert(annealing.accept(1.0, 0.5));

  SimulatedAnnealing annealing_first(10.0, 0.1, 1234);
  SimulatedAnnealing annealing_second(10.0, 0.1, 1234);
  for (int i = 0; i < 1000; ++i) {
    assert(annealing_first.accept(-1.0, 0.5) ==
           annealing_second.accept(-1.0, 0.5));
  }

  TimeBasedSimulatedAnnealing timed(1000.0, 100.0, 1.0, 4321);
  assert(!timed.is_over());
  assert(std::abs(timed.temperature(0.0) - 100.0) < 1e-12);
  assert(std::abs(timed.temperature(1.0) - 1.0) < 1e-12);
  assert(timed.accept(1LL));

  int small = 10;
  assert(chmin(small, 7));
  assert(small == 7);
  assert(!chmin(small, 8));
  assert(chmax(small, 12));
  assert(small == 12);

  assert(linear_schedule(100, 0, 0.25) == 75);
  assert(power_schedule(100, 0, 0.5, 2.0) == 75);
  assert(std::abs(geometric_schedule(100.0, 1.0, 0.5) - 10.0) <
         1e-12);

  BestKeeper<int, std::string> best(10, "ten");
  assert(!best.update(9, "nine"));
  assert(best.update(12, "twelve"));
  assert(best.best_score == 12 && best.best_state == "twelve");

  BestKeeper<double, int> minimum(10.0, 10, false);
  assert(minimum.update(5.0, 5));
  assert(!minimum.update(8.0, 8));

  TopK<int, std::string> top(2);
  top.add(10, "ten");
  top.add(30, "thirty");
  top.add(20, "twenty");
  assert(top.size() == 2);
  assert(top.best_score() == 30);
  assert(top.entries[1].first == 20);
  top.add(5, "discarded");
  assert(top.size() == 2);
  assert(top.entries[0].second == "thirty");

  CumulativeSum<long long> cumulative({3, 1, 4, 1, 5});
  assert(cumulative.query(1, 4) == 6);

  CumulativeSum2D<int> cumulative_2d({{1, 2, 3}, {4, 5, 6}});
  assert(cumulative_2d.query(0, 1, 2, 3) == 16);

  FenwickTree<long long> fenwick({2, 1, 3, 0, 4});
  assert(fenwick.prefix_sum(3) == 6);
  assert(fenwick.query(1, 5) == 8);
  assert(fenwick.lower_bound(1) == 0);
  assert(fenwick.lower_bound(3) == 1);
  assert(fenwick.lower_bound(4) == 2);
  assert(fenwick.lower_bound(7) == 4);
  assert(fenwick.lower_bound(11) == 5);
  fenwick.add(3, 5);
  assert(fenwick.query(2, 4) == 8);

  auto segment_sum = make_segment_tree(
      std::vector<long long>{2, 1, 3, 0, 4}, 0LL,
      [](const long long& a, const long long& b) { return a + b; });
  assert(segment_sum.query(1, 4) == 4);
  segment_sum.set(3, 5);
  assert(segment_sum.get(3) == 5);
  assert(segment_sum.query(1, 4) == 9);
  assert(segment_sum.all() == 15);

  auto segment_minimum = make_segment_tree(
      std::vector<int>{7, 2, 8, 3}, 1000000000,
      [](const int& a, const int& b) { return std::min(a, b); });
  assert(segment_minimum.query(1, 3) == 2);
  segment_minimum.set(1, 9);
  assert(segment_minimum.query(1, 3) == 8);

  FlatGrid<int> flat_grid(3, 4, -1);
  flat_grid(1, 2) = 7;
  assert(flat_grid.index(1, 2) == 6);
  assert(flat_grid.position(6) == std::make_pair(1, 2));
  assert(flat_grid(1, 2) == 7);
  flat_grid.fill(3);
  assert(std::all_of(flat_grid.begin(), flat_grid.end(),
                     [](int value) { return value == 3; }));

  FlatGrid<std::string> flat_strings(
      std::vector<std::vector<std::string>>{{"a", "b"}, {"c", "d"}});
  assert(flat_strings(1, 0) == "c");

  RadixHeap<std::string> radix_heap;
  radix_heap.push(5, "five");
  radix_heap.push(1, "one");
  radix_heap.push(9, "nine");
  auto radix_entry = radix_heap.pop();
  assert(radix_entry.first == 1 && radix_entry.second == "one");
  radix_heap.push(2, "two");
  std::uint64_t previous_key = 1;
  while (!radix_heap.empty()) {
    radix_entry = radix_heap.pop();
    assert(previous_key <= radix_entry.first);
    previous_key = radix_entry.first;
  }
  radix_heap.clear();
  assert(radix_heap.empty() && radix_heap.last() == 0);
  std::vector<std::uint64_t> radix_keys;
  for (int i = 0; i < 1000; ++i) {
    const auto key = static_cast<std::uint64_t>(random.next_int(0, 1000000));
    radix_keys.push_back(key);
    radix_heap.push(key, std::to_string(i));
  }
  std::sort(radix_keys.begin(), radix_keys.end());
  for (std::uint64_t expected_key : radix_keys) {
    assert(radix_heap.pop().first == expected_key);
  }

  std::FILE* io_file = std::tmpfile();
  assert(io_file != nullptr);
  {
    FastOutput output(io_file);
    output.write_integer(std::numeric_limits<long long>::min(), ' ');
    output.write_integer(0, ' ');
    output.write_string("hello Z 42\n");
    output.flush();
  }
  std::rewind(io_file);
  {
    FastInput input(io_file);
    assert(input.next<long long>() == std::numeric_limits<long long>::min());
    assert(input.next<int>() == 0);
    assert(input.next<std::string>() == "hello");
    assert(input.next<char>() == 'Z');
    assert(input.next<unsigned>() == 42U);
    int after_end = 0;
    assert(!input.read(after_end));
  }
  std::fclose(io_file);

  std::vector<long long> coordinates{100, 20, 100, 50};
  CoordinateCompression<long long> compression(coordinates);
  assert(compression.size() == 3);
  assert(compression.index(50) == 1);
  assert(compression.value(2) == 100);
  assert(compression.compress(coordinates) ==
         std::vector<int>({2, 0, 2, 1}));

  Dsu dsu(5);
  assert(dsu.component_count() == 5);
  assert(dsu.unite(0, 1));
  assert(dsu.unite(1, 2));
  assert(dsu.same(0, 2));
  assert(dsu.size(1) == 3);
  assert(dsu.component_count() == 3);
  assert(dsu.groups().size() == 3);

  std::vector<std::vector<std::pair<int, long long>>> graph(6);
  add_undirected_edge(graph, 0, 1, 4LL);
  add_undirected_edge(graph, 0, 2, 1LL);
  add_undirected_edge(graph, 2, 1, 2LL);
  add_undirected_edge(graph, 1, 3, 1LL);
  add_undirected_edge(graph, 2, 3, 5LL);
  add_undirected_edge(graph, 3, 4, 3LL);
  const auto shortest = dijkstra(graph, 0, (1LL << 60));
  assert(shortest.distance[4] == 7);
  assert(shortest.path_to(4) == std::vector<int>({0, 2, 1, 3, 4}));
  assert(!shortest.reachable(5));
  assert(shortest.path_to(5).empty());

  const std::vector<std::string> grid{"..#.", "....", "##.."};
  const auto grid_shortest = grid_bfs(grid, {0, 0});
  assert(grid_shortest.distance[2][3] == 5);
  assert(grid_shortest.distance[0][2] == -1);
  const auto grid_path = grid_shortest.path_to({2, 3});
  assert(grid_path.size() == 6);
  assert(grid_path.front().first == 0 && grid_path.front().second == 0);
  assert(grid_path.back().first == 2 && grid_path.back().second == 3);

  const std::vector<std::vector<int>> unweighted_graph{
      {1, 2}, {0, 3}, {0, 3}, {1, 2, 4}, {3}, {}};
  const auto bfs_result = graph_bfs(unweighted_graph, 0);
  assert(bfs_result.distance[4] == 3);
  assert(bfs_result.path_to(4) == std::vector<int>({0, 1, 3, 4}));
  assert(!bfs_result.reachable(5));

  std::vector<std::vector<std::pair<int, int>>> zero_one_graph(5);
  zero_one_graph[0] = {{1, 1}, {2, 0}};
  zero_one_graph[2] = {{1, 0}, {3, 1}};
  zero_one_graph[1] = {{3, 0}};
  zero_one_graph[3] = {{4, 1}};
  const auto zero_one_result = zero_one_bfs(zero_one_graph, 0);
  assert(zero_one_result.distance == std::vector<int>({0, 0, 0, 0, 1}));
  assert(zero_one_result.path_to(4) ==
         std::vector<int>({0, 2, 1, 3, 4}));

  const std::vector<std::vector<int>> dag{{1, 2}, {3}, {3}, {}};
  const auto topological = topological_sort(dag);
  assert(topological.is_dag());
  std::vector<int> topological_position(4);
  for (int i = 0; i < 4; ++i) {
    topological_position[topological.order[i]] = i;
  }
  for (int from = 0; from < 4; ++from) {
    for (int to : dag[from]) {
      assert(topological_position[from] < topological_position[to]);
    }
  }
  assert(topological_sort(std::vector<std::vector<int>>{{1}, {0}})
             .has_cycle);

  const std::vector<std::vector<int>> directed_graph{
      {1}, {0, 2}, {3}, {2, 4}, {}};
  const auto components = strongly_connected_components(directed_graph);
  assert(components.component_count() == 3);
  assert(components.same(0, 1));
  assert(components.same(2, 3));
  assert(!components.same(1, 2));
  assert(components.component_id[0] < components.component_id[2]);
  assert(components.component_id[2] < components.component_id[4]);

  const long long floyd_infinity = (1LL << 60);
  std::vector<std::vector<long long>> all_pairs(
      3, std::vector<long long>(3, floyd_infinity));
  for (int i = 0; i < 3; ++i) all_pairs[i][i] = 0;
  all_pairs[0][1] = 3;
  all_pairs[1][2] = -2;
  all_pairs[0][2] = 10;
  floyd_warshall(all_pairs, floyd_infinity);
  assert(all_pairs[0][2] == 1);
  assert(!has_negative_cycle(all_pairs));

  std::vector<std::vector<int>> negative_cycle{{0, -1}, {-1, 0}};
  floyd_warshall(negative_cycle, 1000000000);
  assert(has_negative_cycle(negative_cycle));

  const auto minimum_tree = kruskal<long long>(
      4, {{0, 1, 4}, {0, 2, 1}, {1, 2, 2}, {1, 3, 5}, {2, 3, 3}});
  assert(minimum_tree.connected());
  assert(minimum_tree.total_cost == 6);
  assert(minimum_tree.edges.size() == 3);

  const auto minimum_forest =
      kruskal<int>(4, {{0, 1, 1}, {2, 3, 2}});
  assert(!minimum_forest.connected());
  assert(minimum_forest.component_count == 2);

  using Mint11 = StaticModInt<11>;
  assert((Mint11(10) + Mint11(3)).value() == 2);
  assert((Mint11(10) - Mint11(3)).value() == 7);
  assert((Mint11(10) * Mint11(3)).value() == 8);
  assert(Mint11(3).inverse().value() == 4);
  assert((Mint11(10) / Mint11(3)).value() == 7);
  assert(Mint11(2).pow(10).value() == 1);
  assert(Mint11::raw(5).value() == 5);
  using LargeMod = StaticModInt<2000000000>;
  assert((LargeMod(1999999999) + LargeMod(1999999999)).value() ==
         1999999998);

  const RollingHash text_hash("abracadabra");
  assert(text_hash.hash(0, 4) == text_hash.hash(7, 11));
  assert(text_hash.concatenate(text_hash.hash(0, 4),
                               text_hash.hash(4, 7), 3) ==
         text_hash.hash(0, 7));
  const RollingHash other_hash("abrxxxx");
  assert(text_hash.longest_common_prefix(0, text_hash.size(), other_hash, 0,
                                         other_hash.size()) == 3);

  DifferenceArray<int> difference_array(std::vector<int>{1, 2, 3, 4});
  difference_array.add(1, 3, 5);
  assert(difference_array.build() == std::vector<int>({1, 7, 8, 4}));
  difference_array.clear();
  assert(difference_array.build() == std::vector<int>({0, 0, 0, 0}));

  auto sparse_minimum = make_sparse_table(
      std::vector<int>{5, 2, 7, 1, 3},
      [](int a, int b) { return std::min(a, b); });
  assert(sparse_minimum.query(0, 3) == 2);
  assert(sparse_minimum.query(2, 5) == 1);
  auto sparse_gcd = make_sparse_table(
      std::vector<int>{12, 18, 24, 9},
      [](int a, int b) { return std::gcd(a, b); });
  assert(sparse_gcd.query(0, 3) == 6);
  assert(sparse_gcd.query(1, 4) == 3);

  const std::vector<int> window_values{4, 2, 2, 5, 1};
  assert(sliding_window_minimum(window_values, 3) ==
         std::vector<int>({2, 2, 1}));
  assert(sliding_window_maximum(window_values, 3) ==
         std::vector<int>({4, 5, 5}));

  RangeAddRangeSum<long long> lazy_sum(
      std::vector<long long>{1, 2, 3, 4, 5});
  lazy_sum.add(1, 4, 10);
  assert(lazy_sum.query(0, 5) == 45);
  assert(lazy_sum.query(2, 4) == 27);
  lazy_sum.add(0, 5, -1);
  assert(lazy_sum.all() == 40);

  const std::vector<std::vector<int>> test_tree{
      {1, 2}, {0, 3, 4}, {0, 5}, {1}, {1}, {2, 6}, {5}};
  const LowestCommonAncestor lca(test_tree);
  assert(lca.lca(3, 4) == 1);
  assert(lca.lca(3, 6) == 0);
  assert(lca.distance(3, 6) == 5);
  assert(lca.jump(3, 6, 0) == 3);
  assert(lca.jump(3, 6, 2) == 0);
  assert(lca.jump(3, 6, 5) == 6);

  const std::vector<int> lis_values{3, 1, 2, 2, 4};
  const auto strict_lis = longest_increasing_subsequence(lis_values);
  const auto non_strict_lis =
      longest_increasing_subsequence(lis_values, false);
  assert(strict_lis.length() == 3);
  assert(strict_lis.values == std::vector<int>({1, 2, 4}));
  assert(non_strict_lis.length() == 4);
  assert(non_strict_lis.values == std::vector<int>({1, 2, 2, 4}));

  assert(z_algorithm(std::string("aabcaabxaaaz")) ==
         std::vector<int>({12, 1, 0, 0, 3, 1, 0, 0, 2, 2, 1, 0}));

  const PrimeTable prime_table(100);
  assert(prime_table.is_prime(97));
  assert(!prime_table.is_prime(1));
  assert((prime_table.factorize(84) ==
          std::vector<std::pair<int, int>>({{2, 2}, {3, 1}, {7, 1}})));
  assert(prime_table.divisors(12) ==
         std::vector<int>({1, 2, 3, 4, 6, 12}));

  for (int iteration = 0; iteration < 100; ++iteration) {
    const int n = random.next_int(1, 9);
    std::vector<std::vector<std::pair<int, int>>> random_zero_one(n);
    std::vector<std::vector<std::pair<int, int>>> random_weighted(n);
    std::vector<std::vector<int>> random_directed(n);
    std::vector<std::vector<char>> reachable(
        n, std::vector<char>(n, false));
    for (int vertex = 0; vertex < n; ++vertex) reachable[vertex][vertex] = true;

    for (int from = 0; from < n; ++from) {
      for (int to = 0; to < n; ++to) {
        if (random.next_int(0, 4) != 0) continue;
        const int cost = random.next_int(0, 2);
        random_zero_one[from].push_back({to, cost});
        random_weighted[from].push_back({to, cost});
        random_directed[from].push_back(to);
        reachable[from][to] = true;
      }
    }

    const int start = random.next_int(0, n);
    const auto deque_shortest = zero_one_bfs(random_zero_one, start);
    const auto heap_shortest = dijkstra(random_weighted, start, 1000000000);
    for (int vertex = 0; vertex < n; ++vertex) {
      const int deque_distance = deque_shortest.distance[vertex];
      const int heap_distance = heap_shortest.distance[vertex];
      assert((deque_distance == -1 && heap_distance == 1000000000) ||
             deque_distance == heap_distance);
    }

    for (int middle = 0; middle < n; ++middle) {
      for (int from = 0; from < n; ++from) {
        for (int to = 0; to < n; ++to) {
          reachable[from][to] =
              reachable[from][to] ||
              (reachable[from][middle] && reachable[middle][to]);
        }
      }
    }
    const auto random_components =
        strongly_connected_components(random_directed);
    for (int a = 0; a < n; ++a) {
      for (int b = 0; b < n; ++b) {
        assert(random_components.same(a, b) ==
               (reachable[a][b] && reachable[b][a]));
      }
    }
  }

  for (int iteration = 0; iteration < 100; ++iteration) {
    const int n = random.next_int(1, 25);
    std::vector<long long> naive_values(n);
    for (long long& value : naive_values) value = random.next_int(-20, 21);
    RangeAddRangeSum<long long> tested_sum(naive_values);

    for (int operation = 0; operation < 100; ++operation) {
      int left = random.next_int(0, n + 1);
      int right = random.next_int(0, n + 1);
      if (left > right) std::swap(left, right);
      if (random.next_int(0, 2) == 0) {
        const long long added = random.next_int(-10, 11);
        tested_sum.add(left, right, added);
        for (int i = left; i < right; ++i) naive_values[i] += added;
      } else {
        long long expected = 0;
        for (int i = left; i < right; ++i) expected += naive_values[i];
        assert(tested_sum.query(left, right) == expected);
      }
    }

    auto tested_sparse = make_sparse_table(
        naive_values,
        [](long long a, long long b) { return std::min(a, b); });
    for (int left = 0; left < n; ++left) {
      long long expected = naive_values[left];
      for (int right = left + 1; right <= n; ++right) {
        expected = std::min(expected, naive_values[right - 1]);
        assert(tested_sparse.query(left, right) == expected);
      }
    }

    const int width = random.next_int(1, n + 1);
    const auto tested_window = sliding_window_minimum(naive_values, width);
    for (int left = 0; left + width <= n; ++left) {
      const auto expected = *std::min_element(
          naive_values.begin() + left,
          naive_values.begin() + left + width);
      assert(tested_window[left] == expected);
    }

    std::vector<int> small_sequence(n);
    for (int& value : small_sequence) value = random.next_int(0, 8);
    for (bool strict : {false, true}) {
      std::vector<int> dp(n, 1);
      int expected_length = 0;
      for (int i = 0; i < n; ++i) {
        for (int j = 0; j < i; ++j) {
          if (strict ? small_sequence[j] < small_sequence[i]
                     : small_sequence[j] <= small_sequence[i]) {
            dp[i] = std::max(dp[i], dp[j] + 1);
          }
        }
        expected_length = std::max(expected_length, dp[i]);
      }
      const auto tested_lis =
          longest_increasing_subsequence(small_sequence, strict);
      assert(tested_lis.length() == expected_length);
      for (int i = 1; i < tested_lis.length(); ++i) {
        assert(strict ? tested_lis.values[i - 1] < tested_lis.values[i]
                      : tested_lis.values[i - 1] <= tested_lis.values[i]);
        assert(tested_lis.indices[i - 1] < tested_lis.indices[i]);
      }
    }

    const auto tested_z = z_algorithm(small_sequence);
    for (int start = 0; start < n; ++start) {
      int expected = 0;
      while (start + expected < n &&
             small_sequence[expected] == small_sequence[start + expected]) {
        ++expected;
      }
      assert(tested_z[start] == expected);
    }

    std::vector<std::vector<int>> random_tree(n);
    std::vector<int> random_parent(n, 0);
    std::vector<int> random_depth(n, 0);
    for (int vertex = 1; vertex < n; ++vertex) {
      const int parent = random.next_int(0, vertex);
      random_parent[vertex] = parent;
      random_depth[vertex] = random_depth[parent] + 1;
      random_tree[parent].push_back(vertex);
      random_tree[vertex].push_back(parent);
    }
    const LowestCommonAncestor random_lca(random_tree);
    for (int query = 0; query < 100; ++query) {
      const int original_a = random.next_int(0, n);
      const int original_b = random.next_int(0, n);
      int a = original_a;
      int b = original_b;
      while (random_depth[a] > random_depth[b]) a = random_parent[a];
      while (random_depth[b] > random_depth[a]) b = random_parent[b];
      while (a != b) {
        a = random_parent[a];
        b = random_parent[b];
      }
      assert(random_lca.lca(original_a, original_b) == a);
      assert(random_lca.distance(original_a, original_b) ==
             random_depth[original_a] + random_depth[original_b] -
                 2 * random_depth[a]);
    }
  }

  RollbackDsu rollback_dsu(5);
  rollback_dsu.unite(0, 1);
  const int dsu_snapshot = rollback_dsu.snapshot();
  rollback_dsu.unite(1, 2);
  assert(rollback_dsu.same(0, 2));
  rollback_dsu.rollback(dsu_snapshot);
  assert(!rollback_dsu.same(0, 2));

  RollbackArray<std::string> rollback_array({"a", "b", "c"});
  const int array_snapshot = rollback_array.snapshot();
  rollback_array.set(1, "changed");
  rollback_array.set(2, "also changed");
  assert(rollback_array[1] == "changed");
  rollback_array.rollback(array_snapshot);
  assert(rollback_array[1] == "b" && rollback_array[2] == "c");

  StampArray<int> stamped(5, -1);
  assert(stamped.get(2) == -1);
  stamped[2] = 100;
  assert(stamped.get(2) == 100);
  stamped.clear();
  assert(stamped.get(2) == -1);

  const BeamTestState simple_answer = simple_beam_search(
      BeamTestState{}, 3, 2,
      [](const BeamTestState& state) {
        std::vector<BeamTestState> next_states;
        for (int move : {-1, 1}) {
          BeamTestState next = state;
          next.position += move;
          next.actions.push_back(move);
          next_states.push_back(std::move(next));
        }
        return next_states;
      },
      [](const BeamTestState& state) { return state.position; });
  assert(simple_answer.position == 3);
  assert(simple_answer.actions == std::vector<int>({1, 1, 1}));

  SharedHistory<int> history;
  int history_node = history.root();
  history_node = history.add(history_node, 10);
  history_node = history.add(history_node, 20);
  assert(history.restore(history_node) == std::vector<int>({10, 20}));

  BestByKey<int, int, std::string> unique;
  assert(unique.add(1, 10, "old"));
  assert(!unique.add(1, 5, "worse"));
  assert(unique.add(1, 20, "better"));
  assert(unique.size() == 1);
  assert(unique.entries[0].score == 20);
  assert(unique.entries[0].state == "better");

  ZobristHash zobrist(3, 4, 123);
  std::vector<int> hash_state{0, 1, 2};
  std::uint64_t hash = zobrist.build(hash_state);
  zobrist.change(hash, 1, 1, 3);
  hash_state[1] = 3;
  assert(hash == zobrist.build(hash_state));

  FixedVector<std::string, 3> fixed;
  fixed.push_back("a");
  fixed.emplace_back("b");
  assert(fixed.size() == 2);
  assert(fixed[0] == "a" && fixed.back() == "b");
  fixed.pop_back();
  assert(fixed.size() == 1);

  int generated = 0;
  const int multi_start_best = multi_start<int>(
      5, [&]() { return generated++; }, [](int value) { return value; });
  assert(multi_start_best == 4);

  BatchedTimer batched_timer(1000.0, 16);
  assert(!batched_timer.is_over());
  assert(0.0 <= batched_timer.cached_progress());
  assert(batched_timer.cached_progress() <= 1.0);

  MoveStatistics move_statistics;
  move_statistics.add(true, true);
  move_statistics.add(false, false);
  assert(move_statistics.tried == 2);
  assert(move_statistics.accepted == 1);
  assert(move_statistics.improved == 1);
  assert(std::abs(move_statistics.acceptance_rate() - 0.5) < 1e-12);

  TreeBeamSearch<TreeBeamTestState, int, int> tree_beam(
      TreeBeamTestState{}, 0, 2);
  for (int turn = 0; turn < 3; ++turn) {
    const bool advanced = tree_beam.step(
        [](const TreeBeamTestState&) { return std::vector<int>{-1, 1}; },
        [](TreeBeamTestState& state, int move) { state.position += move; },
        [](TreeBeamTestState& state, int move) { state.position -= move; },
        [](const TreeBeamTestState& state) { return state.position; });
    assert(advanced);
  }
  assert(tree_beam.best_score() == 3);
  assert(tree_beam.restore() == std::vector<int>({1, 1, 1}));

  TreeBeamSearch<TreeBeamTestState, int, int> keyed_tree_beam(
      TreeBeamTestState{}, 0, 10);
  for (int turn = 0; turn < 2; ++turn) {
    const bool advanced = keyed_tree_beam.step_with_key(
        [](const TreeBeamTestState&) { return std::vector<int>{-1, 1}; },
        [](TreeBeamTestState& state, int move) { state.position += move; },
        [](TreeBeamTestState& state, int move) { state.position -= move; },
        [](const TreeBeamTestState& state) { return state.position; },
        [](const TreeBeamTestState& state) { return state.position; });
    assert(advanced);
  }
  assert(keyed_tree_beam.beam.size() == 3);

  const std::vector<int> route{0, 3, 7, 10};
  auto line_distance = [](int a, int b) { return std::abs(a - b); };
  assert(route_length(route, line_distance) == 10);
  assert(route_insertion_delta(route, 2, 5, line_distance) == 0);
  assert(route_removal_delta(route, 2, line_distance) == 0);

  const std::vector<int> detour{0, 7, 3, 10};
  const int reverse_delta =
      route_reverse_delta(detour, 1, 2, line_distance);
  std::vector<int> reversed = detour;
  std::reverse(reversed.begin() + 1, reversed.begin() + 3);
  assert(route_length(detour, line_distance) + reverse_delta ==
         route_length(reversed, line_distance));

  const AxisAlignedRectangle<int> rectangle{0, 0, 10, 20};
  const AxisAlignedRectangle<int> touching{10, 5, 15, 15};
  const AxisAlignedRectangle<int> overlapping{9, 5, 15, 15};
  assert(rectangle.is_valid());
  assert(rectangle.width() == 10 && rectangle.height() == 20);
  assert(rectangle.area() == 200);
  assert(rectangle.contains(0, 0));
  assert(!rectangle.contains(10, 0));
  assert(!rectangle.overlaps(touching));
  assert(rectangle.overlaps(overlapping));
}
