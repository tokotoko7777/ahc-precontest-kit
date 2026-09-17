#include <bits/stdc++.h>
using namespace std;
#include "library/chokudai-search.hpp" // 提出時はhpp全文をここに貼る。

// ==================== 問題ごとに編集する場所 ====================
struct Problem {
  using Score = long long; // TODO: 順位値・最終値の型を書く。double等も可。
  using Action = int; // TODO: 1手を書く。移動方向、選ぶ品物の番号など。
  struct State {
    // TODO: 盤面・手数・答えを復元する操作列・差分評価cacheなどを書く。
    // 通常のコピーで独立した状態になる値型にする。
  };
  void read_input() { /* TODO: 入力と必要な前計算を書く。 */ }
  State initial_state() const { return {}; /* TODO: 探索の開始状態。 */ }
  State fallback_answer() const { return {}; /* TODO: 時間切れでも出せる合法な完成解。 */ }
  vector<Action> generate_actions(const State&) const {
    // TODO: この状態で可能な1手の一覧を返す。共有配列のconst参照でもよい。
    return {};
  }
  Score evaluate_action(const State&, const Action&) const {
    // TODO: 1手適用後の順位値そのものを返す。改善量ではない。
    // Stateを作らず計算する。評価関数は深さが同じ候補同士だけを比較する。
    return 0;
  }
  void apply_action(State&, Action&) const {
    // TODO: 採用候補のStateへ実際に1手を反映し、操作列・cacheも更新する。
  }
  bool is_terminal(const State&) const {
    // TODO: 出力できる完成解ならtrue、探索途中ならfalseを返す。
    return false;
  }
  Score final_score(const State&) const {
    // TODO: 完成解の本当の得点を返す。途中の順位値と混同しない。
    return 0;
  }
  void print_answer(const State&) const { /* TODO: 問題指定の出力を書く。 */ }
};
// ==================== 探索本体は編集しない ====================
int main() {
  Problem problem;
  problem.read_input();
  auto fallback = problem.fallback_answer();
  ChokudaiOptions options;
  options.max_depth = 100; // TODO: 完成までに必要な最大の手数。
  options.capacity_per_depth = 256; // TODO: 各深さに残す候補の上限。RAMと相談。
  options.time_limit_ms = 1800; // TODO: 前計算・出力時間を別に確保する。
  options.maximize = true; // TODO: 最小化ならfalse。
  ChokudaiSearch<Problem> search(problem, problem.initial_state(), 0, options);
  search.run();
  problem.print_answer(search.best_state() ? *search.best_state() : fallback);
}
