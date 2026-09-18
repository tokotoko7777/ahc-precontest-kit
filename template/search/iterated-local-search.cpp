// GCC提出用の最適化。診断時は -DAHC_DISABLE_GCC_OPTIMIZE で無効化。
#if defined(__GNUC__) && !defined(__clang__) && !defined(AHC_DISABLE_GCC_OPTIMIZE)
#pragma GCC optimize("O3")
#endif

#include <bits/stdc++.h>
using namespace std;
#include "library/iterated-local-search.hpp" // 提出時はhpp全文を貼る。

// ==================== 問題ごとに編集する場所 ====================
struct Problem {
  using Score = long long; // TODO: int / double等でもよい。
  struct State { /* TODO: 解と差分計算cache。コピーで独立する値型にする。 */ };
  void read_input() { /* TODO: 入力・前計算。 */ }
  State initial_state() const { return {}; /* TODO: 合法な完成解を返す。 */ }
  Score evaluate(const State&) const { return 0; /* TODO: 絶対スコアを返す。 */ }
  void perturb(const State& current, State& candidate, mt19937_64& rng, const IlsBudget& budget) {
    candidate = current; // TODO: currentを変更せず、candidateへ大きな変更を入れる。
    // 例: 何点かを入れ替える、経路を大きくつなぎ替える、一部を壊して修復する。
    // 戻る時点で必ず合法な解にする。candidateには前回の残骸があるので上書きする。
    (void)rng; (void)budget;
  }
  Score local_search(State& candidate, mt19937_64& rng, const IlsBudget& budget) {
    // TODO: 小さい変更を繰り返し、candidateをその場で改善する。
    // 例: 改善するswapがなくなるまで / 連続失敗64回まで。
    // 重いループの途中にもbudget.expired()の確認を入れる。
    // 時間切れでも合法な解と整合したcacheを残して戻ること。
    // TODO(任意): 差分の受理閾値を評価へ渡し、改善不能と証明できた時だけ打ち切る。
    // 問題ごとに必要な前計算の有無とは独立してよい。
    (void)rng; (void)budget;
    return evaluate(candidate); // 必ず完成解の絶対値。改善量ではない。
  }
  void print_answer(const State&) const { /* TODO: 最良解を問題指定の形式で出力。 */ }
};
// ==================== 探索本体は編集しない ====================
int main() {
  Problem problem;
  problem.read_input();
  auto initial = problem.initial_state();
  const auto score = problem.evaluate(initial);
  IlsOptions options;
  options.time_limit_ms = 1800; // TODO: 入出力・前計算の余裕を残す。
  options.maximize = true; // TODO: 最小化ならfalse。負号を付ける必要はない。
  options.accept_worse = false; // TODO: trueなら悪化した局所解からも探索を続ける。
  options.restart_after = 20; // TODO: 最良更新がない外側の反復数。0で無効。
  IteratedLocalSearch<Problem> search(problem, std::move(initial), score, options);
  search.run();
  problem.print_answer(search.best_state());
}
