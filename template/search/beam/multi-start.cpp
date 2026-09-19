// GCC提出用の最適化。診断時は -DAHC_DISABLE_GCC_OPTIMIZE で無効化。
#if defined(__GNUC__) && !defined(__clang__) && !defined(AHC_DISABLE_GCC_OPTIMIZE)
#pragma GCC optimize("O3")
#endif

// 共通の締切内で、初期条件を変えてビームサーチを複数回試す。

#include <bits/stdc++.h>
using namespace std;
// 提出時は各includeをhpp全文へ置換する。
#include "library/tree-beam-search.hpp"
#include "library/multi-start.hpp"
#include "library/timer.hpp"

// ==================== 問題ごとに編集する場所 ====================
struct Problem {
  struct State {
    // TODO: 盤面と差分更新する評価・hash・cacheを書く。
  };
  struct Move {
    // TODO: 1手とundo用の旧値を書く。
  };
  using Score = long long; // TODO: 途中の候補順位の型。ここでは最大化。
  // TODO: 全状態で共通の入力・前計算を書く。
  vector<Move> generate_moves(const State&) const {
    // TODO: 合法な次の手を返す。
    return {};
  }
  void apply_move(State&, Move&) const {
    // TODO: 1手進めて盤面・評価・cacheを差分更新する。
  }
  void revert_move(State&, const Move&) const {
    // TODO: apply直前の全状態へ戻す。
  }
  Score evaluate(const State&) const {
    // TODO: 探索中の順位値を返す。差分ではない。
    return 0;
  }
  State initial_state(int trial) const {
    // TODO: trial（0始まり）ごとに初期状態やseedを変える。
    // 同じ決定的探索の繰り返しは無意味。対称変換を使うなら答えを元へ戻す。
    (void)trial;
    return {};
  }
  long long final_score(const State&) const {
    // TODO: 完成解の正式得点を返す。途中の近似評価と異なってもよい。
    return 0;
  }
};
struct Answer {
  vector<Problem::Move> moves;
  long long score = 0; // TODO: 正式得点の型を選ぶ。
};
Answer make_fallback(const Problem&) {
  // TODO: 時間がなくても提出できる合法解と、その正式得点を返す。
  // 例: 軽い貪欲1回。空の手順が違法な問題では必ず完成解を作る。
  return {};
}
void print_answer(const Answer&) {
  // TODO: 元の座標系・問題指定の形式で出力する。
}

// ==================== 探索を呼ぶ場所 ====================
int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);
  Timer timer; // fallbackも全体時間に含める。
  Problem problem; // TODO: 入力を読む。
  constexpr int TRIALS = 3, WIDTH = 100, TURNS = 100; // TODO: 試行数・幅・世代数。
  auto stop = [&] { return timer.is_over(1800.0); }; // TODO: 復元・出力分を残す。
  Answer fallback = make_fallback(problem);
  auto generate = [&](int trial, auto& should_stop) -> optional<Answer> {
    auto initial = problem.initial_state(trial);
    TreeBeamRunner<Problem> beam(problem, initial, problem.evaluate(initial), WIDTH);
    for (int turn = 0; turn < TURNS; ++turn) {
      if (should_stop()) return nullopt; // 未完成は採用しない。
      if (!beam.step()) return nullopt;
    }
    int best_rank = 0;
    long long best_score = numeric_limits<long long>::lowest();
    beam.for_each_state([&](int rank, const Problem::State& state) {
      const auto score = problem.final_score(state);
      if (score > best_score) { best_score = score; best_rank = rank; }
    });
    return Answer{beam.restore(best_rank), best_score};
  };
  auto best = budgeted_multi_start(std::move(fallback), TRIALS, generate,
      [](const Answer& answer) { return answer.score; }, stop);
  // 最小化では上の完成候補選択・Runner・budgeted_multi_startの全てを最小化へ揃える。
  print_answer(best);
}
