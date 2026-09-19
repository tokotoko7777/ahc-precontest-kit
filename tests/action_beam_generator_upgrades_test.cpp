#include <cassert>
#include <cmath>
#include <limits>
#include <memory>
#include <random>
#include "library/action-beam-search.hpp"

struct State { int id; };
struct Move {
  std::unique_ptr<int> id;
  explicit Move(int value) : id(new int(value)) {}
};

struct GeneratorOnly {
  using State = int;
  using Action = int;
  using Score = int;
  void apply_action(State& state, Action& action) { state += action; }
  template <class Emit, class CanImprove>
  void enumerate_actions(const State& state, Emit emit, CanImprove can_improve) {
    // 良い順なのでこのboundは「残り全部」の上限。
    for (int action = 5; action >= 0; --action) {
      if (!can_improve(state + action)) break;
      emit(action, state + action);
    }
  }
};

int main() {
  GeneratorOnly problem;
  ActionBeamRunner<GeneratorOnly> runner(problem, 0, 0, 2);
  assert(runner.run_with_generator(3) == 3);
  assert(runner.best_score() == 15 && runner.best() == 15);
  runner.set_width(1);
  assert(runner.run_with_generator(1) == 1 && runner.best() == 20);
  assert(runner.run_with_generator(0) == 0);
  std::mt19937 rng(32);
  for (bool maximize : {false, true}) {
    for (int width : {1, 2, 11, 100}) {
      ActionBeamSearch<State, Move, double> fast({0}, 0, width, maximize);
      ActionBeamSearch<State, Move, double> full({0}, 0, width, maximize);
      full.set_batched_selection(false);
      for (int depth = 0; depth < 5; ++depth) {
        std::vector<double> scores(51);
        for (double& x : scores) x = int(rng() % 13) - 6; // 多数の同点。
        scores[0] = std::numeric_limits<double>::quiet_NaN();
        scores[1] = std::numeric_limits<double>::infinity();
        scores[2] = -std::numeric_limits<double>::infinity();
        auto enumerate = [&](const State& state, auto emit, auto can_improve) {
          assert(can_improve(std::numeric_limits<double>::quiet_NaN()));
          for (int i = 0; i < int(scores.size()); ++i) {
            const double value = scores[i];
            // 正確な値は上限でも下限でもある。順不同なのでbreakは禁止。
            if (can_improve(value)) emit(Move(state.id * 51 + i), value);
          }
        };
        auto apply = [](State& state, Move& move) { state.id = *move.id; };
        assert(fast.step_with_generator(enumerate, apply));
        assert(full.step_with_generator(enumerate, apply));
        assert(fast.size() == full.size());
        for (size_t i = 0; i < fast.size(); ++i) {
          assert(fast.states()[i].id == full.states()[i].id);
          assert(fast.scores()[i] == full.scores()[i] ||
                 (std::isnan(fast.scores()[i]) && std::isnan(full.scores()[i])));
        }
      }
      fast.reset({7}, 3.0);
      auto empty = [](const State&, auto, auto) {};
      assert(!fast.step_with_generator(empty, [](State&, Move&) {}));
      assert(fast.best().id == 7);
    }
  }
}
