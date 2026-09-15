#include <array>
#include <cassert>
#include <memory>
#include "library/time-based-simulated-annealing.hpp"

struct IgnoringThresholdProblem {
  using Score = long long;
  struct State { int x = 0; };
  struct Move { int x; };
  int evaluations = 0, discarded = 0;
  bool saw_finite_threshold = false;
  static Score score(const State& state) { return -1LL * state.x * state.x; }

  std::optional<Move> propose_move(const State&, std::mt19937_64& random, double) const {
    const int next = static_cast<int>(random() % 25) - 12;
    if (next == 12) return std::nullopt;
    return Move{next};
  }
  // 3引数の評価関数1個だけで、run/stepと閾値方式の全てを使える。
  std::optional<Score> evaluate_move(const State& state, const Move& move, double threshold) {
    ++evaluations;
    saw_finite_threshold |= std::isfinite(threshold);
    // この問題ではx=11は禁止。OFF(-inf)でも不合法手はnulloptで棄却できる。
    if (move.x == 11) { ++discarded; return std::nullopt; }
    // 閾値を使わず全計算しても、採否はRunnerが判断する。
    return score(State{move.x}) - score(state);
  }
  void apply_move(State& state, Move& move) const {
    assert(move.x != 11);
    state.x = move.x;
  }
};

using Runner = TimeBasedAnnealingRunner<IgnoringThresholdProblem>;

void test_ignored_threshold() {
  std::array<IgnoringThresholdProblem, 4> problems;
  std::array<std::unique_ptr<Runner>, 4> runners;
  for (int mode = 0; mode < 4; ++mode) {
    runners[mode] = std::make_unique<Runner>(problems[mode], IgnoringThresholdProblem::State{},
                                           0LL, 1e9, 20, 20, 53, 64);
    runners[mode]->annealing().set_threshold_precomputation((mode & 2) != 0);
  }
  for (int trial = 0; trial < 5000; ++trial) {
    for (int mode = 0; mode < 4; ++mode) {
      auto& current = *runners[mode];
      assert(current.step_with_threshold((mode & 1) != 0));
      assert(current.current_state().x == runners[0]->current_state().x);
      assert(current.current_score() == runners[0]->current_score());
      assert(current.best_score() == runners[0]->best_score());
      assert(current.accepted_moves() == runners[0]->accepted_moves());
      assert(current.annealing().engine == runners[0]->annealing().engine);
    }
  }
  for (int mode = 0; mode < 4; ++mode) {
    assert(problems[mode].discarded > 0);
    assert(problems[mode].saw_finite_threshold == static_cast<bool>(mode & 1));
    assert(runners[mode]->threshold_pruned_moves() == static_cast<unsigned>(problems[mode].discarded));
    // 3引数の評価関数だけで、引数なしrunと閾値版runの全てがコンパイルできる。
    auto& clock = runners[mode]->annealing();
    clock.start -= std::chrono::hours(1000);
    clock.calls_until_check = 0;
    assert(runners[mode]->run_with_threshold() == 5000);
    assert(runners[mode]->run_with_threshold(false) == 5000);
    assert(runners[mode]->run() == 5000);
  }
}

void test_legacy_acceptance_stream() {
  // 通常stepは従来どおり、全評価後にaccept()する。受理乱数を増やさない。
  IgnoringThresholdProblem problem, reference_problem;
  Runner runner(problem, {}, 0LL, 1e9, 20, 20, 17, 64);
  TimeBasedSimulatedAnnealing reference(1e9, 20, 20, 17, 64);
  std::mt19937_64 moves(17 ^ 0xd1b54a32d192ed03ULL);
  IgnoringThresholdProblem::State state;
  long long score = 0, best = 0;
  std::uint64_t accepted = 0;
  for (int trial = 0; trial < 5000; ++trial) {
    auto move = reference_problem.propose_move(state, moves, 0);
    if (move) {
      auto delta = reference_problem.evaluate_move(state, *move, -std::numeric_limits<double>::infinity());
      if (delta && reference.accept(*delta)) {
        reference_problem.apply_move(state, *move);
        score += *delta;
        best = std::max(best, score);
        ++accepted;
      }
    }
    assert(runner.step());
    assert(runner.current_state().x == state.x && runner.current_score() == score);
    assert(runner.best_score() == best && runner.accepted_moves() == accepted);
    assert(runner.annealing().engine == reference.engine);
  }
  assert(!problem.saw_finite_threshold && problem.discarded > 0);
}

int main() {
  test_ignored_threshold();
  test_legacy_acceptance_stream();
}
