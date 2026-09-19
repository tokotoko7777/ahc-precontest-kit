#define main ahc002_solver_main
#include "../examples/search/ahc002_destroy_repair_sa.cpp"
#undef main

int main() {
  for (int mode = 0; mode < 3; ++mode) {
    TilePathProblem problem;
    problem.start_cell = 25 * BOARD_SIZE + 25;
    problem.seed = 123 + mode;
    for (int r = 0; r < BOARD_SIZE; ++r) {
      for (int c = 0; c < BOARD_SIZE; ++c) {
        const int cell = r * BOARD_SIZE + c;
        problem.tile[cell] = mode == 0 ? cell : mode == 1 ? r * 25 + c / 2 : (r / 2) * 50 + c;
        problem.point[cell] = (cell * 17 + r * 13) % 100;
      }
    }
    std::mt19937_64 random(91823 + mode);
    auto state = problem.make_initial_state(0.0);
    assert(problem.is_valid(state));
    for (int step = 0; step < 2000; ++step) {
      const auto previous = state;
      auto move = problem.propose_move(state, random, (step % 101) / 100.0);
      assert(state.path == previous.path && state.used_tile == previous.used_tile);
      assert(state.prefix_score == previous.prefix_score && state.score == previous.score);
      if (!move) continue;
      auto change = problem.evaluate_move(state, *move, -std::numeric_limits<double>::infinity());
      if (!change) continue;
      auto candidate = state;
      problem.apply_move(candidate, *move);
      assert(problem.is_valid(candidate));
      assert(candidate.score - state.score == *change);
      auto rebuilt = candidate;
      problem.rebuild_cache(rebuilt);
      assert(rebuilt.used_tile == candidate.used_tile);
      assert(rebuilt.prefix_score == candidate.prefix_score);
      assert(rebuilt.score == candidate.score);
      if (*change >= 0 || random() % 7 == 0) state = std::move(candidate);
    }
  }
  std::cout << "AHC002: 6000 incremental proposals match full checks\n";

  // 3x3の全単純路と照合し、上界・偶奇枝刈りが最善の修復を捨てないことを検査。
  std::mt19937_64 random(2233);
  for (int test = 0; test < 300; ++test) {
    TilePathProblem problem;
    std::iota(problem.tile.begin(), problem.tile.end(), 0);
    std::array<unsigned char, CELL_COUNT> used;
    used.fill(1);
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) {
      const int cell = r * BOARD_SIZE + c;
      used[cell] = 0;
      problem.point[cell] = static_cast<int>(random() % 100);
    }
    const int target_index = 1 + static_cast<int>(random() % 8);
    const int target = (target_index / 3) * BOARD_SIZE + target_index % 3;
    const int limit = 1 + static_cast<int>(random() % 9);
    used[0] = used[target] = 1;
    auto visited = used;
    int expected = -1;
    std::function<void(int, int, int)> brute = [&](int cell, int depth, int score) {
      if (depth == limit) return;
      for (int i = 0; i < problem.next_count[cell]; ++i) {
        const int to = problem.next[cell][i];
        if (to == target) { expected = std::max(expected, score); continue; }
        if (visited[to]) continue;
        visited[to] = 1;
        brute(to, depth + 1, score + problem.point[to]);
        visited[to] = 0;
      }
    };
    brute(0, 0, 0);
    TilePathProblem::SegmentRepair repair(problem, random, used, target, limit,
                                         1000000, -1, {});
    repair.search(0, 0, 0);
    assert(repair.best_score == expected);
    assert(used == visited);  // DFSの仮更新は完全に戻る。
  }
  std::cout << "AHC002: 300 exhaustive repair optima match bounded DFS\n";
}
