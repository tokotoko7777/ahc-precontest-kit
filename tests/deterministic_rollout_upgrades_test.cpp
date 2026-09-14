#include <array>
#include <cassert>
#include <stdexcept>

#include "library/deterministic-rollout.hpp"

namespace {

struct TestProblem {
  struct State {
    int target = 0;
  };
  using Action = int;
  using Score = long long;

  std::array<Action, 3> actions{{-2, 1, 4}};

  const std::array<Action, 3>& generate_actions(const State&) const {
    return actions;
  }

  Score evaluate_action(const State& state, const Action& action) const {
    const long long difference = action - state.target;
    return difference * difference;
  }
};

void test_minimize_and_inspection() {
  TestProblem problem;
  DeterministicRolloutRunner<TestProblem> runner(problem, false);
  runner.reserve(3);
  assert(runner.choose_action({2}) == 1);
  assert(runner.last_actions().size() == 3);
  assert(runner.last_scores()[0] == 16);
  assert(runner.last_scores()[1] == 1);
  assert(runner.last_scores()[2] == 4);
  assert(runner.last_best_index() == 1);
}

void test_maximize_and_validation() {
  TestProblem problem;
  DeterministicRolloutRunner<TestProblem> runner(problem);
  assert(runner.choose_action({2}) == -2);

  bool threw = false;
  try {
    runner.reserve(-1);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

struct EmptyProblem {
  using State = int;
  using Action = int;
  using Score = int;

  std::array<int, 0> generate_actions(const State&) const { return {}; }
  Score evaluate_action(const State&, const Action&) const { return 0; }
};

void test_empty_actions() {
  EmptyProblem problem;
  DeterministicRolloutRunner<EmptyProblem> runner(problem);
  bool threw = false;
  try {
    runner.choose_action(0);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
}

}  // namespace

int main() {
  test_minimize_and_inspection();
  test_maximize_and_validation();
  test_empty_actions();
}
