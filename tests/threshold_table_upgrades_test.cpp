#include <cassert>
#include <cmath>
#include <limits>
#include <random>
#include "library/time-based-simulated-annealing.hpp"

void test_windows() {
  const double infinity = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  for (std::size_t bins : {std::size_t{0}, std::size_t{1}, std::size_t{16}, std::size_t{4096}}) {
    for (double temperature : {std::numeric_limits<double>::denorm_min(),
                               1e-310, 1e-100, 1e-8, 1.0, 123.5, 1e100,
                               std::numeric_limits<double>::max()}) {
      TimeBasedSimulatedAnnealing table(1e9, temperature, temperature, 9182);
      TimeBasedSimulatedAnnealing exact(1e9, temperature, temperature, 9182);
      table.set_threshold_table_size(bins);
      assert(table.threshold_table_size() == bins);
      // 前計算は乱数を消費しない。別instanceの進行とも干渉しない。
      assert(table.engine == exact.engine);
      for (int trial = 0; trial < 3000; ++trial) {
        const auto window = table.draw_acceptance_window();
        const double threshold = exact.draw_acceptance_threshold();
        assert(window.lower <= threshold && threshold <= window.upper);
        for (double delta : {threshold, std::nextafter(threshold, -infinity),
                             std::nextafter(threshold, infinity), -infinity, infinity,
                             nan, 0.0, -temperature, -temperature * 0.01}) {
          assert(window.accept(delta) == exact.accept_with_threshold(delta, threshold));
        }
      }
      assert(table.engine == exact.engine);
      table.set_threshold_table_size(0);
      assert(table.threshold_table_size() == 0);
      assert(table.draw_acceptance_window().lower == exact.draw_acceptance_threshold());
    }
  }
  TimeBasedSimulatedAnnealing sa(1e9, 1, 1, 0);
  sa.set_threshold_table_size(16);
  for (std::size_t bins : {std::size_t{3}, (std::size_t{1} << 20) + 1}) {
    bool threw = false;
    try { sa.set_threshold_table_size(bins); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw && sa.threshold_table_size() == 16);
  }
  // u=0を捨てない。大きな悪化を受理する裾も有限表へ切り詰めない。
  const TimeBasedSimulatedAnnealing::AcceptanceWindow zero{-infinity, -1, 0, 1};
  assert(zero.accept(-1000000.0));
  assert(!zero.accept(-infinity));
}

struct Problem {
  struct State { int x; };
  struct Move { int change; };
  using Score = int;
  std::optional<Move> propose_move(const State&, std::mt19937_64& random, double) {
    return Move{static_cast<int>(random() % 7) - 3};
  }
  std::optional<int> evaluate_move(const State& state, const Move& move, double bound) {
    const int next = state.x + move.change;
    const int delta = state.x * state.x - next * next;
    return delta <= bound ? std::nullopt : std::optional<int>{delta};
  }
  void apply_move(State& state, Move& move) { state.x += move.change; }
};

int main() {
  test_windows();
  Problem problem;
  TimeBasedAnnealingRunner<Problem> exact(problem, {12}, -144, 1e9, 8, 8, 91, 64);
  TimeBasedAnnealingRunner<Problem> table(problem, {12}, -144, 1e9, 8, 8, 91, 64);
  table.annealing().set_threshold_table_size(4096);
  for (int i = 0; i < 20000; ++i) {
    assert(exact.step_with_threshold() && table.step_with_threshold());
    assert(exact.current_state().x == table.current_state().x);
    assert(exact.current_score() == table.current_score());
    assert(exact.best_score() == table.best_score());
    assert(exact.accepted_moves() == table.accepted_moves());
  }
}
