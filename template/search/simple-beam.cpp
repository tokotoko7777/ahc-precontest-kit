#include <bits/stdc++.h>
using namespace std;

// 提出時は次の1行を、このhppの全文へ置き換える。
#include "library/simple-beam-search.hpp"

// ============================================================================
// ここから問題ごとに編集する。
// ============================================================================

struct State {
  // TODO: 探索途中の解を書く。
  // Stateは候補ごとにコピーされるため、大きくしすぎない。
};

vector<State> expand(const State&) {
  // TODO: 現在Stateから1手進めた合法な子Stateを全て返す。
  return {};
}

long long evaluate(const State&) {
  // TODO: 現在Stateの順位値そのものを返す。既定では大きいほど良い。
  return 0;
}

State make_initial_state() {
  // TODO: 初期Stateを返す。
  return {};
}

void print_answer(const State&) {
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
  SimpleBeamSearch<State, long long> beam(
      make_initial_state(), BEAM_WIDTH);
  for (int turn = 0; turn < MAX_TURN; ++turn) {
    if (!beam.step(expand, evaluate)) break;
  }
  print_answer(beam.best());
}
