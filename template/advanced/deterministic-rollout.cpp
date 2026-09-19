// GCC提出用の最適化。診断時は -DAHC_DISABLE_GCC_OPTIMIZE で無効化。
#if defined(__GNUC__) && !defined(__clang__) && !defined(AHC_DISABLE_GCC_OPTIMIZE)
#pragma GCC optimize("O3")
#endif

#include <bits/stdc++.h>
using namespace std;

// 提出時はライブラリのincludeをhpp全文へ置き換える。
#include "library/deterministic-rollout.hpp"

// ============================================================================
// ここから問題ごとに編集する。
// ============================================================================

struct Problem {
  struct State {
    // TODO: 現在の盤面、次の手番、確定済み出力などを書く。
  };

  struct Action {
    // TODO: 今比較する方策パラメータや最初の1手を書く。
  };

  // TODO: 1 rolloutの評価値型を書く。int、long long、doubleなどでよい。
  using Score = long long;

  // TODO: 入力と、全rolloutで共有する事前計算結果をここへ置く。

  vector<Action> generate_actions(const State&) const {
    // TODO: 現在状態で比較したい合法Actionを全て返す。
    // vectorでなくarrayやFixedVectorを返してもよい。
    // 空だとchoose_actionできないので、実問題では1個以上返す。
    return {Action{}};
  }

  Score evaluate_action(const State&, const Action&) const {
    // TODO: 元Stateのコピーを作り、Actionを最初に適用してから、
    // 終端または決めた深さまで同じ軽い方策で仮実行する。
    // 元Stateそのものは変更しない。
    return 0;
  }
};

Problem::State make_initial_state(const Problem&) {
  // TODO: 入力から初期Stateを作る。
  return {};
}

void apply_real_action(
    const Problem&, Problem::State&, const Problem::Action&) {
  // TODO: Runnerが選んだActionを、実際のStateへ1段だけ反映する。
  // 全rolloutの操作列を反映せず「最初の決定だけ」を使うのが基本。
}

bool is_finished(const Problem&, const Problem::State&) {
  // TODO: 全ての意思決定が終わったらtrueを返す。
  return true;
}

void print_answer(const Problem&, const Problem::State&) {
  // TODO: Stateに保存したAction列などを問題指定の形式で出力する。
}

// ============================================================================
// ここまでが主な編集場所。下は探索の呼び出し。
// ============================================================================

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  Problem problem;  // TODO: 必要ならここで入力を読む。
  Problem::State state = make_initial_state(problem);

  // TODO: 大きい評価が良いならtrue、小さいコストが良いならfalse。
  constexpr bool MAXIMIZE = true;
  DeterministicRolloutRunner<Problem> rollout(problem, MAXIMIZE);

  // TODO: 最大Action数が分かればreserveして、毎ターンの再確保を防ぐ。
  rollout.reserve(100);
  while (!is_finished(problem, state)) {
    const Problem::Action action = rollout.choose_action(state);
    apply_real_action(problem, state, action);
  }
  print_answer(problem, state);
}
