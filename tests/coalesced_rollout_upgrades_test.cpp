#include <array>
#include <cassert>
#include <random>
#include <stdexcept>
#include "library/coalesced-rollout.hpp"
#include "library/common-scenario-average.hpp"

struct NonDefault {
  int value;
  explicit NonDefault(int x) : value(x) {}
  bool operator==(const NonDefault& other) const { return value == other.value; }
};

int main() {
  CoalescedRollout<NonDefault, NonDefault> merged, separate;
  merged.reserve(20);
  const auto advance = [](NonDefault& state, int step) { state.value = state.value / 2 + step; };
  const auto score = [](const NonDefault& state) { return state; };
  const std::vector<NonDefault> initial{NonDefault(0), NonDefault(0), NonDefault(11), NonDefault(24)};
  assert(merged.evaluate(initial, 10, advance, score) == separate.evaluate<false>(initial, 10, advance, score));
  assert(merged.last_transitions() < separate.last_transitions());
  assert(separate.last_transitions() == 40);
  assert(merged.evaluate(initial, 0, advance, score) == initial);
  assert(merged.evaluate({}, 5, advance, score).empty());
  bool threw = false;
  try { merged.evaluate(initial, -1, advance, score); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  std::mt19937 random(15015);
  for (int run = 0; run < 200; ++run) {
    std::vector<NonDefault> starts;
    for (int i = 0, n = random() % 20; i < n; ++i) starts.emplace_back(random() % 1000);
    const int steps = random() % 15;
    assert(merged.evaluate(starts, steps, advance, score) == separate.evaluate<false>(starts, steps, advance, score));
  }

  struct Problem {
    using State = int;
    using Action = int;
    using Scenario = int;
    using Score = double;
    std::vector<int> generate_actions(int) { return {2, 0, 1}; }
    int generate_scenario(int, std::mt19937_64& engine) { return engine() % 100; }
    double evaluate_action(int state, int action, int scenario) { return state + action * 1e-14 + scenario; }
  } problem;
  CommonScenarioRolloutRunner<Problem, std::mt19937_64, double> classic(problem, 31), batched(problem, 31);
  const auto evaluate_batch = [&](int state, const std::vector<int>& actions, int scenario) {
    std::vector<double> scores;
    for (int action : actions) scores.push_back(problem.evaluate_action(state, action, scenario));
    return scores;
  };
  for (int turn = 0; turn < 20; ++turn) {
    assert(classic.choose_action(turn, 13) == batched.choose_action_batched(turn, 13, evaluate_batch));
    assert(classic.last_scenarios() == batched.last_scenarios());
    assert(classic.last_average_scores() == batched.last_average_scores());
  }
  const auto smaller = [](int action, double, int best, double) { return action < best; };
  assert(batched.choose_action_batched(0, 3, evaluate_batch, smaller) == 0);
  threw = false;
  try { batched.choose_action_batched(0, 1, [](int, const auto&, int) { return std::array<int, 1>{0}; }); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  assert(batched.choose_action_batched(0, 2, evaluate_batch) == 2);
}
