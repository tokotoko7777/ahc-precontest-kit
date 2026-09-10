#include <algorithm>
#include <cassert>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
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

// 問題依存部分をProblemへ集める、共通シナリオrolloutの薄いRunner。
//
// 【使う人がmain.cpp側へ書く場所】
// 次のTODOだけを自分の問題に合わせる。Runner本体は通常変更しない。
//
//   TODO: 【問題ごと】現在情報、今の1手、未知の未来、評価値の型を書く。
//   using State, Action, Scenario, Score
//   TODO: 【問題ごと】今比較したい合法Actionを全て返す。
//   generate_actions(const State&) -> 比較する最初のAction一覧。
//   TODO: 【問題ごと】自分では決められない未知情報だけをsampleする。
//   generate_scenario(const State&, mt19937_64&)
//     -> 未知の未来を1本生成する。
//   TODO: 【問題ごと】最初のAction後を終端まで進め、評価値を返す。
//   evaluate_action(const State&, const Action&, const Scenario&)
//     -> その最初のActionからシナリオを辿った評価値。
//
// Runnerは全Actionを同じScenario集合で比較し、平均が最良の1手を返す。
// Stateの更新や出力は行わない。選んだActionの反映は呼び出し側が行う。
// ↓↓↓ ここから下はライブラリ本体。通常は編集しない。↓↓↓
template <class Problem>
struct CommonScenarioRolloutRunner {
  using State = typename Problem::State;
  using Action = typename Problem::Action;
  using Scenario = typename Problem::Scenario;
  using Score = typename Problem::Score;

  explicit CommonScenarioRolloutRunner(Problem& problem,
                                       std::uint64_t seed = 0,
                                       bool maximize = true)
      : problem_(problem), engine_(seed), maximize_(maximize) {}

  Action choose_action(const State& state, int sample_count) {
    if (sample_count <= 0) {
      throw std::invalid_argument("sample_count must be positive");
    }

    actions_.clear();
    auto&& generated_actions = problem_.generate_actions(state);
    for (const auto& action : generated_actions) actions_.push_back(action);
    if (actions_.empty()) {
      throw std::runtime_error("generate_actions returned no action");
    }

    scenarios_.clear();
    scenarios_.reserve(static_cast<std::size_t>(sample_count));
    for (int sample = 0; sample < sample_count; ++sample) {
      scenarios_.push_back(problem_.generate_scenario(state, engine_));
    }

    average_scores_.assign(actions_.size(), 0.0L);
    for (std::size_t action = 0; action < actions_.size(); ++action) {
      for (const Scenario& scenario : scenarios_) {
        average_scores_[action] += static_cast<long double>(
            problem_.evaluate_action(state, actions_[action], scenario));
      }
      average_scores_[action] /= static_cast<long double>(sample_count);
    }

    std::size_t best = 0;
    for (std::size_t action = 1; action < actions_.size(); ++action) {
      const bool better = maximize_
                              ? average_scores_[best] < average_scores_[action]
                              : average_scores_[action] < average_scores_[best];
      if (better) best = action;
    }
    return actions_[best];
  }

  void reserve(int action_count, int sample_count) {
    if (action_count < 0 || sample_count < 0) {
      throw std::invalid_argument("reserve counts must be non-negative");
    }
    actions_.reserve(static_cast<std::size_t>(action_count));
    scenarios_.reserve(static_cast<std::size_t>(sample_count));
    average_scores_.reserve(static_cast<std::size_t>(action_count));
  }

  const std::vector<Action>& last_actions() const { return actions_; }
  const std::vector<Scenario>& last_scenarios() const { return scenarios_; }
  const std::vector<long double>& last_average_scores() const {
    return average_scores_;
  }
  std::mt19937_64& engine() { return engine_; }

 private:
  Problem& problem_;
  std::mt19937_64 engine_;
  bool maximize_;
  std::vector<Action> actions_;
  std::vector<Scenario> scenarios_;
  std::vector<long double> average_scores_;
};
