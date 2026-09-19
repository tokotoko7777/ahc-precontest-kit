// GCC提出用の最適化。診断時は -DAHC_DISABLE_GCC_OPTIMIZE で無効化。
#if defined(__GNUC__) && !defined(__clang__) && !defined(AHC_DISABLE_GCC_OPTIMIZE)
#pragma GCC optimize("O3")
#endif

// 部分破壊・再構築を近傍にした局所探索。初期設定は山登り、焼きなましにも切替可能。

#include <bits/stdc++.h>
using namespace std;

// 提出時はこのincludeをhpp全文へ置き換える。別ファイルの提出は不要。
#include "library/large-neighborhood-search.hpp"

// ==================== ここから問題ごとに編集する ====================
struct Problem {
  using Score = long long; // TODO: int / long long / double等の得点型を選ぶ。
  struct State {
    // TODO: 解を書く。例: vector<int> order; 盤面、差分計算用cacheなど。
    // Stateは値型にする。浅いコピーで同じ盤面を共有するポインタは避ける。
  };
  // TODO: 入力、全解で共有する前計算、壊した箇所の一覧等の作業bufferを置く。

  void read_input() {
    // TODO: cinから入力を読み、必要なら距離表などを前計算する。
    // 前計算が使えない問題では省略してよい。閾値打ち切りとは独立。
  }
  State make_initial_state() const {
    // TODO: 必ず合法な初期解を返す。全要素を割り当てる単純貪欲等でよい。
    return {};
  }
  Score evaluate(const State&) const {
    // TODO: 完成解の絶対スコアを返す。改善量ではない。
    return 0;
  }
  void destroy(const State& current, State& candidate,
               mt19937_64& rng, double progress) {
    // TODO: currentの一部を壊した状態をcandidateへ書く。
    // 例: 経路の数点を除く、一部の割当を未割当へ戻す、操作列の区間を消す。
    // currentは変更しない。candidateには「前回の残骸」があるので必ず上書きする。
    candidate = current; // 最初は全コピーでOK。重ければ必要部分だけ詰め直す。
    // TODO: rng()で壊す場所を選ぶ。progressは0(開始)〜1(終了直前)。
    // 序盤は広く、終盤は狭く壊す等に使える。不要なら無視してよい。
    (void)rng; (void)progress;
  }
  optional<Score> repair(State& candidate, mt19937_64& rng,
                         double progress, long double threshold) {
    // TODO: 壊した部分を貪欲・DP・DFS・フローなどで埋め、合法な完成解へ戻す。
    // 成功 → 完成したcandidateの「絶対スコア」を返す（evaluate(candidate)等）。
    // 失敗 → nullopt。candidateが壊れたままでもcurrent/bestには影響しない。
    //
    // TODO(任意): thresholdで修復や評価を途中打ち切りする。
    // 最大化: 最良に修復しても score < threshold と証明できたらnullopt。
    // 最小化: 最良に修復しても score > threshold と証明できたらnullopt。
    // 等号は採用されるので切らない。「途中値」だけで判定せず上下界を使う。
    // 証明できなければ閾値を無視して最後まで計算してよい。
    // 打ち切りOFFなら-inf(最大化) / +inf(最小化)が渡される。
    (void)candidate; (void)rng; (void)progress; (void)threshold;
    return nullopt; // TODO: 修復を実装したらreturn evaluate(candidate)等へ置換。
  }
  void print_answer(const State&) const {
    // TODO: 問題指定の形式で解を出力する。ログはcerrへ。
  }
};
// ==================== ここまでが主な編集箇所 ====================

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);
  Problem problem;
  problem.read_input();
  auto initial = problem.make_initial_state();
  const auto initial_score = problem.evaluate(initial);
  LnsOptions options;
  options.time_limit_ms = 1800; // TODO: 入力・前計算・出力時間の余裕を残す。
  options.maximize = true; // TODO: 距離の最小化ならfalse。Scoreの符号反転は不要。
  options.acceptance = LnsAcceptance::HillClimbing; // TODO: 焼きなましならSimulatedAnnealing。
  // HillClimbing: 悪化しない解を採用。同点も採用する点はbasic.cppと異なる。
  // RecordToRecord: 最良値から一定幅まで許す。必要ならこの方式も選べる。
  // SimulatedAnnealing: 現在値からの悪化を温度に応じた確率で許す。
  options.start_margin = options.end_margin = 2; // TODO(任意): RRTを選んだ時だけ使う許容幅。
  options.start_temperature = 10; // TODO: SAを選んだ場合の開始温度。
  options.end_temperature = 0.1;  // TODO: SAを選んだ場合の終了温度。
  options.early_cutoff = true;   // TODO: 閾値打ち切りを使うか。前計算とは独立。
  options.seed = 123;
  options.clock_interval = 1; // 重い修復では1。軽ければ間引ける。
  LargeNeighborhoodSearch<Problem> search(problem, std::move(initial), initial_score, options);
  search.run();
  problem.print_answer(search.best_state()); // current_stateではなく最良解を出す。
}
