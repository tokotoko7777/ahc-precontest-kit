#define main ahc001_region_main
#include "../examples/search/ahc001_region_sa.cpp"
#undef main

int main() {
  {
    // x方向に押すと相手の点を失うが、相手の下辺を上げれば両方の点を残せる。
    vector<Request> input{{1, 1, 100}, {11, 8, 100}};
    RegionProblem clipping(input);
    auto initial = clipping.make_state({{0,0,10,5}, {10,0,20,10}});
    RegionProblem::Move move{0, 0, 1, 15, 0, 0};
    const auto delta = clipping.evaluate_move(initial, move, -numeric_limits<double>::infinity());
    assert(delta);
    const double before = clipping.score(initial);
    clipping.apply_move(initial, move);
    clipping.validate(initial);
    assert(initial.regions[1].bottom == 5);
    assert(abs(clipping.score(initial) - before - *delta) < 1e-12);
  }
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
  // 領域0を右へ広げ、領域1の要求点を失わせる手。-infでも不合法なら棄却する。
  const RegionProblem::Move illegal{0, 0, 1, 3500, 0, 0};
  assert(!problem.evaluate_move(state, illegal, -numeric_limits<double>::infinity()));
  assert(state.regions[0].right == 2500 && state.regions[1].left == 2500);
  problem.validate(state);
  mt19937_64 random(42);
  for (int trial = 0; trial < 1600; ++trial) {
    const auto move = problem.propose_move(state, random, 0.3);
    if (!move) continue;
    const auto previous = state;
    const auto delta = problem.evaluate_move(state, *move, -numeric_limits<double>::infinity());
    assert(state.quality == previous.quality);
    for (size_t i = 0; i < state.regions.size(); ++i) {
      const auto& a = state.regions[i];
      const auto& b = previous.regions[i];
      assert(a.left == b.left && a.bottom == b.bottom &&
             a.right == b.right && a.top == b.top);
    }
    if (delta) {
      auto candidate = state;
      auto action = *move;
      problem.apply_move(candidate, action);
      problem.validate(candidate);
      assert(abs(problem.score(candidate) - problem.score(state) - *delta) < 1e-10);
      // 閾値版が切った場合は、本当に境界を超えないことを確認する。
      const double threshold = -static_cast<double>(random() % 100) / 10000.0;
      const auto bounded = problem.evaluate_move(state, *move, threshold);
      if (!bounded) assert(*delta <= threshold + 1e-12);
      else assert(abs(*bounded - *delta) < 1e-10);
      if (trial % 3 != 0) state = candidate;
    }
  }
  TimeBasedAnnealingRunner<RegionProblem> runner(problem, state, problem.score(state),
                                                1e9, 0.01, 0.01, 1, 1);
  for (int i = 0; i < 300; ++i) runner.step_with_threshold();
  problem.validate(runner.best_state());
  assert(abs(problem.score(runner.best_state()) - runner.best_score()) < 1e-8);

  // 実問題のadapterも4通りで比較。同じ初期状態・固定温度なら採否は同じ。
  array<RegionProblem, 4> problems = {RegionProblem(requests), RegionProblem(requests),
                                    RegionProblem(requests), RegionProblem(requests)};
  using Runner = TimeBasedAnnealingRunner<RegionProblem>;
  array<unique_ptr<Runner>, 4> runners;
  for (int mode = 0; mode < 4; ++mode) {
    runners[mode] = make_unique<Runner>(problems[mode], state, problem.score(state),
                                      1e9, 0.01, 0.01, 42, 1);
    runners[mode]->annealing().set_threshold_precomputation((mode & 2) != 0);
  }
  auto same_state = [](const RegionProblem::State& a, const RegionProblem::State& b) {
    assert(a.quality == b.quality && a.regions.size() == b.regions.size());
    for (size_t i = 0; i < a.regions.size(); ++i) {
      assert(a.regions[i].left == b.regions[i].left);
      assert(a.regions[i].right == b.regions[i].right);
      assert(a.regions[i].bottom == b.regions[i].bottom);
      assert(a.regions[i].top == b.regions[i].top);
    }
  };
  for (int trial = 0; trial < 1000; ++trial) {
    for (int mode = 0; mode < 4; ++mode) {
      auto& current = *runners[mode];
      assert(current.step_with_threshold((mode & 1) != 0));
      same_state(current.current_state(), runners[0]->current_state());
      same_state(current.best_state(), runners[0]->best_state());
      assert(current.current_score() == runners[0]->current_score());
      assert(current.best_score() == runners[0]->best_score());
      assert(current.accepted_moves() == runners[0]->accepted_moves());
      assert(current.annealing().engine == runners[0]->annealing().engine);
    }
  }
  for (const auto& current : runners) problem.validate(current->best_state());
}
