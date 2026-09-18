// GCC提出用の最適化。診断時は -DAHC_DISABLE_GCC_OPTIMIZE で無効化。
#if defined(__GNUC__) && !defined(__clang__) && !defined(AHC_DISABLE_GCC_OPTIMIZE)
#pragma GCC optimize("O3")
#endif

#include <bits/stdc++.h>
using namespace std;

// 提出時は次の1行を、このhppの全文へ置き換える。
#include "library/tree-beam-search.hpp"

// ============================================================================
// ここから問題ごとに編集する。
// ============================================================================

struct Problem {
  struct State {
    // TODO: DFS中に1個だけ持つ全状態と差分更新用cacheを書く。
  };

  struct Move {
    // TODO: 1手と、revertに必要な変更前の情報を書く。
  };

  // TODO: 候補順位の型を選ぶ。既定では大きいほど良い。
  using Score = long long;

  // TODO: 入力と、全Stateで共通の事前計算結果をここへ置く。

  vector<Move> generate_moves(const State&) const {
    // TODO: 現在Stateから試す合法Moveを全て返す。
    return {};
  }

  void apply_move(State&, Move&) const {
    // TODO: Moveを1手進め、盤面・score・hash・cacheを差分更新する。
    // revert用の旧値が必要ならMoveへ保存してよい。
  }

  void revert_move(State&, const Move&) const {
    // TODO: apply_move直前と完全に同じStateへ戻す。
  }

  Score evaluate(const State&) const {
    // TODO: 現在Stateの順位値そのものを返す。
    return 0;
  }

  uint64_t make_key(const State&) const {
    // TODO: 【重複除去する場合だけ】同じ局面で同じ値になるkeyを返す。
    return 0;
  }

  State make_initial_state() const {
    // TODO: 初期Stateを返す。
    return {};
  }
};

void print_answer(const Problem&, const vector<Problem::Move>&) {
  // TODO: 復元されたMove列を問題指定の形式で出力する。
}

// ============================================================================
// ここまでが主な編集場所。下は探索の呼び出し。
// ============================================================================

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  constexpr int BEAM_WIDTH = 100;  // TODO: ビーム幅。
  constexpr int MAX_TURN = 100;    // TODO: 最大世代数。
  constexpr bool USE_KEY = false;  // TODO: hash重複除去を使うか。
  Problem problem;                 // TODO: 必要なら入力を読んで渡す。
  Problem::State initial = problem.make_initial_state();
  TreeBeamRunner<Problem> beam(
      problem, initial, problem.evaluate(initial), BEAM_WIDTH);
  if (USE_KEY) {
    beam.run_with_key(MAX_TURN);
  } else {
    beam.run(MAX_TURN);
  }
  print_answer(problem, beam.restore());
}
