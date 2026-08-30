#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cmath>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

#include "library/batched-timer.hpp"
#include "library/axis-aligned-rectangle.hpp"
#include "library/alias-table.hpp"
#include "library/all-pairs-bfs.hpp"
#include "library/bellman-ford.hpp"
#include "library/bipartite-matching.hpp"
#include "library/bipartite-check.hpp"
#include "library/binary-search-answer.hpp"
#include "library/binary-trie.hpp"
#include "library/best-keeper.hpp"
#include "library/best-by-key.hpp"
#include "library/chmin-chmax.hpp"
#include "library/common-scenario-average.hpp"
#include "library/coordinate-compression.hpp"
#include "library/convex-hull.hpp"
#include "library/cumulative-sum-2d.hpp"
#include "library/cumulative-sum.hpp"
#include "library/dense-int-set.hpp"
#include "library/difference-array-2d.hpp"
#include "library/dsu.hpp"
#include "library/extended-gcd.hpp"
#include "library/dijkstra.hpp"
#include "library/difference-array.hpp"
#include "library/fenwick-tree.hpp"
#include "library/fast-io.hpp"
#include "library/farthest-point-sampling.hpp"
#include "library/flat-grid.hpp"
#include "library/floyd-warshall.hpp"
#include "library/floor-sum.hpp"
#include "library/functional-graph.hpp"
#include "library/fixed-vector.hpp"
#include "library/graph-bfs.hpp"
#include "library/greedy-balanced-partition.hpp"
#include "library/grid-bfs.hpp"
#include "library/hungarian.hpp"
#include "library/inversion-count.hpp"
#include "library/integer-square-root.hpp"
#include "library/kruskal.hpp"
#include "library/longest-increasing-subsequence.hpp"
#include "library/lowest-common-ancestor.hpp"
#include "library/max-flow.hpp"
#include "library/manacher.hpp"
#include "library/matrix.hpp"
#include "library/min-cost-flow.hpp"
#include "library/mod-combination.hpp"
#include "library/move-statistics.hpp"
#include "library/multi-start.hpp"
#include "library/prime-table.hpp"
#include "library/prefix-function.hpp"
#include "library/point-2d.hpp"
#include "library/probability-move-dp.hpp"
#include "library/random.hpp"
#include "library/radix-heap.hpp"
#include "library/range-add-range-minimum.hpp"
#include "library/range-add-range-maximum.hpp"
#include "library/range-add-range-sum.hpp"
#include "library/range-assign-range-sum.hpp"
#include "library/rollback-array.hpp"
#include "library/rollback-dsu.hpp"
#include "library/route-utils.hpp"
#include "library/rolling-hash.hpp"
#include "library/schedule.hpp"
#include "library/sequence-overlap.hpp"
#include "library/segment-tree.hpp"
#include "library/segment-intersection.hpp"
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
#include "library/tree-diameter.hpp"
#include "library/two-sat.hpp"
#include "library/weighted-dsu.hpp"
#include "library/xor-basis.hpp"
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

  MaxFlow<int> maximum_flow(4);
  const int first_flow_edge = maximum_flow.add_edge(0, 1, 2);
  maximum_flow.add_edge(0, 2, 1);
  maximum_flow.add_edge(1, 2, 1);
  maximum_flow.add_edge(1, 3, 1);
  maximum_flow.add_edge(2, 3, 2);
  assert(maximum_flow.flow(0, 3) == 3);
  assert(maximum_flow.get_edge(first_flow_edge).flow == 2);
  const auto minimum_cut = maximum_flow.min_cut(0);
  assert(minimum_cut[0] && !minimum_cut[3]);

  BipartiteMatching matching(3, 3);
  matching.add_edge(0, 0);
  matching.add_edge(0, 1);
  matching.add_edge(1, 0);
  matching.add_edge(2, 2);
  const auto matching_result = matching.solve();
  assert(matching_result.size() == 3);
  assert(matching_result.pairs().size() == 3);

  TwoSat two_sat(3);
  two_sat.add_clause(0, true, 1, true);
  two_sat.add_clause(0, false, 2, true);
  two_sat.set_value(1, false);
  assert(two_sat.solve());
  const auto& boolean_answer = two_sat.answer();
  assert(boolean_answer[0] && !boolean_answer[1] && boolean_answer[2]);

  TwoSat impossible_sat(1);
  impossible_sat.set_value(0, true);
  impossible_sat.set_value(0, false);
  assert(!impossible_sat.solve());

  MinCostFlow<int, long long> assignment_flow(6);
  const int assignment_source = 4;
  const int assignment_sink = 5;
  assignment_flow.add_edge(assignment_source, 0, 1, 0);
  assignment_flow.add_edge(assignment_source, 1, 1, 0);
  assignment_flow.add_edge(0, 2, 1, 1);
  assignment_flow.add_edge(0, 3, 1, 2);
  assignment_flow.add_edge(1, 2, 1, 1);
  assignment_flow.add_edge(1, 3, 1, 100);
  assignment_flow.add_edge(2, assignment_sink, 1, 0);
  assignment_flow.add_edge(3, assignment_sink, 1, 0);
  assert(assignment_flow.flow(assignment_source, assignment_sink, 2) ==
         std::make_pair(2, 3LL));

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
    const int n = random.next_int(2, 8);
    const int source = 0;
    const int sink = n - 1;
    MaxFlow<int> tested_flow(n);
    struct FlowInputEdge {
      int from;
      int to;
      int capacity;
    };
    std::vector<FlowInputEdge> flow_edges;
    for (int from = 0; from < n; ++from) {
      for (int to = 0; to < n; ++to) {
        if (from == to || random.next_int(0, 4) != 0) continue;
        const int capacity = random.next_int(0, 6);
        tested_flow.add_edge(from, to, capacity);
        flow_edges.push_back({from, to, capacity});
      }
    }
    const int actual_flow = tested_flow.flow(source, sink);
    int expected_cut = 1000000000;
    for (int mask = 0; mask < (1 << n); ++mask) {
      if (((mask >> source) & 1) == 0 || ((mask >> sink) & 1) != 0) {
        continue;
      }
      int cut = 0;
      for (const auto& edge : flow_edges) {
        if (((mask >> edge.from) & 1) != 0 &&
            ((mask >> edge.to) & 1) == 0) {
          cut += edge.capacity;
        }
      }
      expected_cut = std::min(expected_cut, cut);
    }
    assert(actual_flow == expected_cut);

    const int left_size = random.next_int(1, 7);
    const int right_size = random.next_int(1, 7);
    BipartiteMatching tested_matching(left_size, right_size);
    std::vector<std::vector<int>> matching_edges(left_size);
    for (int left = 0; left < left_size; ++left) {
      for (int right = 0; right < right_size; ++right) {
        if (random.next_int(0, 2) == 0) {
          tested_matching.add_edge(left, right);
          matching_edges[left].push_back(right);
        }
      }
    }
    std::vector<int> matching_dp(1 << right_size, -1000000000);
    matching_dp[0] = 0;
    for (int left = 0; left < left_size; ++left) {
      std::vector<int> next_dp = matching_dp;
      for (int mask = 0; mask < (1 << right_size); ++mask) {
        if (matching_dp[mask] < 0) continue;
        for (int right : matching_edges[left]) {
          if ((mask >> right) & 1) continue;
          next_dp[mask | (1 << right)] =
              std::max(next_dp[mask | (1 << right)], matching_dp[mask] + 1);
        }
      }
      matching_dp.swap(next_dp);
    }
    const int expected_matching =
        *std::max_element(matching_dp.begin(), matching_dp.end());
    assert(tested_matching.solve().size() == expected_matching);

    const int variable_count = random.next_int(1, 7);
    const int clause_count = random.next_int(0, 15);
    std::vector<std::array<int, 4>> clauses;
    TwoSat tested_sat(variable_count);
    for (int clause = 0; clause < clause_count; ++clause) {
      const int a = random.next_int(0, variable_count);
      const int b = random.next_int(0, variable_count);
      const bool value_a = random.next_int(0, 2);
      const bool value_b = random.next_int(0, 2);
      clauses.push_back({a, value_a, b, value_b});
      tested_sat.add_clause(a, value_a, b, value_b);
    }
    bool brute_satisfiable = false;
    for (int mask = 0; mask < (1 << variable_count); ++mask) {
      bool valid = true;
      for (const auto& clause : clauses) {
        valid &= (((mask >> clause[0]) & 1) == clause[1]) ||
                 (((mask >> clause[2]) & 1) == clause[3]);
      }
      brute_satisfiable |= valid;
    }
    assert(tested_sat.solve() == brute_satisfiable);
    if (brute_satisfiable) {
      const auto& answer = tested_sat.answer();
      for (const auto& clause : clauses) {
        assert(answer[clause[0]] == static_cast<bool>(clause[1]) ||
               answer[clause[2]] == static_cast<bool>(clause[3]));
      }
    }

    constexpr int assignment_size = 4;
    std::array<std::array<int, assignment_size>, assignment_size> costs{};
    MinCostFlow<int, long long> tested_cost_flow(assignment_size * 2 + 2);
    const int cost_source = assignment_size * 2;
    const int cost_sink = cost_source + 1;
    for (int left = 0; left < assignment_size; ++left) {
      tested_cost_flow.add_edge(cost_source, left, 1, 0);
      tested_cost_flow.add_edge(assignment_size + left, cost_sink, 1, 0);
      for (int right = 0; right < assignment_size; ++right) {
        costs[left][right] = random.next_int(0, 30);
        tested_cost_flow.add_edge(left, assignment_size + right, 1,
                                  costs[left][right]);
      }
    }
    std::array<int, assignment_size> permutation{0, 1, 2, 3};
    int expected_cost = 1000000000;
    do {
      int cost = 0;
      for (int left = 0; left < assignment_size; ++left) {
        cost += costs[left][permutation[left]];
      }
      expected_cost = std::min(expected_cost, cost);
    } while (std::next_permutation(permutation.begin(), permutation.end()));
    const auto [sent_flow, actual_cost] =
        tested_cost_flow.flow(cost_source, cost_sink, assignment_size);
    assert(sent_flow == assignment_size);
    assert(actual_cost == expected_cost);
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

  RangeAddRangeMinimum<long long> range_minimum(
      std::vector<long long>{5, 2, 7, 1, 4}, (1LL << 60));
  assert(range_minimum.query(0, 5) == 1);
  range_minimum.add(1, 4, 3);
  assert(range_minimum.query(0, 5) == 4);
  assert(range_minimum.query(1, 3) == 5);
  assert(range_minimum.get(3) == 4);

  const std::vector<BellmanFordEdge<long long>> negative_edges{
      {0, 1, 2}, {1, 2, -5}, {0, 2, 10}, {2, 3, 4}};
  const auto negative_shortest =
      bellman_ford(4, negative_edges, 0, (1LL << 60));
  assert(negative_shortest.distance[3] == 1);
  assert(negative_shortest.path_to(3) == std::vector<int>({0, 1, 2, 3}));
  assert(!negative_shortest.affected_by_negative_cycle[3]);

  const std::vector<BellmanFordEdge<long long>> cycle_edges{
      {0, 1, 0}, {1, 2, -2}, {2, 1, 1}, {2, 3, 0}, {4, 4, -1}};
  const auto negative_cycle_result =
      bellman_ford(5, cycle_edges, 0, (1LL << 60));
  assert(!negative_cycle_result.affected_by_negative_cycle[0]);
  assert(negative_cycle_result.affected_by_negative_cycle[1]);
  assert(negative_cycle_result.affected_by_negative_cycle[2]);
  assert(negative_cycle_result.affected_by_negative_cycle[3]);
  assert(!negative_cycle_result.affected_by_negative_cycle[4]);

  std::vector<std::vector<std::pair<int, long long>>> weighted_tree(5);
  const auto add_tree_edge = [&](int a, int b, long long cost) {
    weighted_tree[a].push_back({b, cost});
    weighted_tree[b].push_back({a, cost});
  };
  add_tree_edge(0, 1, 2);
  add_tree_edge(1, 2, 5);
  add_tree_edge(1, 3, 1);
  add_tree_edge(3, 4, 4);
  const auto diameter = tree_diameter(weighted_tree);
  assert(diameter.length == 10);
  assert((diameter.path == std::vector<int>({2, 1, 3, 4}) ||
          diameter.path == std::vector<int>({4, 3, 1, 2})));

  ModCombination<998244353> combinations(30);
  assert(combinations.choose(5, 2) == 10);
  assert(combinations.permutation(5, 2) == 20);
  assert(combinations.multichoose(3, 4) == 15);
  assert(combinations.choose(3, 4) == 0);

  assert(prefix_function(std::string("ababa")) ==
         std::vector<int>({0, 0, 1, 2, 3}));
  assert(find_pattern_occurrences(std::string("ababa"), std::string("aba")) ==
         std::vector<int>({0, 2}));
  assert(find_pattern_occurrences(std::string("abc"), std::string("")) ==
         std::vector<int>({0, 1, 2, 3}));

  const auto palindromes = palindrome_radii(std::string("abacabba"));
  assert(palindromes.is_palindrome(0, 3));
  assert(palindromes.is_palindrome(4, 8));
  assert(!palindromes.is_palindrome(0, 4));
  const auto longest_palindrome = palindromes.longest_interval();
  assert(longest_palindrome.second - longest_palindrome.first == 5);

  for (int iteration = 0; iteration < 100; ++iteration) {
    const int n = random.next_int(1, 21);
    std::vector<long long> plain_values(n);
    for (long long& value : plain_values) value = random.next_int(-20LL, 21LL);
    RangeAddRangeMinimum<long long> tested_minimum(plain_values, (1LL << 60));
    for (int operation = 0; operation < 200; ++operation) {
      int left = random.next_int(0, n + 1);
      int right = random.next_int(0, n + 1);
      if (left > right) std::swap(left, right);
      if (random.next_int(0, 2) == 0) {
        const long long amount = random.next_int(-10LL, 11LL);
        tested_minimum.add(left, right, amount);
        for (int i = left; i < right; ++i) plain_values[i] += amount;
      } else {
        long long expected = (1LL << 60);
        for (int i = left; i < right; ++i) {
          expected = std::min(expected, plain_values[i]);
        }
        assert(tested_minimum.query(left, right) == expected);
      }
    }

    const int text_size = random.next_int(0, 31);
    std::string text(text_size, 'a');
    for (char& character : text) character = random.next_int(0, 3) + 'a';
    const int pattern_size = random.next_int(0, 9);
    std::string pattern(pattern_size, 'a');
    for (char& character : pattern) character = random.next_int(0, 3) + 'a';
    std::vector<int> expected_occurrences;
    for (int start = 0; start + pattern_size <= text_size; ++start) {
      if (text.compare(start, pattern_size, pattern) == 0) {
        expected_occurrences.push_back(start);
      }
    }
    assert(find_pattern_occurrences(text, pattern) == expected_occurrences);

    const auto tested_palindromes = palindrome_radii(text);
    int expected_longest = 0;
    for (int left = 0; left <= text_size; ++left) {
      for (int right = left; right <= text_size; ++right) {
        bool expected = true;
        for (int offset = 0; left + offset < right - offset - 1; ++offset) {
          if (text[left + offset] != text[right - offset - 1]) expected = false;
        }
        assert(tested_palindromes.is_palindrome(left, right) == expected);
        if (expected) expected_longest = std::max(expected_longest, right - left);
      }
    }
    const auto tested_longest = tested_palindromes.longest_interval();
    assert(tested_longest.second - tested_longest.first == expected_longest);
  }

  for (int iteration = 0; iteration < 100; ++iteration) {
    const int n = random.next_int(2, 7);
    const long long infinity = (1LL << 50);
    std::vector<BellmanFordEdge<long long>> edges;
    std::vector<std::vector<long long>> all_distance(
        n, std::vector<long long>(n, infinity));
    for (int vertex = 0; vertex < n; ++vertex) all_distance[vertex][vertex] = 0;
    for (int from = 0; from < n; ++from) {
      for (int to = 0; to < n; ++to) {
        if (from == to || random.next_int(0, 3) != 0) continue;
        const long long cost = random.next_int(-5LL, 8LL);
        edges.push_back({from, to, cost});
        all_distance[from][to] = std::min(all_distance[from][to], cost);
      }
    }
    for (int middle = 0; middle < n; ++middle) {
      for (int from = 0; from < n; ++from) {
        for (int to = 0; to < n; ++to) {
          if (all_distance[from][middle] == infinity ||
              all_distance[middle][to] == infinity) {
            continue;
          }
          all_distance[from][to] =
              std::min(all_distance[from][to],
                       all_distance[from][middle] + all_distance[middle][to]);
        }
      }
    }
    const auto tested = bellman_ford(n, edges, 0, infinity);
    for (int target = 0; target < n; ++target) {
      bool affected = false;
      for (int cycle = 0; cycle < n; ++cycle) {
        if (all_distance[0][cycle] != infinity &&
            all_distance[cycle][cycle] < 0 &&
            all_distance[cycle][target] != infinity) {
          affected = true;
        }
      }
      assert(static_cast<bool>(tested.affected_by_negative_cycle[target]) ==
             affected);
      if (!affected) assert(tested.distance[target] == all_distance[0][target]);
    }
  }

  for (int n = 0; n <= 30; ++n) {
    std::vector<int> row(n + 1, 1);
    for (int k = 1; k < n; ++k) {
      row[k] = combinations.choose(n - 1, k - 1) +
               combinations.choose(n - 1, k);
    }
    for (int k = 0; k <= n; ++k) assert(combinations.choose(n, k) == row[k]);
  }

  for (int iteration = 0; iteration < 100; ++iteration) {
    const int n = random.next_int(1, 15);
    std::vector<std::vector<std::pair<int, long long>>> tree(n);
    for (int vertex = 1; vertex < n; ++vertex) {
      const int parent = random.next_int(0, vertex);
      const long long cost = random.next_int(0LL, 20LL);
      tree[vertex].push_back({parent, cost});
      tree[parent].push_back({vertex, cost});
    }
    long long expected_diameter = 0;
    for (int start = 0; start < n; ++start) {
      std::vector<long long> distance(n, -1);
      distance[start] = 0;
      std::vector<int> stack{start};
      while (!stack.empty()) {
        const int vertex = stack.back();
        stack.pop_back();
        for (const auto& [next, cost] : tree[vertex]) {
          if (distance[next] != -1) continue;
          distance[next] = distance[vertex] + cost;
          stack.push_back(next);
        }
      }
      expected_diameter =
          std::max(expected_diameter,
                   *std::max_element(distance.begin(), distance.end()));
    }
    assert(tree_diameter(tree).length == expected_diameter);
  }

  const Point2D<long long> point_a{1, 2};
  const Point2D<long long> point_b{4, 6};
  assert(point_a + point_b == Point2D<long long>({5, 8}));
  assert(point_b - point_a == Point2D<long long>({3, 4}));
  assert(dot(point_a, point_b) == 16);
  assert(cross(point_a, point_b) == -2);
  assert(squared_distance(point_a, point_b) == 25);
  assert(manhattan_distance(point_a, point_b) == 7);
  assert(orientation(Point2D<long long>{0, 0},
                     Point2D<long long>{2, 0},
                     Point2D<long long>{1, 1}) == 1);

  std::vector<Point2D<long long>> hull_input{{0, 0}, {2, 0}, {2, 2},
                                              {0, 2}, {1, 1}, {0, 0}};
  const auto square_hull = convex_hull(hull_input);
  assert(square_hull == std::vector<Point2D<long long>>(
                            {{0, 0}, {2, 0}, {2, 2}, {0, 2}}));
  const auto collinear_hull = convex_hull(
      std::vector<Point2D<long long>>{{2, 0}, {0, 0}, {1, 0}}, true);
  assert(collinear_hull == std::vector<Point2D<long long>>(
                               {{0, 0}, {1, 0}, {2, 0}}));

  const Point2D<long long> segment_a{0, 0};
  const Point2D<long long> segment_b{4, 4};
  assert(segments_intersect(segment_a, segment_b, {0, 4}, {4, 0}));
  assert(segments_properly_intersect(segment_a, segment_b, {0, 4}, {4, 0}));
  assert(segments_intersect(segment_a, segment_b, {4, 4}, {8, 4}));
  assert(!segments_properly_intersect(segment_a, segment_b, {4, 4}, {8, 4}));
  assert(segments_intersect(segment_a, segment_b, {2, 2}, {6, 6}));
  assert(!segments_intersect(segment_a, segment_b, {5, 5}, {6, 6}));
  assert(point_on_segment(segment_a, segment_b, Point2D<long long>{3, 3}));

  assert(binary_search_first_true<int>(
             0, 101, [](int value) { return value * value >= 100; }) == 10);
  assert(binary_search_last_true<int>(
             0, 101, [](int value) { return value * value <= 100; }) == 10);
  const auto square_root_range = binary_search_real<double>(
      0.0, 2.0, 100, [](double value) { return value * value >= 2.0; });
  assert(square_root_range.first * square_root_range.first <= 2.0);
  assert(square_root_range.second * square_root_range.second >= 2.0);
  assert(square_root_range.second - square_root_range.first < 1e-12);

  DenseIntSet dense_set(10);
  assert(dense_set.insert(3));
  assert(dense_set.insert(7));
  assert(!dense_set.insert(3));
  assert(dense_set.contains(3) && dense_set.contains(7));
  assert(dense_set.erase(3));
  assert(!dense_set.contains(3) && dense_set.size() == 1);
  dense_set.clear();
  assert(dense_set.empty() && !dense_set.contains(7));

  BinaryTrie<unsigned, 8> binary_trie;
  binary_trie.reserve(10);
  binary_trie.insert(5);
  binary_trie.insert(10);
  binary_trie.insert(10);
  binary_trie.insert(240);
  assert(binary_trie.size() == 4 && binary_trie.count(10) == 2);
  assert(binary_trie.minimum_xor_element(7) == 5);
  assert(binary_trie.maximum_xor_element(7) == 240);
  assert(binary_trie.erase(10) && binary_trie.count(10) == 1);

  for (int threshold = 1; threshold <= 100; ++threshold) {
    assert(binary_search_first_true<int>(
               0, 101, [threshold](int value) { return value >= threshold; }) ==
           threshold);
    assert(binary_search_last_true<int>(
               0, 101, [threshold](int value) { return value <= threshold; }) ==
           threshold);
  }

  for (int iteration = 0; iteration < 100; ++iteration) {
    const int point_count = random.next_int(1, 31);
    std::vector<Point2D<long long>> points(point_count);
    for (auto& point : points) {
      point.x = random.next_int(-5LL, 6LL);
      point.y = random.next_int(-5LL, 6LL);
    }
    const auto hull = convex_hull(points);
    assert(!hull.empty());
    for (int i = 0; i < static_cast<int>(hull.size()); ++i) {
      for (int j = i + 1; j < static_cast<int>(hull.size()); ++j) {
        assert(hull[i] != hull[j]);
      }
    }
    if (hull.size() == 2) {
      for (const auto& point : points) {
        assert(cross(hull[0], hull[1], point) == 0);
        assert(std::min(hull[0].x, hull[1].x) <= point.x);
        assert(point.x <= std::max(hull[0].x, hull[1].x));
        assert(std::min(hull[0].y, hull[1].y) <= point.y);
        assert(point.y <= std::max(hull[0].y, hull[1].y));
      }
    } else if (hull.size() >= 3) {
      for (int i = 0; i < static_cast<int>(hull.size()); ++i) {
        const auto& a = hull[i];
        const auto& b = hull[(i + 1) % hull.size()];
        const auto& c = hull[(i + 2) % hull.size()];
        assert(cross(a, b, c) > 0);
        for (const auto& point : points) assert(cross(a, b, point) >= 0);
      }
    }

    constexpr int universe = 100;
    DenseIntSet tested_set(universe);
    std::vector<char> present(universe, false);
    for (int operation = 0; operation < 1000; ++operation) {
      const int value = random.next_int(0, universe);
      const int type = random.next_int(0, 20);
      if (type < 9) {
        assert(tested_set.insert(value) == !present[value]);
        present[value] = true;
      } else if (type < 18) {
        assert(tested_set.erase(value) == static_cast<bool>(present[value]));
        present[value] = false;
      } else {
        tested_set.clear();
        std::fill(present.begin(), present.end(), false);
      }
      int expected_size = 0;
      for (char exists : present) expected_size += exists;
      assert(tested_set.size() == expected_size);
      for (int i = 0; i < universe; ++i) {
        assert(tested_set.contains(i) == static_cast<bool>(present[i]));
      }
      std::vector<char> listed(universe, false);
      for (int i = 0; i < tested_set.size(); ++i) {
        assert(present[tested_set[i]] && !listed[tested_set[i]]);
        listed[tested_set[i]] = true;
      }
    }

    BinaryTrie<unsigned, 8> tested_trie;
    std::vector<int> frequency(256, 0);
    int element_count = 0;
    for (int operation = 0; operation < 1000; ++operation) {
      const unsigned value = random.next_int(0U, 256U);
      const int type = random.next_int(0, 3);
      if (type == 0 || element_count == 0) {
        tested_trie.insert(value);
        ++frequency[value];
        ++element_count;
      } else if (type == 1) {
        const bool existed = frequency[value] > 0;
        assert(tested_trie.erase(value) == existed);
        if (existed) {
          --frequency[value];
          --element_count;
        }
      } else {
        unsigned expected_minimum = 256;
        unsigned expected_maximum = 0;
        for (unsigned candidate = 0; candidate < 256; ++candidate) {
          if (frequency[candidate] == 0) continue;
          expected_minimum = std::min(expected_minimum, value ^ candidate);
          expected_maximum = std::max(expected_maximum, value ^ candidate);
        }
        assert(tested_trie.minimum_xor_value(value) == expected_minimum);
        assert(tested_trie.maximum_xor_value(value) == expected_maximum);
      }
      assert(tested_trie.size() == element_count);
      assert(tested_trie.count(value) == frequency[value]);
    }
  }

  DifferenceArray2D<long long> difference_2d(3, 4);
  difference_2d.add(0, 0, 3, 4, 1);
  difference_2d.add(1, 1, 3, 4, 2);
  assert(difference_2d.build() ==
         std::vector<std::vector<long long>>(
             {{1, 1, 1, 1}, {1, 3, 3, 3}, {1, 3, 3, 3}}));
  assert(difference_2d.build_flat() ==
         std::vector<long long>({1, 1, 1, 1, 1, 3, 3, 3, 1, 3, 3, 3}));

  RangeAssignRangeSum<long long> assign_sum(
      std::vector<long long>{1, 2, 3, 4, 5});
  assign_sum.assign(1, 4, 10);
  assert(assign_sum.query(0, 5) == 36);
  assert(assign_sum.query(2, 4) == 20);
  assign_sum.assign(0, 5, -2);
  assert(assign_sum.all_sum() == -10 && assign_sum.get(3) == -2);

  RangeAddRangeMaximum<long long> range_maximum(
      std::vector<long long>{1, 5, 2, 4}, -(1LL << 60));
  assert(range_maximum.query(0, 4) == 5);
  range_maximum.add(2, 4, 10);
  assert(range_maximum.query(0, 4) == 14);
  assert(range_maximum.query(0, 2) == 5);
  assert(range_maximum.get(2) == 12);

  WeightedDsu<long long> weighted_dsu(4);
  assert(weighted_dsu.unite(0, 1, 3));
  assert(weighted_dsu.unite(1, 2, -2));
  assert(weighted_dsu.same(0, 2));
  assert(weighted_dsu.difference(0, 2) == 1);
  assert(weighted_dsu.unite(0, 2, 1));
  assert(!weighted_dsu.unite(0, 2, 2));
  assert(weighted_dsu.component_count() == 2);

  const auto bipartite_path = bipartite_check(
      std::vector<std::vector<int>>{{1}, {0, 2}, {1, 3}, {2}});
  assert(bipartite_path.bipartite && bipartite_path.component_count == 1);
  for (int vertex = 0; vertex < 3; ++vertex) {
    assert(bipartite_path.color[vertex] != bipartite_path.color[vertex + 1]);
  }
  assert(!bipartite_check(
              std::vector<std::vector<int>>{{1, 2}, {0, 2}, {0, 1}})
              .bipartite);

  FunctionalGraph fixed_functional_graph({1, 2, 3, 2, 5, 4});
  assert(fixed_functional_graph.jump(0, 5) == 3);
  assert(fixed_functional_graph.jump(0, 1000000000000000000ULL) == 2);
  assert(fixed_functional_graph.steps_to_cycle[0] == 2);
  assert(fixed_functional_graph.cycle_entry[0] == 2);
  assert(fixed_functional_graph.cycle_length(0) == 2);
  assert(fixed_functional_graph.cycles.size() == 2);

  for (int iteration = 0; iteration < 100; ++iteration) {
    const int height = random.next_int(1, 11);
    const int width = random.next_int(1, 11);
    DifferenceArray2D<long long> tested_difference(height, width);
    std::vector<std::vector<long long>> expected_grid(
        height, std::vector<long long>(width, 0));
    for (int operation = 0; operation < 100; ++operation) {
      int top = random.next_int(0, height + 1);
      int bottom = random.next_int(0, height + 1);
      int left = random.next_int(0, width + 1);
      int right = random.next_int(0, width + 1);
      if (top > bottom) std::swap(top, bottom);
      if (left > right) std::swap(left, right);
      const long long amount = random.next_int(-10LL, 11LL);
      tested_difference.add(top, left, bottom, right, amount);
      for (int row = top; row < bottom; ++row) {
        for (int column = left; column < right; ++column) {
          expected_grid[row][column] += amount;
        }
      }
    }
    assert(tested_difference.build() == expected_grid);

    const int n = random.next_int(1, 21);
    std::vector<long long> plain_values(n);
    for (long long& value : plain_values) value = random.next_int(-20LL, 21LL);
    RangeAssignRangeSum<long long> tested_assign(plain_values);
    RangeAddRangeMaximum<long long> tested_maximum(plain_values,
                                                    -(1LL << 60));
    std::vector<long long> assigned_values = plain_values;
    std::vector<long long> maximum_values = plain_values;
    for (int operation = 0; operation < 200; ++operation) {
      int left = random.next_int(0, n + 1);
      int right = random.next_int(0, n + 1);
      if (left > right) std::swap(left, right);
      const int type = random.next_int(0, 4);
      if (type == 0) {
        const long long value = random.next_int(-20LL, 21LL);
        tested_assign.assign(left, right, value);
        for (int i = left; i < right; ++i) assigned_values[i] = value;
      } else if (type == 1) {
        const long long amount = random.next_int(-10LL, 11LL);
        tested_maximum.add(left, right, amount);
        for (int i = left; i < right; ++i) maximum_values[i] += amount;
      } else if (type == 2) {
        long long expected = 0;
        for (int i = left; i < right; ++i) expected += assigned_values[i];
        assert(tested_assign.query(left, right) == expected);
      } else {
        long long expected = -(1LL << 60);
        for (int i = left; i < right; ++i) {
          expected = std::max(expected, maximum_values[i]);
        }
        assert(tested_maximum.query(left, right) == expected);
      }
    }
  }

  for (int iteration = 0; iteration < 100; ++iteration) {
    const int n = random.next_int(2, 13);
    WeightedDsu<long long> tested_weighted_dsu(n);
    std::vector<std::vector<std::pair<int, long long>>> constraints(n);
    for (int operation = 0; operation < 200; ++operation) {
      const int a = random.next_int(0, n);
      const int b = random.next_int(0, n);
      const long long requested_difference = random.next_int(-30LL, 31LL);
      std::vector<char> visited(n, false);
      std::vector<long long> potential(n, 0);
      std::vector<int> queue{a};
      visited[a] = true;
      for (int head = 0; head < static_cast<int>(queue.size()); ++head) {
        const int vertex = queue[head];
        for (const auto& [next, difference] : constraints[vertex]) {
          if (visited[next]) continue;
          visited[next] = true;
          potential[next] = potential[vertex] + difference;
          queue.push_back(next);
        }
      }
      const bool expected =
          !visited[b] || potential[b] == requested_difference;
      assert(tested_weighted_dsu.unite(a, b, requested_difference) ==
             expected);
      if (!visited[b]) {
        constraints[a].push_back({b, requested_difference});
        constraints[b].push_back({a, -requested_difference});
      }
      assert(tested_weighted_dsu.same(a, b));
      if (expected) {
        assert(tested_weighted_dsu.difference(a, b) ==
               requested_difference);
      }
    }

    std::vector<std::vector<int>> graph(n);
    for (int a = 0; a < n; ++a) {
      for (int b = a + 1; b < n; ++b) {
        if (random.next_int(0, 3) == 0) {
          graph[a].push_back(b);
          graph[b].push_back(a);
        }
      }
    }
    bool brute_bipartite = false;
    for (int mask = 0; mask < (1 << n); ++mask) {
      bool valid = true;
      for (int a = 0; a < n; ++a) {
        for (int b : graph[a]) {
          if (((mask >> a) & 1) == ((mask >> b) & 1)) valid = false;
        }
      }
      brute_bipartite |= valid;
    }
    const auto tested_bipartite = bipartite_check(graph);
    assert(tested_bipartite.bipartite == brute_bipartite);
    if (brute_bipartite) {
      for (int a = 0; a < n; ++a) {
        for (int b : graph[a]) {
          assert(tested_bipartite.color[a] != tested_bipartite.color[b]);
        }
      }
    }

    std::vector<int> next(n);
    for (int& successor : next) successor = random.next_int(0, n);
    const FunctionalGraph tested_functional(next);
    for (int start = 0; start < n; ++start) {
      std::vector<int> first_visit(n, -1);
      std::vector<int> sequence;
      int vertex = start;
      while (first_visit[vertex] == -1) {
        first_visit[vertex] = static_cast<int>(sequence.size());
        sequence.push_back(vertex);
        vertex = next[vertex];
      }
      const int cycle_start = first_visit[vertex];
      const int cycle_length = static_cast<int>(sequence.size()) - cycle_start;
      assert(tested_functional.steps_to_cycle[start] == cycle_start);
      assert(tested_functional.cycle_entry[start] == sequence[cycle_start]);
      assert(tested_functional.cycle_length(start) == cycle_length);

      for (int query = 0; query < 20; ++query) {
        const unsigned long long steps = random.next_u64();
        int expected_vertex = start;
        unsigned long long remaining = steps;
        if (remaining < static_cast<unsigned long long>(cycle_start)) {
          while (remaining-- > 0) expected_vertex = next[expected_vertex];
        } else {
          for (int i = 0; i < cycle_start; ++i) {
            expected_vertex = next[expected_vertex];
          }
          remaining -= cycle_start;
          remaining %= static_cast<unsigned long long>(cycle_length);
          while (remaining-- > 0) expected_vertex = next[expected_vertex];
        }
        assert(tested_functional.jump(start, steps) == expected_vertex);
      }
    }
  }

  const std::vector<double> alias_weights{0.0, 1.0, 3.0, 6.0};
  const AliasTable alias_table(alias_weights);
  std::vector<double> reconstructed_probability(alias_table.size(), 0.0);
  for (int column = 0; column < alias_table.size(); ++column) {
    reconstructed_probability[column] +=
        alias_table.probability[column] / alias_table.size();
    reconstructed_probability[alias_table.alias[column]] +=
        (1.0 - alias_table.probability[column]) / alias_table.size();
    assert(alias_weights[alias_table.choose(column, 0.0)] > 0.0);
    assert(alias_weights[alias_table.choose(column, 0.999999)] > 0.0);
  }
  for (int i = 0; i < alias_table.size(); ++i) {
    assert(std::abs(reconstructed_probability[i] - alias_weights[i] / 10.0) <
           1e-12);
  }

  XorBasis<unsigned, 8> xor_basis;
  assert(xor_basis.insert(3));
  assert(xor_basis.insert(5));
  assert(!xor_basis.insert(6));
  assert(xor_basis.rank() == 2);
  assert(xor_basis.contains(0) && xor_basis.contains(6));
  assert(!xor_basis.contains(1));
  assert(xor_basis.maximum_xor() == 6);
  assert(xor_basis.maximum_xor(1) == 7);
  assert(xor_basis.minimum_xor(7) == 1);

  const Matrix<long long> fibonacci_matrix(
      std::vector<std::vector<long long>>{{1, 1}, {1, 0}});
  const Matrix<long long> fibonacci_tenth =
      matrix_power(fibonacci_matrix, 10);
  assert(fibonacci_tenth(0, 0) == 89 && fibonacci_tenth(0, 1) == 55);
  assert(fibonacci_tenth(1, 0) == 55 && fibonacci_tenth(1, 1) == 34);
  assert(matrix_power(fibonacci_matrix, 0) == Matrix<long long>::identity(2));

  assert(inversion_count(std::vector<int>{3, 1, 2, 1}) == 4);
  assert(inversion_count(std::vector<int>{1, 1, 1}) == 0);
  assert(inversion_count(std::vector<int>{4, 3, 2, 1}) == 6);

  const auto gcd_result = extended_gcd(30LL, 18LL);
  assert(gcd_result.gcd == 6);
  assert(30 * gcd_result.x + 18 * gcd_result.y == gcd_result.gcd);
  assert(modular_inverse(3LL, 11LL) == std::optional<long long>(4));
  assert(!modular_inverse(6LL, 9LL).has_value());

  assert(floor_sum(4, 10, 6, 3) == 3);
  assert(floor_sum(5, 7, -3, 4) == -4);

  for (int iteration = 0; iteration < 200; ++iteration) {
    const int value_count = random.next_int(0, 11);
    std::vector<unsigned> xor_values(value_count);
    XorBasis<unsigned, 8> tested_basis;
    for (unsigned& value : xor_values) {
      value = random.next_int(0U, 256U);
      tested_basis.insert(value);
    }
    std::vector<char> possible(256, false);
    for (int mask = 0; mask < (1 << value_count); ++mask) {
      unsigned value = 0;
      for (int i = 0; i < value_count; ++i) {
        if ((mask >> i) & 1) value ^= xor_values[i];
      }
      possible[value] = true;
    }
    int expected_rank = 0;
    int possible_count = 0;
    for (char exists : possible) possible_count += exists;
    while ((1 << expected_rank) < possible_count) ++expected_rank;
    assert(tested_basis.rank() == expected_rank);
    for (unsigned value = 0; value < 256; ++value) {
      assert(tested_basis.contains(value) == static_cast<bool>(possible[value]));
    }
    for (int query = 0; query < 20; ++query) {
      const unsigned seed = random.next_int(0U, 256U);
      unsigned expected_minimum = 255;
      unsigned expected_maximum = 0;
      for (unsigned value = 0; value < 256; ++value) {
        if (!possible[value]) continue;
        expected_minimum = std::min(expected_minimum, seed ^ value);
        expected_maximum = std::max(expected_maximum, seed ^ value);
      }
      assert(tested_basis.minimum_xor(seed) == expected_minimum);
      assert(tested_basis.maximum_xor(seed) == expected_maximum);
    }

    const int n = random.next_int(0, 11);
    std::vector<int> inversion_values(n);
    for (int& value : inversion_values) value = random.next_int(-5, 6);
    long long expected_inversions = 0;
    for (int i = 0; i < n; ++i) {
      for (int j = i + 1; j < n; ++j) {
        expected_inversions += inversion_values[i] > inversion_values[j];
      }
    }
    assert(inversion_count(inversion_values) == expected_inversions);

    const int rows = random.next_int(1, 5);
    const int middle = random.next_int(1, 5);
    const int columns = random.next_int(1, 5);
    Matrix<long long> left(rows, middle);
    Matrix<long long> right(middle, columns);
    for (long long& value : left.values) value = random.next_int(-3LL, 4LL);
    for (long long& value : right.values) value = random.next_int(-3LL, 4LL);
    const Matrix<long long> product = left * right;
    for (int row = 0; row < rows; ++row) {
      for (int column = 0; column < columns; ++column) {
        long long expected = 0;
        for (int k = 0; k < middle; ++k) {
          expected += left(row, k) * right(k, column);
        }
        assert(product(row, column) == expected);
      }
    }

    const int weight_count = random.next_int(1, 15);
    std::vector<double> weights(weight_count);
    double weight_sum = 0.0;
    for (double& weight : weights) {
      weight = random.next_int(0, 11);
      weight_sum += weight;
    }
    if (weight_sum == 0.0) {
      weights[0] = 1.0;
      weight_sum = 1.0;
    }
    const AliasTable tested_alias(weights);
    std::vector<double> actual(weight_count, 0.0);
    for (int column = 0; column < weight_count; ++column) {
      actual[column] += tested_alias.probability[column] / weight_count;
      actual[tested_alias.alias[column]] +=
          (1.0 - tested_alias.probability[column]) / weight_count;
    }
    for (int i = 0; i < weight_count; ++i) {
      assert(std::abs(actual[i] - weights[i] / weight_sum) < 1e-10);
    }
  }

  for (int iteration = 0; iteration < 1000; ++iteration) {
    const long long n = random.next_int(0LL, 31LL);
    const long long modulus = random.next_int(1LL, 21LL);
    const long long a = random.next_int(-30LL, 31LL);
    const long long b = random.next_int(-30LL, 31LL);
    long long expected = 0;
    for (long long i = 0; i < n; ++i) {
      const long long numerator = a * i + b;
      long long quotient = numerator / modulus;
      if (numerator % modulus < 0) --quotient;
      expected += quotient;
    }
    assert(floor_sum(n, modulus, a, b) == expected);
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

  assert(suffix_prefix_overlap(std::string("ABCDE"), std::string("CDEFG")) ==
         3);
  assert(suffix_prefix_overlap(std::string("AAAA"), std::string("AA")) == 2);
  assert(suffix_prefix_overlap(std::string("ABC"), std::string("XYZ")) == 0);
  assert(merge_with_overlap(std::string("ABCDE"), std::string("CDEFG")) ==
         "ABCDEFG");
  assert((merge_with_overlap(std::vector<int>{1, 2, 3},
                             std::vector<int>{3, 4}) ==
          std::vector<int>{1, 2, 3, 4}));

  ProbabilityMoveDP<double> probability_dp(3, 0);
  const std::vector<int> move_right{1, 2, 2};
  assert(std::abs(probability_dp.step(move_right, 0.5, 2)) < 1e-12);
  assert(std::abs(probability_dp.step(move_right, 0.5, 2) - 0.25) < 1e-12);
  assert(std::abs(probability_dp.remaining_probability() - 0.75) < 1e-12);
  assert(std::abs(probability_dp.step(move_right, 0.5, 2) - 0.25) < 1e-12);
  assert(std::abs(probability_dp.remaining_probability() - 0.50) < 1e-12);

  ProbabilityMoveDP<float> wall_probability_dp(2, 0);
  const std::vector<int> stay_in_place{0, 1};
  assert(wall_probability_dp.step(stay_in_place, 0.7F) == 0.0F);
  assert(std::abs(wall_probability_dp.probability[0] - 1.0F) < 1e-6F);

  const std::vector<int> scenario_actions{0, 1, 2};
  const std::vector<int> common_scenarios{1, 2, 3};
  const std::vector<long double> scenario_average = common_scenario_average(
      scenario_actions,
      common_scenarios,
      [](int action, int scenario) { return action * scenario; });
  assert(scenario_average[0] == 0.0L);
  assert(scenario_average[1] == 2.0L);
  assert(scenario_average[2] == 4.0L);

  const std::vector<int> sample_positions{0, 2, 9, 10};
  const std::vector<int> spread_samples = farthest_point_sampling(
      static_cast<int>(sample_positions.size()), 3,
      [&](int a, int b) {
        const int difference = sample_positions[a] - sample_positions[b];
        return difference * difference;
      });
  assert(spread_samples == std::vector<int>({0, 3, 1}));

  assert(floor_integer_square_root(0LL) == 0);
  assert(floor_integer_square_root(15LL) == 3);
  assert(floor_integer_square_root(16LL) == 4);
  assert(ceil_integer_square_root(15LL) == 4);
  assert(ceil_integer_square_root(16LL) == 4);
  assert(floor_integer_square_root(std::numeric_limits<long long>::max()) ==
         3037000499LL);
  assert(ceil_integer_square_root(std::numeric_limits<long long>::max()) ==
         3037000500LL);

  const std::vector<long long> partition_weight{9, 8, 7, 6, 5, 4};
  const std::vector<std::vector<int>> balanced_groups =
      greedy_balanced_partition(partition_weight, 3);
  std::vector<long long> partition_sum(3, 0);
  std::vector<int> partition_seen(6, 0);
  for (int group = 0; group < 3; ++group) {
    for (int item : balanced_groups[group]) {
      partition_sum[group] += partition_weight[item];
      ++partition_seen[item];
    }
  }
  assert(partition_sum == std::vector<long long>({13, 13, 13}));
  assert(partition_seen == std::vector<int>({1, 1, 1, 1, 1, 1}));

  const std::vector<std::vector<long long>> assignment_cost{
      {4, 1, 3},
      {2, 0, 5},
      {3, 2, 2},
  };
  assert(hungarian_minimum_assignment(assignment_cost) ==
         std::vector<int>({1, 0, 2}));
  const std::vector<std::vector<double>> negative_assignment_cost{
      {-1.0, 2.0},
      {0.0, -2.0},
  };
  assert(hungarian_minimum_assignment(negative_assignment_cost) ==
         std::vector<int>({0, 1}));

  const std::vector<std::vector<int>> short_path_graph{
      {1},
      {0, 2},
      {1, 3},
      {2},
      {},
  };
  const auto all_distances =
      all_pairs_bfs<unsigned short>(short_path_graph);
  assert(all_distances[0][3] == 3);
  assert(all_distances[3][0] == 3);
  assert(all_distances[0][4] ==
         std::numeric_limits<unsigned short>::max());
}
