#include <cassert>
#include <vector>

// 全ての候補を「同じ未来シナリオ集合」で評価し、候補ごとの平均値を返す。
// 候補ごとに別乱数を使うより、候補差と偶然差を区別しやすい。
//
// 使い方:
// auto average = common_scenario_average(
//     actions, scenarios,
//     [](const Action& action, const Scenario& future) {
//       return simulate(action, future);
//     });
template <class Action, class Scenario, class Evaluate>
std::vector<long double> common_scenario_average(
    const std::vector<Action>& actions,
    const std::vector<Scenario>& scenarios,
    Evaluate evaluate
) {
  assert(!actions.empty());
  assert(!scenarios.empty());

  std::vector<long double> average(actions.size(), 0.0L);
  for (int action = 0; action < static_cast<int>(actions.size()); ++action) {
    for (const Scenario& scenario : scenarios) {
      average[action] += static_cast<long double>(
          evaluate(actions[action], scenario));
    }
    average[action] /= static_cast<long double>(scenarios.size());
  }
  return average;
}
