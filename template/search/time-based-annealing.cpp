#include <bits/stdc++.h>
using namespace std;

// 提出時は次の1行を、このhppの全文へ置き換える。
#include "library/time-based-simulated-annealing.hpp"

// ============================================================================
// ここから問題ごとに編集する。
// ============================================================================

struct Problem {
  struct State {
    // TODO: 現在解を書く。例: 順列、盤面、現在得点、差分更新用cache。
  };

  struct Move {
    // TODO: 近傍1回分を書く。例: swapする2添字、変更前後の値。
  };

  // TODO: 得点型を選ぶ。Runnerでは大きいほど良い値にする。
  using Score = long long;

  // TODO: 入力と、全Stateで共通の事前計算結果をここへ置く。

  optional<Move> propose_move(
      const State&, mt19937_64&, double /* progress */) const {
    // TODO: 次に試す近傍を1個返す。作れない試行はnulloptを返す。
    return nullopt;
  }

  Score evaluate_move(const State&, const Move&) const {
    // TODO: Moveによる改善量を返す。正なら良化、負なら悪化。
    // State全体を作り直さず、変更箇所だけから計算すると速い。
    return 0;
  }

  void apply_move(State&, const Move&) const {
    // TODO: 採用されたMoveだけをStateへ反映する。cacheも忘れず更新する。
  }
};

Problem::State make_initial_state(const Problem&) {
  // TODO: 必ず合法な初期解を返す。
  return {};
}

Problem::Score calculate_initial_score(
    const Problem&, const Problem::State&) {
  // TODO: 初期解の得点を全計算して返す。
  return 0;
}

void print_answer(const Problem&, const Problem::State&) {
  // TODO: Stateを問題指定の形式で出力する。
}

// ============================================================================
// ここまでが主な編集場所。下は探索の呼び出し。
// ============================================================================

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  Problem problem;  // TODO: 必要なら入力を読んでProblemへ渡す。
  Problem::State initial = make_initial_state(problem);
  const Problem::Score initial_score =
      calculate_initial_score(problem, initial);

  TimeBasedAnnealingRunner<Problem> runner(
      problem, initial, initial_score,
      1900.0,       // TODO: 制限時間[ms]
      1000.0, 1.0,  // TODO: 開始温度、終了温度
      123, 64);     // TODO: seed、時計を見る間隔
  runner.run();
  print_answer(problem, runner.best_state());
}
