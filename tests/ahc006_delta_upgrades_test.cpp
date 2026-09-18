#define AHC006_NO_MAIN
#ifndef AHC006_PRECOMPUTE_DISTANCE
#define AHC006_PRECOMPUTE_DISTANCE 0
#endif
#include "../examples/search/ahc006_sa.cpp"

// 全再計算を独立のoracleにし、境界・隣接・非対称距離・同点も検査する。
void test_route_helpers() {
  mt19937 rng(12345);
  for (int trial = 0; trial < 100; ++trial) {
    const int n = 2 + rng() % 18;
    vector<int> route(n);
    iota(route.begin(), route.end(), 0);
    vector<vector<long long>> costs(n + 2, vector<long long>(n + 2));
    for (auto& row : costs) for (auto& x : row) x = rng() % 30;
    if (trial == 0) for (auto& row : costs) fill(row.begin(), row.end(), 0);
    auto dist = [&](int a, int b) { return costs[a][b]; };
    const auto old_cost = route_length(route, dist);
    for (int a = 1; a + 1 < n; ++a) for (int b = 1; b + 1 < n; ++b) {
      auto changed = route;
      int event = changed[a];
      changed.erase(changed.begin() + a);
      changed.insert(changed.begin() + b, event);
      assert(route_relocate_delta(route, a, b, dist) == route_length(changed, dist) - old_cost);
      changed = route;
      swap(changed[a], changed[b]);
      assert(route_swap_delta(route, a, b, dist) == route_length(changed, dist) - old_cost);
    }
    const auto actual = best_ordered_pair_insertion(route, n, n + 1, dist);
    tuple<long long, int, int> expected{LLONG_MAX, 0, 0};
    for (int a = 1; a < n; ++a) for (int b = a; b < n; ++b) {
      auto changed = route;
      changed.insert(changed.begin() + b, n + 1);
      changed.insert(changed.begin() + a, n);
      expected = min(expected, make_tuple(route_length(changed, dist) - old_cost, a, b));
    }
    assert(make_tuple(actual.delta, actual.first_gap, actual.second_gap) == expected);
  }
  vector<double> points{0, 2, 5, 0};
  auto dist = [](double a, double b) { return abs(a - b); };
  static_assert(is_same_v<decltype(route_swap_delta(points, 1, 2, dist)), double>);
  assert(route_relocate_delta(points, 1, 2, dist) == 0.0);
  assert(route_reverse_delta(points, 1, 2, dist) == 0.0);
  auto pair = best_ordered_pair_insertion(points, 1.0, 3.0, dist);
  assert(pair.delta == 0.0);
}

void check_state(const DeliveryProblem& p, const DeliveryProblem::State& s) {
  assert(p.is_valid(s.route));
  assert(p.route_cost(s.route) == s.cost);
  array<int, DEPOT_EVENT> position;
  position.fill(-1);
  for (int i = 1; i + 1 < ROUTE_SIZE; ++i) position[s.route[i]] = i;
  assert(position == s.position);
  for (int i = 0; i < ORDER_COUNT; ++i) {
    assert(s.selected[i] == (position[2 * i] != -1));
    assert(s.selected[i] == (position[2 * i + 1] != -1));
  }
}

void test_problem() {
  static_assert(sizeof(DeliveryProblem::Move) <= 32, "Move must not contain a route copy");
  DeliveryProblem p;
  mt19937_64 rng(6789);
  for (auto& order : p.orders)
    order = {int(rng() % 801), int(rng() % 801), int(rng() % 801), int(rng() % 801)};
  p.prepare_distances();
  auto s = p.make_initial_state();
  check_state(p, s);
  for (int a = 1; a + 1 < ROUTE_SIZE; ++a) for (int b = 1; b + 1 < ROUTE_SIZE; ++b) {
    auto candidate = s.route;
    swap(candidate[a], candidate[b]);
    assert(DeliveryProblem::swap_is_valid(s, a, b) == p.is_valid(candidate));
  }
  array<int, 4> count{};
  for (int iteration = 0; iteration < 20000; ++iteration) {
    const auto before = s;
    const auto move = p.propose_move(s, rng, (iteration % 1001) / 1000.0);
    // propose/evaluateは元stateを一切変えない（不採用時にundoも不要）。
    assert(s.route == before.route && s.position == before.position &&
           s.selected == before.selected && s.cost == before.cost);
    if (!move) continue;
    ++count[move->kind];
    auto oracle = vector<int>(s.route.begin(), s.route.end());
    const int a = move->first, b = move->second;
    if (move->kind == DeliveryProblem::Relocate) {
      int event = oracle[a];
      oracle.erase(oracle.begin() + a);
      oracle.insert(oracle.begin() + b, event);
    } else if (move->kind == DeliveryProblem::Swap) {
      swap(oracle[a], oracle[b]);
    } else if (move->kind == DeliveryProblem::Reverse) {
      reverse(oracle.begin() + a, oracle.begin() + b + 1);
    } else {
      oracle.erase(oracle.begin() + b);
      oracle.erase(oracle.begin() + a);
      auto view = DeliveryProblem::WithoutPair{s.route, a, b};
      for (int i = 0; i < view.size(); ++i) assert(view[i] == oracle[i]);
      oracle.insert(oracle.begin() + move->second_gap, 2 * move->inserted_order + 1);
      oracle.insert(oracle.begin() + move->first_gap, 2 * move->inserted_order);
    }
    auto next = s;
    p.apply_move(next, *move);
    assert(equal(oracle.begin(), oracle.end(), next.route.begin()));
    check_state(p, next);
    const auto improvement = p.evaluate_move(s, *move, -numeric_limits<double>::infinity());
    assert(improvement && *improvement == before.cost - p.route_cost(next.route));
    // 採用・不採用の両方を通す。高コスト状態からの差分も検査する。
    if (rng() % 2) s = next;
  }
  for (int n : count) assert(n > 100);
}

int main() {
  test_route_helpers();
  test_problem();
  cout << "AHC006 delta tests passed\n";
}
