#include <array>
#include <cassert>
#include <chrono>
#include <memory>
#include "library/time-based-simulated-annealing.hpp"

// 非負の損失を順に引く、途中打ち切り可能な最小化問題。
struct LossProblem {
  using Score = long long;
  struct State { int x; Score cost; };
  struct Move { int x; };
  int last_proposal = 0;
  Score pending_cost = 0;
  int full_calls = 0, bounded_calls = 0, terms = 0, stopped = 0;

  static Score loss(int x, int index) {
    const Score difference = x - (index - 32);
    return difference * difference;
  }
  static Score cost(int x) {
    Score result = 0;
    for (int i = 0; i < 64; ++i) result += loss(x, i);
    return result;
  }
  std::optional<Move> propose_move(const State&, std::mt19937_64& random, double) {
    last_proposal = static_cast<int>(random() % 129) - 64;
    if (last_proposal == 64) return std::nullopt;
    return Move{last_proposal};
  }
  // 評価関数は1個だけ。-infならこの同じループで全項を計算する。
  std::optional<Score> evaluate_move(
      const State& state, const Move& move, double threshold) {
    if (threshold == -std::numeric_limits<double>::infinity()) ++full_calls;
    else ++bounded_calls;
    Score new_cost = 0;
    for (int i = 0; i < 64; ++i) {
      ++terms;
      new_cost += loss(move.x, i);
      // 残りの損失は非負。state.cost-new_costは最終改善量の上限。
      if (static_cast<double>(state.cost - new_cost) <= threshold) {
        ++stopped;
        return std::nullopt;
      }
    }
    pending_cost = new_cost;
    return state.cost - new_cost;
  }
  void apply_move(State& state, Move& move) { state = {move.x, pending_cost}; }
};

void test_configuration() {
  TimeBasedSimulatedAnnealing sa(1e9, 1, 1, 123);
  const auto random = sa.engine;
  const auto start = sa.start;
  assert(!sa.threshold_precomputation_enabled());
  sa.set_threshold_precomputation(true);
  assert(sa.threshold_precomputation_enabled() && sa.threshold_table_size() == 4096);
  for (std::size_t bins : {std::size_t{0}, std::size_t{3}, (std::size_t{1} << 20) + 1}) {
    bool threw = false;
    try { sa.set_threshold_precomputation(true, bins); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw && sa.threshold_table_size() == 4096);
  }
  sa.set_threshold_precomputation(false, 3); // OFFなら表のサイズは使わない。
  assert(!sa.threshold_precomputation_enabled() && sa.threshold_table_size() == 0);
  sa.set_threshold_precomputation(true, 16);
  assert(sa.threshold_precomputation_enabled() && sa.threshold_table_size() == 16);
  sa.set_threshold_table_size(0); // 既存APIと状態を共有。
  assert(!sa.threshold_precomputation_enabled());
  assert(sa.engine == random && sa.start == start);
}

int main() {
  test_configuration();
  using Runner = TimeBasedAnnealingRunner<LossProblem>;
  std::array<LossProblem, 4> problems;
  std::array<std::unique_ptr<Runner>, 4> runners;
  for (int mode = 0; mode < 4; ++mode) {
    const LossProblem::State initial{30, LossProblem::cost(30)};
    runners[mode] = std::make_unique<Runner>(problems[mode], initial, -initial.cost,
                                           1e9, 80, 80, 192, 64);
    runners[mode]->annealing().set_threshold_precomputation((mode & 2) != 0);
  }
  for (int step = 0; step < 5000; ++step) {
    for (int mode = 0; mode < 4; ++mode) {
      auto& runner = *runners[mode];
      assert(runner.step_with_threshold((mode & 1) != 0));
      const auto& reference = *runners[0];
      assert(runner.current_state().x == reference.current_state().x);
      assert(runner.current_state().cost == reference.current_state().cost);
      assert(runner.current_score() == -LossProblem::cost(runner.current_state().x));
      assert(runner.best_state().x == reference.best_state().x);
      assert(runner.best_score() == reference.best_score());
      assert(runner.accepted_moves() == reference.accepted_moves());
      assert(runner.best_updates() == reference.best_updates());
      assert(runner.valid_moves() == reference.valid_moves());
      assert(runner.annealing().engine == reference.annealing().engine);
      assert(problems[mode].last_proposal == problems[0].last_proposal);
    }
  }
  for (int mode = 0; mode < 4; ++mode) {
    const auto& problem = problems[mode];
    if (mode & 1) {
      // ONでもu=0や区間表の最初のbinでは安全な下限が-infになることがある。
      assert(problem.bounded_calls > 0);
      assert(problem.stopped > 0 && problem.terms < problems[0].terms);
      assert(runners[mode]->threshold_pruned_moves() == static_cast<unsigned>(problem.stopped));
    } else {
      assert(problem.full_calls > 0 && problem.bounded_calls == 0);
      assert(problem.terms == 64 * problem.full_calls);
      assert(runners[mode]->threshold_pruned_moves() == 0);
    }
    // 終了済みのrunも両方の設定で動く。時計依存の長い実行にはしない。
    runners[mode]->annealing().start -= std::chrono::hours(1000);
    runners[mode]->annealing().calls_until_check = 0;
    assert(runners[mode]->run_with_threshold((mode & 1) != 0) == 5000);
    assert(runners[mode]->iterations() == 5000);
  }
}
