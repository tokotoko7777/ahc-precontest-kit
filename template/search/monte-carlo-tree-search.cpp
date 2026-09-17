#include <bits/stdc++.h>
using namespace std;
#include "library/monte-carlo-tree-search.hpp" // 提出時はhpp全文を貼る。

// ==================== 問題ごとに編集する場所 ====================
struct Problem {
  using Action = int; // TODO: 1手の型。Stateを変更する操作を書く。
  struct State {
    // TODO: 今までに観測済みの情報・盤面・手数を書く。未公開の実入力は入れない。
  };
  void read_input() { /* TODO: 事前に公開された入力だけを読む。 */ }
  State read_state() { return {}; /* TODO: 現時点で公開された実状態を作る。 */ }
  vector<Action> generate_actions(const State&) const {
    // TODO: この状態で合法な手を返す。空なら選択結果はnullopt。
    return {};
  }
  bool is_terminal(const State&) const { return false; /* TODO: 終局ならtrue。 */ }
  uint64_t sample_transition(State& state, const Action& action, mt19937_64& rng) const {
    // TODO: actionをstateへ適用し、必要なら未知の結果をrngで抽選して反映する。
    // 戻り値: 同じ親・同じ手から出た「結果」の正確なID。
    // 決定的なら0。ランダム配置なら配置マス/空きマス順位など。
    // 違う次状態を同じIDにしない。複数の抽選結果なら組を衝突なく符号化する。
    // 未知の将来の実入力を先読みして渡すのは不可。シミュレーション用乱数で作る。
    (void)state; (void)action; (void)rng;
    return 0;
  }
  double rollout(State state, mt19937_64& rng) const {
    // TODO: stateから軽い方策で終局/決めた深さまで仮実行し、評価を[0,1]で返す。
    // 終局Stateが渡されることもある。その場合は仮実行せず最終評価を返す。
    // 例: score / 上限値。最小化でも正規化値を返し、options.maximize=falseにする。
    // 1回が長すぎると制限時間を超えるので、必要なら手数上限を設ける。
    (void)state; (void)rng;
    return 0;
  }
  void output_action(Action) { /* TODO: 選んだ手を出力。対話問題はflushする。 */ }
};
// ==================== 探索本体は編集しない ====================
int main() {
  Problem problem;
  problem.read_input();
  MonteCarloTreeSearch<Problem> search(problem, 123);
  MctsOptions options;
  options.time_limit_ms = 15; // TODO: 1手の時間。全ターン分の制限に注意！
  options.exploration = 0.5; // TODO: 未探索の手を試す強さ。値域[0,1]を守る。
  options.max_nodes = 10000; // TODO: 1回の探索で持つ木の大きさ。
  options.max_depth = 100; // TODO: 木をたどる最大手数。続きはrollout。
  auto action = search.choose_action(problem.read_state(), options);
  if (action) problem.output_action(*action);
  // TODO: 対話問題ならこのread_state→choose_action→output_actionを各ターンで繰り返す。
  // 現実の観測結果が変わるため、choose_actionごとに木は作り直す。
}
