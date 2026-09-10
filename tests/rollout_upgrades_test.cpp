#include <array>
#include <cassert>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

#include "library/common-scenario-average.hpp"

namespace {

struct RolloutProblem {
  struct State {
    int bias = 0;
  };
  using Action = int;
  using Scenario = int;
  using Score = int;

  std::array<Action, 2> actions{{0, 1}};

  const std::array<Action, 2>& generate_actions(const State&) const {
    return actions;
  }

  Scenario generate_scenario(const State&, std::mt19937_64& engine) const {
    return static_cast<int>(engine() % 100);
  }

  Score evaluate_action(const State& state,
                        const Action& action,
                        const Scenario& scenario) const {
    return state.bias + scenario + 5 * action;
  }
};

void test_common_scenario_runner() {
  RolloutProblem problem;
  CommonScenarioRolloutRunner<RolloutProblem> maximize(problem, 123);
  maximize.reserve(2, 20);
  assert(maximize.choose_action({7}, 20) == 1);
  assert(maximize.last_actions().size() == 2);
  assert(maximize.last_scenarios().size() == 20);
  assert(maximize.last_average_scores().size() == 2);
  assert(maximize.last_average_scores()[1] -
             maximize.last_average_scores()[0] ==
         5.0L);

  CommonScenarioRolloutRunner<RolloutProblem> minimize(problem, 123, false);
  assert(minimize.choose_action({7}, 20) == 0);

  bool threw = false;
  try {
    maximize.choose_action({0}, 0);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  threw = false;
  try {
    maximize.reserve(-1, 1);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

}  // namespace

int main() { test_common_scenario_runner(); }
