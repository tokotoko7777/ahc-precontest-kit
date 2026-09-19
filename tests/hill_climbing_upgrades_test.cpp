#include "library/time-based-simulated-annealing.hpp"
#include <cassert>

template <class Value> struct Problem {
  struct State { Value score = 0; int applied = 0; };
  struct Move { Value delta; };
  using Score = Value;
  int calls = 0, evaluated = 0;
  std::vector<std::optional<Value>> changes{Value{3}, Value{-2}, Value{0},
      std::nullopt, Value{2}, Value{-99}};
  std::optional<Move> propose_move(const State&, std::mt19937_64&, double p) {
    assert(p >= 0 && p <= 1);
    auto delta = changes.at(calls++);
    if (!delta) return std::nullopt;
    return Move{*delta};
  }
  std::optional<Score> evaluate_move(const State&, const Move& move, double threshold) {
    assert(threshold == 0); // 温度に関係なく山登りの採用境界は0。
    ++evaluated;
    if (move.delta == Value{-99}) return std::nullopt;
    return move.delta;
  }
  void apply_move(State& state, Move& move) {
    state.score += move.delta;
    ++state.applied;
  }
};

template <class Score> void check() {
  Problem<Score> problem;
  TimeBasedAnnealingRunner<Problem<Score>> search(problem, {}, Score{0}, 60000, 123, 456, 789, 1);
  const auto acceptance_rng = search.annealing().engine;
  for (int i = 0; i < 6; ++i) assert(search.step_hill_climbing());
  assert(search.iterations() == 6 && search.valid_moves() == 5);
  assert(search.accepted_moves() == 2 && search.best_updates() == 2);
  assert(search.threshold_pruned_moves() == 1 && problem.evaluated == 5);
  assert(search.current_score() == Score{5} && search.best_score() == Score{5});
  assert(search.current_state().score == Score{5} && search.current_state().applied == 2);
  assert(search.best_state().score == Score{5});
  assert(search.annealing().engine == acceptance_rng); // 受理抽選なし。
  search.annealing().start -= std::chrono::hours(1);
  assert(!search.step_hill_climbing());
  assert(search.run_hill_climbing() == 6 && problem.calls == 6);
}

int main() {
  check<long long>();
  check<double>();
  Problem<double> problem;
  problem.changes = {std::numeric_limits<double>::infinity(),
                    -std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::quiet_NaN()};
  TimeBasedAnnealingRunner<Problem<double>> search(problem, {}, 0.0, 60000, 1, 1);
  for (int i = 0; i < 3; ++i) assert(search.step_hill_climbing());
  assert(search.accepted_moves() == 0 && search.best_score() == 0);
}
