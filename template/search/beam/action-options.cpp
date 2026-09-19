// GCC提出用の最適化。診断時は -DAHC_DISABLE_GCC_OPTIMIZE で無効化。
#if defined(__GNUC__) && !defined(__clang__) && !defined(AHC_DISABLE_GCC_OPTIMIZE)
#pragma GCC optimize("O3")
#endif

#include <bits/stdc++.h>
using namespace std;

// 提出時はライブラリのincludeをhpp全文へ置き換える。
#include "library/action-beam-search.hpp"

// ============================================================================
// ここから問題ごとに編集する。
// ============================================================================

struct Problem {
  struct State {
    // TODO: 探索途中の解と差分評価用cacheを書く。
    long long rank_score = 0;
  };

  struct Action {
    // TODO: 次の1手だけを書く。全候補ぶん持つため小さくする。
  };

  // TODO: 候補順位の型を選ぶ。既定では大きいほど良い。
  using Score = long long;

  // TODO: 入力と、全Stateで共通の事前計算結果をここへ置く。

  vector<Action> generate_actions(const State&) const {
    // TODO: 現在Stateから試せる合法Actionを全て返す。
    return {};
  }

  Score evaluate_action(const State&, const Action&) const {
    // TODO: Action適用後の「子Stateの順位値そのもの」を返す。
    // 全候補に呼ばれるので、Stateを変更せず差分計算する。
    return 0;
  }

  optional<Score> evaluate_action_with_threshold(
      const State& state,
      const Action& action,
      const Score* threshold) const {
    // TODO: 【任意・高速化】重い順位計算を少しずつ行う。
    // threshold==nullptr の間は上位N件の境界が未確定なので、必ず正確な
    // Scoreを返す。非nullなら、最大化では「残りを全部足しても
    // *thresholdを超えない」と証明できた時だけnulloptを返してよい。
    // 最小化なら大小を逆に考える。証明できなければ正確なScoreを返す。
    // 下の実装は枝刈りしない安全な初期形。まずこれで動かしてよい。
    (void)threshold;
    return evaluate_action(state, action);
  }

  void apply_action(State&, Action&) const {
    // TODO: 採用されたActionだけを、コピー済みStateへ反映する。
  }

  template <class Emit, class CanImprove>
  void enumerate_actions(const State& state, Emit emit, CanImprove can_improve) const {
    // TODO: 【任意・生成前の枝刈り】run_with_generator()を使う場合だけ編集。
    // emit(Action, 正確な子の絶対評価値)で候補を1個渡す。
    // can_improve(上限)がfalseなら、その候補を作らなくてよい（最小化は下限）。
    // 残り全部の上限ならbreakできる。1候補の不採用だけでbreakしない！
    // 現在の実装は通常版と同じ全列挙なので、まずこのまま動かせる。
    (void)can_improve;
    for (const auto& action : generate_actions(state)) {
      emit(action, evaluate_action(state, action));
    }
  }

  State make_initial_state() const {
    // TODO: 初期Stateを返す。
    return {};
  }

  Score initial_score(const State& state) const {
    // TODO: 初期Stateの順位値を返す。
    return state.rank_score;
  }
};

void print_answer(const Problem&, const Problem::State&) {
  // TODO: 完成したStateを問題指定の形式で出力する。
}

// ============================================================================
// ここまでが主な編集場所。下は探索の呼び出し。
// ============================================================================

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  constexpr int BEAM_WIDTH = 100;  // TODO: ビーム幅。
  constexpr int MAX_TURN = 100;    // TODO: 最大世代数。
  Problem problem;                 // TODO: 必要なら入力を読んで渡す。
  Problem::State initial = problem.make_initial_state();
  ActionBeamRunner<Problem> beam(
      problem, initial, problem.initial_score(initial), BEAM_WIDTH);
  // evaluate_actionが十分軽いならbeam.run(MAX_TURN)でもよい。
  // 候補生成前にも枝刈りするならbeam.run_with_generator(MAX_TURN)へ替える。
  beam.run_with_threshold(MAX_TURN);
  print_answer(problem, beam.best());
}
