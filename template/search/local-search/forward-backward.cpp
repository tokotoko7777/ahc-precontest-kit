// GCC提出用の最適化。診断時は -DAHC_DISABLE_GCC_OPTIMIZE で無効化。
#if defined(__GNUC__) && !defined(__clang__) && !defined(AHC_DISABLE_GCC_OPTIMIZE)
#pragma GCC optimize("O3")
#endif

// 有限状態の確率DPを前後の表に分け、1操作変更を評価する焼きなまし。

#include <bits/stdc++.h>
using namespace std;
// 提出時はそれぞれのhpp全文を貼り付ける。依存するローカルファイルを残さない。
#include "library/forward-backward-dp.hpp"
#include "library/time-based-simulated-annealing.hpp"

// TODO: 固定長の操作列、有限状態、確率遷移、加算報酬を持つ問題向け。
// 通常の盤面SAなら同じフォルダのbasic.cppの方が簡単。
struct Problem {
  using Action = int; // TODO: 1時刻の操作。structでもよい。
  using Score = double;
  struct State { vector<Action> actions; Score score = 0; };
  struct Move { size_t position; Action action; };
  int state_count; // TODO: 確率DPの状態数（盤面のマス数など）。
  ForwardBackwardDP<Action, Score> cache;
  explicit Problem(int states) : state_count(states), cache(states) {}

  template <class Emit>
  void transitions(size_t turn, int from, const Action& action, Emit emit) const {
    // TODO: 各遷移について emit(遷移先, 遷移確率, その場合の即時報酬)を呼ぶ。
    // fromは状態番号、turnは0始まり。報酬には確率をまだ掛けない。
    // 終了なら遷移先=-1。確率の合計は1にする。以下は何も起こらない初期形。
    (void)turn; (void)action;
    emit(from, 1.0, 0.0);
  }
  auto edges() const {
    return [this](size_t turn, int from, const Action& action, auto emit) {
      transitions(turn, from, action, emit);
    };
  }
  State make_initial_state() {
    State state;
    // TODO: 初期操作列を作る。例: state.actions.assign(200, 初期操作);
    vector<Score> initial(state_count, 0);
    initial[0] = 1; // TODO: 初期状態の確率を置く。合計1。
    cache.build(initial, state.actions, edges());
    state.score = cache.score();
    return state;
  }
  optional<Move> propose_move(const State& state, mt19937_64& random, double) const {
    // TODO: 変更位置と新しい操作を1個返す。固定長の1操作置換のみ。
    // 作れない試行はnullopt。stateやcacheを書き換えない。
    (void)state; (void)random;
    return nullopt;
  }
  optional<Score> evaluate_move(const State& state, const Move& move, double) const {
    // 候補の絶対期待値から現在値を引き、SAへは改善量を返す。
    // 最小化する問題なら、Runnerに渡す初期値も含めて符号を反転する。
    return cache.score_if_changed(move.position, move.action, edges()) - state.score;
  }
  void apply_move(State& state, Move& move) {
    cache.commit_change(move.position, move.action, edges());
    state.actions[move.position] = move.action;
    state.score = cache.score();
  }
};

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);
  // TODO: 入力を読み、DPの状態数を指定する。ここでは仮に1状態。
  Problem problem(1);
  auto initial = problem.make_initial_state();
  TimeBasedAnnealingRunner<Problem> runner(problem, initial, initial.score,
      1900.0, 100.0, 0.1, 123, 16); // TODO: 時間と温度を得点の単位に合わせる。
  runner.run();
  // TODO: runner.best_state().actionsを問題の出力形式で出す。
  // cacheは現在解専用。runner.restart_from_best()を使う場合はcacheも再buildする。
}
