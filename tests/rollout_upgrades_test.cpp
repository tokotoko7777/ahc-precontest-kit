#include <array>
#include <cassert>
#include <cmath>
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

void test_custom_engine_precision_and_ties() {
  struct Counter {
    std::uint64_t value;
    explicit Counter(std::uint64_t seed) : value(seed) {}
    int next() { return static_cast<int>(value++); }
  };
  struct CustomProblem {
    using State = int;
    using Action = int;
    using Scenario = int;
    using Score = double;
    std::vector<int> actions{5, 3, 1};
    int calls = 0;
    const std::vector<int>& generate_actions(int) const { return actions; }
    int generate_scenario(int, Counter& random) { return random.next(); }
    double evaluate_action(int, int action, int scenario) {
      ++calls;
      return scenario + action * 1e-14;
    }
  } problem;
  CommonScenarioRolloutRunner<CustomProblem, Counter, double> runner(problem, 7);
  const auto smaller_on_ties = [](int action, double score, int best, double value) {
    return score > value + 1e-12 ||
           (std::abs(score - value) <= 1e-12 && action < best);
  };
  assert(runner.choose_action(0, 3, smaller_on_ties) == 1);
  assert((runner.last_scenarios() == std::vector<int>{7, 8, 9}));
  assert(problem.calls == 9);
  assert(runner.engine().value == 10);
  const std::vector<double>& averages = runner.last_average_scores();
  assert(averages.size() == 3);
  assert(runner.choose_action(0, 1) == 5);
  assert((runner.last_scenarios() == std::vector<int>{10}));
  problem.actions.clear();
  bool threw = false;
  try { runner.choose_action(0, 2); }
  catch (const std::runtime_error&) { threw = true; }
  assert(threw);
}

}  // namespace

int main() {
  test_common_scenario_runner();
  test_custom_engine_precision_and_ties();
}
