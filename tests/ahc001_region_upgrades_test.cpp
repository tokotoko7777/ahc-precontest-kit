#define main ahc001_region_main
#include "../examples/search/ahc001_region_sa.cpp"
#undef main

int main() {
  // 領域内の最終サイズ選択を小さい全探索と照合する。
  for (int w = 1; w <= 15; ++w) for (int h = 1; h <= 15; ++h) {
    for (int desired = 1; desired <= 225; desired += 7) {
      int bw = 1, bh = 1;
      const auto area = exact_best_area(w, h, desired, bw, bh);
      assert(1 <= bw && bw <= w && 1 <= bh && bh <= h);
      double exact = 0;
      for (int a = 1; a <= w; ++a) for (int b = 1; b <= h; ++b)
        exact = max(exact, satisfaction(desired, a * b));
      assert(abs(exact - satisfaction(desired, area)) < 1e-14);
    }
  }
  vector<Request> requests;
  vector<Rect> regions;
  for (int i = 0; i < 16; ++i) {
    int x = i % 4 * 2500, y = i / 4 * 2500;
    requests.push_back({x + 700, y + 1100, 3000000 + i * 500000});
    regions.push_back({x,y,x+2500,y+2500});
  }
  RegionProblem problem(requests);
  auto state = problem.make_state(regions);
  mt19937_64 random(42);
  for (int trial = 0; trial < 1600; ++trial) {
    const auto move = problem.propose_move(state, random, 0.3);
    if (!move) continue;
    const auto previous = state;
    const auto delta = problem.evaluate_move(state, *move);
    assert(state.quality == previous.quality);
    for (size_t i = 0; i < state.regions.size(); ++i) {
      const auto& a = state.regions[i];
      const auto& b = previous.regions[i];
      assert(a.left == b.left && a.bottom == b.bottom &&
             a.right == b.right && a.top == b.top);
    }
    if (delta > -1e99) {
      auto candidate = state;
      auto action = *move;
      problem.apply_move(candidate, action);
      problem.validate(candidate);
      assert(abs(problem.score(candidate) - problem.score(state) - delta) < 1e-10);
      // 閾値版が切った場合は、本当に境界を超えないことを確認する。
      const double threshold = -static_cast<double>(random() % 100) / 10000.0;
      const auto bounded = problem.evaluate_move_with_threshold(state, *move, threshold);
      if (!bounded) assert(delta <= threshold + 1e-12);
      else assert(abs(*bounded - delta) < 1e-10);
      if (trial % 3 != 0) state = candidate;
    }
  }
  TimeBasedAnnealingRunner<RegionProblem> runner(problem, state, problem.score(state),
                                                1e9, 0.01, 0.01, 1, 1);
  for (int i = 0; i < 300; ++i) runner.step_with_threshold();
  problem.validate(runner.best_state());
  assert(abs(problem.score(runner.best_state()) - runner.best_score()) < 1e-8);
}
