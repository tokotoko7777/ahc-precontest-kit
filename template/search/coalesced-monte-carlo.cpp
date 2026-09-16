#include <iostream>
#include <random>
#include <vector>
#include "library/common-scenario-average.hpp"
#include "library/coalesced-rollout.hpp"

// 同じ途中状態以降を共有するMonte Carlo。編集はProblemへ集める。
// StateやActionが将来に与える影響をRolloutStateへ全て含めること。
// 普通のrolloutで十分ならmonte-carlo-rollout.cppを使えばよい。
struct Problem {
  struct State {
    // TODO: 現在までに観測した情報を書く。未来の入力を先読みしない。
  };
  using Action = int; // TODO: 今比較する最初の1手の型。
  using Score = long long; // TODO: 最後の評価の型。大きいほど良い。
  struct Scenario {
    // TODO: 自分で決められない未来を1本分保存する。
    int steps = 0; // TODO: 未来の遷移数。全候補で同じ段階を比較する。
  };
  struct RolloutState {
    // TODO: 仮実行の全状態を書く。未来に影響する履歴や方策のモードも含める。
    bool operator==(const RolloutState&) const {
      // TODO: 同じ未来・同じ最終評価になる時だけtrueを返す。
      // 分からない間はfalseのままでよい（共有しないが全候補を評価できる）。
      return false;
    }
  };

  // TODO: 入力や不変の前計算をここへ置く。
  std::vector<Action> generate_actions(const State&) const {
    // TODO: 今選べる合法Actionを列挙。順序は同点時の優先順。必ず1個以上。
    // 同じStateには同じ順序・内容を返す（外部の乱数を消費しない）。
    return {0};
  }
  Scenario generate_scenario(const State&, std::mt19937_64&) const {
    // TODO: 未知情報だけを乱数でsampleする。全Actionに同じScenarioを使う。
    return {};
  }
  RolloutState start_rollout(const State&, Action) const {
    // TODO: 最初のActionを適用した仮状態を返す。実Stateは変更しない。
    return {};
  }
  void advance_rollout(RolloutState&, const Scenario&, int /* step */) const {
    // TODO: Scenarioのstep番の未来を使い、仮状態を1段進める。
    // 未来の自分の行動は決定的な方策で決める。外部に副作用を出さない。
  }
  Score evaluate_rollout(const RolloutState&) const {
    // TODO: 全段進めた状態の最終評価値そのものを返す。差分ではない。
    return 0;
  }
};

int main() {
  // TODO: 入力を読みProblemと実Stateを作る。
  Problem problem;
  Problem::State state;
  constexpr int TURNS = 0; // TODO: 判断する回数。
  constexpr int SAMPLES = 100; // TODO: 同じ制限時間での実問題scoreを見て調整。
  constexpr bool SHARE_EQUAL_STATES = true; // TODO: 必要ならOFFと得点一致を比較。
  CommonScenarioRolloutRunner<Problem> runner(problem, 123);
  CoalescedRollout<Problem::RolloutState, Problem::Score> shared;
  std::vector<Problem::RolloutState> initial;
  for (int turn = 0; turn < TURNS; ++turn) {
    // TODO: このターンの入力だけを読み、stateへ反映する。
    initial.clear();
    for (auto action : problem.generate_actions(state)) {
      initial.push_back(problem.start_rollout(state, action));
    }
    const auto action = runner.choose_action_batched(state, SAMPLES,
        [&](const auto&, const auto&, const Problem::Scenario& scenario) -> const std::vector<Problem::Score>& {
      return shared.evaluate<SHARE_EQUAL_STATES>(initial, scenario.steps,
          [&](auto& simulation, int step) { problem.advance_rollout(simulation, scenario, step); },
          [&](const auto& simulation) { return problem.evaluate_rollout(simulation); });
    });
    // TODO: actionを本番Stateへ適用し、指定形式で出力する。対話問題はflushする。
    (void)action;
  }
}
