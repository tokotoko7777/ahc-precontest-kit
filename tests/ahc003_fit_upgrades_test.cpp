#define main ahc003_solver_main
#include "../examples/search/ahc003_online_fit.cpp"
#undef main

int main() {
  RegressionProblem problem;
  RegressionProblem::State state;
  std::mt19937_64 random(18823);
  double score = 0;
  for (int observation = 0; observation < 100; ++observation) {
    vector<int> edges;
    int observed = 0;
    for (int j = 0; j < 20; ++j) {
      const int edge = static_cast<int>(random() % EDGE_COUNT);
      edges.push_back(edge);
      observed += 4500 + 29 * edge_line(edge) + edge % 100;
    }
    score += problem.observe(state, edges, observed);
    assert(abs(score - problem.full_score(state)) < 1e-7 * (1 + abs(score)));
    for (int step = 0; step < 64; ++step) {
      auto move = problem.propose_move(state, random, 0.5);
      if (!move) continue;
      const double previous = problem.full_score(state);
      auto change = problem.evaluate_move(state, *move, 0.0);
      if (!change) continue;
      problem.apply_move(state, *move);
      score += *change;
      const double actual = problem.full_score(state) - previous;
      assert(abs(actual - *change) < 1e-7 * (1 + abs(actual)));
      for (size_t i = 0; i < problem.history.size(); ++i) {
        const auto& row = problem.history[i];
        double prediction = row.base;
        for (auto [feature, count] : row.features) prediction += count * state.weight[feature];
        assert(abs(prediction - state.prediction[i]) < 1e-6);
      }
    }
    const auto costs = problem.planning_costs(state, observation);
    for (int value : costs) assert(value >= 1000 && value <= 9000);
  }
  cout << "AHC003: coordinate deltas and prediction caches match full recomputation\n";
}
