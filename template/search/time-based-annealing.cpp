#include <bits/stdc++.h>
using namespace std;

// 提出時は次の1行を、このhppの全文へ置き換える。
#include "library/time-based-simulated-annealing.hpp"

// ============================================================================
// ここから問題ごとに編集する。
// ============================================================================

// TODO: この2つは独立してON/OFFできる。4通りすべて使える。
constexpr bool PRECOMPUTE_ACCEPTANCE = false; // 対数表を作る。問題固有の前計算とは別。
constexpr bool STOP_SCORE_EARLY = true;      // 閾値に届かないと判明したら評価を止める。

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

  optional<Score> evaluate_move_with_threshold(
      const State& state,
      const Move& move,
      double acceptance_threshold) const {
    // TODO: 【任意・高速化】改善量を部分ごとに計算する。
    // 「残りを最良に見積もっても improvement > acceptance_threshold に
    // ならない」と証明できた時だけnulloptを返す。そうでなければ最後まで
    // 計算して正確な改善量を返す。下は枝刈りしない安全な初期形。
    //
    // 例: 全利得gainが先に分かり、あとは非負の損失だけを引く場合:
    //   Score delta = gain;
    //   for (各損失項) {
    //     delta -= その項の非負の損失;
    //     if (double(delta) <= acceptance_threshold) return nullopt;
    //   }
    //   return delta;  // 完走した場合だけ正確な差分を返す。
    //
    // 残りに正の利得もある場合は、deltaだけで切らない。
    // delta + 残り利得の上限 <= acceptance_threshold と証明できた時だけ切る。
    // 途中値を「正確な得点」として返さない。State/cacheも変更しない。
    // 前計算がOFFでも、この打ち切りは使える。
    (void)acceptance_threshold;
    return evaluate_move(state, move);
  }

  void apply_move(State&, Move&) const {
    // TODO: 採用されたMoveだけをStateへ反映する。cacheも忘れず更新する。
    // Moveにvector等で次状態を作った場合は、ここでmoveしてよい。
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
  runner.annealing().set_threshold_precomputation(PRECOMPUTE_ACCEPTANCE);
  // OFFならevaluate_move、ONならevaluate_move_with_thresholdを呼ぶ。
  // run()へ替えると乱数の消費方法も変わるので、ON/OFF比較にはこちらを使う。
  runner.run_with_threshold(STOP_SCORE_EARLY);
  print_answer(problem, runner.best_state());
}
