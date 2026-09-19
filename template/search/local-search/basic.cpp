// GCC提出用の最適化。診断時は -DAHC_DISABLE_GCC_OPTIMIZE で無効化。
#if defined(__GNUC__) && !defined(__clang__) && !defined(AHC_DISABLE_GCC_OPTIMIZE)
#pragma GCC optimize("O3")
#endif

#include <bits/stdc++.h>
using namespace std;
// 提出時はこのincludeをhpp全文へ置き換える。
#include "library/time-based-simulated-annealing.hpp"

// ==================== 問題ごとに編集する場所 ====================
constexpr bool USE_ANNEALING = true; // TODO: trueなら焼きなまし、falseなら山登り。
// State・近傍・差分評価・更新は共通。変えるのは採用判定だけ。
struct Problem {
  struct State {
    // TODO: 現在解と差分更新用cacheを書く。例: 順列・逆引き位置・現在の距離。
  };
  struct Move {
    // TODO: 1回の変更を書く。例: swapする2添字。必要なら計算済みの差分も持つ。
    // 大きな破壊・再構築も1つのMoveとして扱える。
  };
  using Score = long long; // TODO: 得点の型。小数ならdoubleなどへ変更する。
  // TODO: 入力・全状態で共通の前計算を書く。

  optional<Move> propose_move(const State&, mt19937_64&, double /* progress */) const {
    // TODO: 近傍を1個作る。作れない場合はnullopt。現在Stateを変更しない。
    // progressは0〜1。必要なときだけ近傍の大きさなどに使う。
    return nullopt;
  }

  optional<Score> evaluate_move(
      const State&, const Move&, double /* threshold */) const {
    // TODO: この変更による「改善量」を返す。変更後の絶対得点ではない。
    // 最大化なら変更後-変更前。コスト最小化なら変更前cost-変更後cost。
    // 正なら良化。変更箇所だけから差分計算し、State/cacheは書き換えない。
    // 不合法な変更はnullopt。まずthresholdは無視して正確な差分を返せばよい。
    // 任意の枝刈り: 最終改善量の上限<=thresholdと証明できる場合だけnullopt。
    return Score{0};
  }

  void apply_move(State&, Move&) const {
    // TODO: 採用されたMoveだけを反映し、逆引き・得点などのcacheも更新する。
    // 不採用時はStateを変更していないのでundoは不要。
  }
};

Problem::State make_initial_state(const Problem&) {
  // TODO: 合法な初期解を返す。
  return {};
}
Problem::Score calculate_initial_score(const Problem&, const Problem::State&) {
  // TODO: 初期解の絶対得点を返す。最大化は得点、最小化は-cost。
  // デバッグ時は、この全計算で差分更新の正しさを確認する。
  return 0;
}
void print_answer(const Problem&, const Problem::State&) {
  // TODO: 問題指定の形式で出力する。
}

// ==================== 探索を呼ぶ場所 ====================
int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);
  Problem problem; // TODO: 入力を読む。
  auto initial = make_initial_state(problem);
  const auto score = calculate_initial_score(problem, initial);
  TimeBasedAnnealingRunner<Problem> search(
      problem, initial, score,
      1900.0,      // TODO: 探索時間[ms]。初期化・出力分の余裕を残す。
      1000.0, 1.0, // TODO: 焼きなましの開始・終了温度（正）。山登りなら変更不要。
      123, 64);    // 乱数seed・時計確認間隔。重い近傍では間隔を1へ。
  if constexpr (USE_ANNEALING) {
    search.run_with_threshold();
  } else {
    search.run_hill_climbing(); // 改善量>0だけ採用。同点も棄却。温度は採否に使わない。
  }
  print_answer(problem, search.best_state());
}
