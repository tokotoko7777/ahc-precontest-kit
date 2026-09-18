// GCC提出用の最適化。診断時は -DAHC_DISABLE_GCC_OPTIMIZE で無効化。
#if defined(__GNUC__) && !defined(__clang__) && !defined(AHC_DISABLE_GCC_OPTIMIZE)
#pragma GCC optimize("O3")
#endif

#include <bits/stdc++.h>
using namespace std;

// 提出時は次の1行を、このhppの全文へ置き換える。
#include "library/common-scenario-average.hpp"

// ============================================================================
// ここから問題ごとに編集する。
// ============================================================================

struct Problem {
  struct State {
    // TODO: 現在までに確定している実状態を書く。
  };

  struct Action {
    // TODO: 今選ぶ1手を書く。
  };

  struct Scenario {
    // TODO: 自分では決められない未知の未来1本を書く。
  };

  // TODO: 1 rolloutの評価値の型を選ぶ。既定では大きいほど良い。
  using Score = long long;

  // TODO: 入力と、全rolloutで共通の事前計算結果をここへ置く。

  vector<Action> generate_actions(const State&) const {
    // TODO: 今比較する合法Actionを全て返す。
    // 空だとchoose_actionできないため、実問題では1個以上返す。
    return {Action{}};
  }

  Scenario generate_scenario(const State&, mt19937_64&) const {
    // TODO: 未知情報だけを乱数で1本sampleして返す。
    return {};
  }

  Score evaluate_action(
      const State&, const Action&, const Scenario&) const {
    // TODO: 最初のActionを適用し、その後を終端までsimulationして評価する。
    return 0;
  }
};

Problem::State make_initial_state(const Problem&) {
  // TODO: 初期Stateを返す。
  return {};
}

void apply_real_action(
    const Problem&, Problem::State&, const Problem::Action&) {
  // TODO: 選ばれたActionを、本番用の実Stateへ反映する。
}

void print_answer(const Problem&, const Problem::State&) {
  // TODO: 保存したAction列などを問題指定の形式で出力する。
}

// ============================================================================
// ここまでが主な編集場所。下は探索の呼び出し。
// ============================================================================

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  constexpr int TURN_COUNT = 0;    // TODO: 意思決定するターン数。
  constexpr int SAMPLE_COUNT = 100; // TODO: 1手あたりの未来sample数。
  Problem problem;                  // TODO: 必要なら入力を読んで渡す。
  Problem::State state = make_initial_state(problem);
  CommonScenarioRolloutRunner<Problem> rollout(problem, 123);
  for (int turn = 0; turn < TURN_COUNT; ++turn) {
    Problem::Action action = rollout.choose_action(state, SAMPLE_COUNT);
    apply_real_action(problem, state, action);
  }
  print_answer(problem, state);
}
