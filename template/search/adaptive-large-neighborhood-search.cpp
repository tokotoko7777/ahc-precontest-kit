// GCC提出用の最適化。診断時は -DAHC_DISABLE_GCC_OPTIMIZE で無効化。
#if defined(__GNUC__) && !defined(__clang__) && !defined(AHC_DISABLE_GCC_OPTIMIZE)
#pragma GCC optimize("O3")
#endif

#include <bits/stdc++.h>
using namespace std;

// 提出時はこの2行を各hpp全文へ置き換える。提出するのはmain.cppだけ。
#include "library/large-neighborhood-search.hpp"
#include "library/adaptive-operator-selector.hpp"

// ==================== ここから問題ごとに編集する ====================
struct Problem {
  using Score = long long; // TODO: int / long long / double等から選ぶ。
  struct State {
    // TODO: 解（訪問順、割当、盤面など）と必要な差分計算用cacheを書く。
    // コピーで内容も独立する値型にする。currentとcandidateは共有させない。
  };
  static constexpr int operator_count = 3; // TODO: 壊し方の個数。
  int operator_id = 0; // ライブラリが選ぶ0..operator_count-1。自分で更新不要。
  // TODO: 入力・前計算・壊した箇所のリスト等を書く。

  void read_input() {
    // TODO: 入力を読む。使える場合だけ距離表などを前計算する。
  }
  State make_initial_state() const {
    // TODO: 必ず合法な初期解を返す。単純貪欲でよい。
    return {};
  }
  Score evaluate(const State&) const {
    // TODO: 完成した解の「絶対スコア」を返す。前の解との差ではない。
    return 0;
  }
  void destroy(const State& current, State& candidate, mt19937_64& rng, double progress) {
    candidate = current; // 最初はコピーでよい。重ければ必要な部分だけ詰め直す。
    // candidateは前回の残骸を含むので必ず上書きする。currentは変更しない。
    switch (operator_id) {
      case 0:
        // TODO: 小さく壊す。例: 訪問順から連続する3点を除く。
        break;
      case 1:
        // TODO: 大きく壊す。例: 訪問順から連続する15点を除く。
        break;
      case 2:
        // TODO: 別の観点で壊す。例: 座標が近い点 / コストが高い辺の点を除く。
        break;
    }
    // TODO: 除いた要素の一覧をProblem内に保存してrepairに渡す。
    // rng()は乱数。progressは0〜1の時間進捗。不要なら使わなくてよい。
    (void)rng; (void)progress;
  }
  optional<Score> repair(State& candidate, mt19937_64& rng,
                         double progress, long double threshold) {
    // TODO: 除いた部分を貪欲/DP/DFS/フロー等で埋め、合法な完成解へ修復する。
    // 成功なら完成候補の絶対スコアを返す。失敗ならnullopt。
    // 壊し方と修復の組合せを変えたければここでもoperator_idで分岐してよい。
    //
    // TODO(任意): 最大化なら到達可能な上界 < threshold、最小化なら到達可能な
    // 下界 > thresholdの時だけnulloptで打ち切る。等号は採用するので切らない。
    // 証明できなければ最後まで修復してよい。OFFなら最大化-inf / 最小化+inf。
    (void)candidate; (void)rng; (void)progress; (void)threshold;
    return nullopt; // TODO: 実装したらreturn evaluate(candidate)等に変更。
  }
  double reward(LnsOutcome outcome) const {
    // TODO(任意): 成果を[0,1]で返す。大小関係だけでなく値の比率も選択頻度に効く。
    // 下は出発点。特定の問題で強いと保証した設定ではない。
    // 差分量を報酬に使う場合は尺度を決めて[0,1]へ正規化する。
    switch (outcome) {
      case LnsOutcome::ImprovedBest: return 1.0; // 過去最良を厳密に更新。
      case LnsOutcome::ImprovedCurrent: return 0.5; // 現在値は改善、過去最良には届かず。
      case LnsOutcome::Accepted: return 0.1; // 同点または悪化だが採用。
      case LnsOutcome::Rejected: return 0.0; // 棄却・修復失敗・閾値打ち切り。
    }
    return 0;
  }
  void print_answer(const State&) const {
    // TODO: 問題が指定した出力形式で最良解を書く。ログはcerrへ。
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
  LnsOptions search_options;
  search_options.time_limit_ms = 1800; // TODO: 入力・前計算・出力分は別に余裕を残す。
  search_options.maximize = true; // TODO: コスト最小化ならfalse。符号反転は不要。
  search_options.acceptance = LnsAcceptance::SimulatedAnnealing;
  search_options.start_temperature = 10; // TODO: Scoreと同じ単位。
  search_options.end_temperature = 0.1;
  search_options.early_cutoff = true; // TODO: 前計算とは独立にON/OFF可能。
  search_options.seed = 123;
  AdaptiveOperatorOptions selection_options;
  selection_options.adaptive = true; // TODO: falseの等確率版と実問題スコアで比べる。
  selection_options.update_interval = 128; // 128試行ごと。1回が重ければ小さくする。
  selection_options.learning_rate = 0.2; // 新しい成果を20%混ぜる。
  selection_options.exploration = 0.1; // 10%は等確率。苦手な近傍も完全には捨てない。
  AdaptiveOperatorSelector selector(Problem::operator_count, selection_options);
  mt19937_64 selection_rng(456); // 近傍生成/採用判定とは独立した乱数列。
  LargeNeighborhoodSearch<Problem> search(problem, std::move(initial), initial_score, search_options);
  while (true) {
    problem.operator_id = selector.select(selection_rng);
    if (!search.step()) break;
    // 失敗・枝刈りも0で記録する。成功時だけ記録するのは誤り。
    selector.record(problem.operator_id, problem.reward(search.last_outcome()));
  }
  problem.print_answer(search.best_state());
}
