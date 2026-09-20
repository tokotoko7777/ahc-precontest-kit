#define AHC005_TEST
#include "../examples/search/ahc005_patrol_sa.cpp"

static void check(const PatrolProblem& problem, const PatrolProblem::State& state) {
  assert(state.cost == problem.full_cost(state));
  assert(state.order.front().group == -1 && state.order.front().point == 0);
  vector<int> count(problem.groups.size()), order;
  for (auto v : state.order) {
    order.push_back(v.point);
    if (v.group < 0) continue;
    ++count[v.group];
    const auto& choices = problem.groups[v.group];
    assert(find(choices.begin(), choices.end(), v.point) != choices.end());
  }
  for (int value : count) assert(value == 1);
  assert(problem.map.route_covers_every_road(order));
  assert(problem.map.answer_is_valid(problem.map.make_answer(order)));
}

int main() {
  mt19937_64 random(2468);
  for (int test = 0; test < 12; ++test) {
    ostringstream text;
    int n = 7 + 2*(test%3);
    text << n << " 0 0\n";
    for (int r = 0; r < n; ++r) {
      for (int c = 0; c < n; ++c)
        text << ((r%2==0 || c%2==0) ? char('5'+random()%5) : '#');
      text << '\n';
    }
    istringstream input(text.str());
    auto* previous = cin.rdbuf(input.rdbuf());
    Solver map;
    map.read_input();
    cin.rdbuf(previous);
    map.build_segments(); map.build_visibility_sets();
    PatrolProblem problem(map);
    for (int a = 0; a < (int)map.terminals.size(); ++a)
      for (int b = 0; b < (int)map.terminals.size(); ++b)
        assert(problem.distance(a,b) == map.distance_between_terminals[a][b]);
    auto state = problem.initial_state();
    check(problem, state);
    for (int step = 0; step < 3000; ++step) {
      auto move = problem.propose_move(state, random, double(step)/3000);
      if (!move) continue;
      auto changed = state;
      const int before = problem.full_cost(state);
      auto delta = problem.evaluate_move(state, *move, -INFINITY);
      assert(delta);
      problem.apply_move(changed, *move);
      assert(*delta == before-problem.full_cost(changed));
      assert(changed.cost == problem.full_cost(changed));
      assert(state.cost == before);
      if (random()%2) state = std::move(changed);
      if (step%200 == 0) check(problem, state);
    }
    check(problem, state);
  }
  cout << "AHC005: 36000 delta / representative / route checks passed\n";
}
