#include <bits/stdc++.h>
using namespace std;

// 提出時は次の1行を、このhppの全文へ置き換える。
#include "library/cost-tree-beam-search.hpp"

// ============================================================================
// ここから問題ごとに編集する。
// ============================================================================

struct State {
  // TODO: DFS中に1個だけ持つ全状態と差分更新用cacheを書く。
};

struct Move {
  // TODO: 1手、revert用情報、何世代進むかを書く。
  int advance = 1;
};

vector<Move> generate_moves(const State&) {
  // TODO: 現在Stateから試す合法Moveを全て返す。
  return {};
}

void apply_move(State&, Move&) {
  // TODO: Moveを1手進め、全cacheを差分更新する。
}

void revert_move(State&, const Move&) {
  // TODO: apply_move直前と完全に同じStateへ戻す。
}

long long evaluate(const State&) {
  // TODO: 現在Stateの順位値そのものを返す。
  return 0;
}

int get_advance(const Move& move) {
  // TODO: このMoveで進む正の世代数を返す。
  return move.advance;
}

State make_initial_state() {
  // TODO: 初期Stateを返す。
  return {};
}

void print_answer(const vector<Move>&) {
  // TODO: 復元されたMove列を問題指定の形式で出力する。
}

// ============================================================================
// ここまでが主な編集場所。下は探索の呼び出し。
// ============================================================================

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  constexpr int BEAM_WIDTH = 100;     // TODO: ビーム幅。
  constexpr int MAX_GENERATION = 100; // TODO: 最大到着世代。
  State initial = make_initial_state();
  CostTreeBeamSearch<State, Move, long long> beam(
      initial, evaluate(initial), BEAM_WIDTH, MAX_GENERATION);
  while (beam.step(
      generate_moves, apply_move, revert_move, evaluate, get_advance)) {
  }
  print_answer(beam.restore());
}
