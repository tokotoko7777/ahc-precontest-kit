#include <bits/stdc++.h>

#include "../../library/tree-beam-search.hpp"

using namespace std;

// AHC021 "Pyramid Sorting" を TreeBeamSearch だけで組み立てる実戦例。
//
// 盤面は 465 個の int を持つので、候補ごとに State をコピーするより、
// swap を apply / revert する木上ビームサーチが自然です。
// このファイルはリポジトリ内では上の header を include しています。
// 提出時は tree-beam-search.hpp の中身を、このファイルの先頭へ貼ります。

constexpr int N = 30;
constexpr int CELL_COUNT = N * (N + 1) / 2;

// errors が最優先、次に違反している値の差、最後に値の高さを見る。
constexpr long long ERROR_UNIT = 1'000'000'000'000LL;
constexpr long long WEIGHT_UNIT = 1'000'000LL;

struct Move {
  int upper;
  int lower;
};

struct State {
  array<int, CELL_COUNT> value{};
  int errors = 0;
  long long error_weight = 0;
  long long height_score = 0;
  uint64_t hash = 0;
};

// (場所, 値) から毎回同じ乱数風の値を作る。
// 大きな Zobrist table を持たずに、swap 後の hash を O(1) 更新できる。
uint64_t hash_token(int position, int value) {
  uint64_t x =
      static_cast<uint64_t>(position) * CELL_COUNT + value + 0x9e3779b97f4a7c15ULL;
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  return x ^ (x >> 31);
}

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  auto id = [](int row, int column) {
    return row * (row + 1) / 2 + column;
  };

  array<int, CELL_COUNT> row{};
  array<int, CELL_COUNT> column{};
  vector<pair<int, int>> edges;
  array<vector<int>, CELL_COUNT> incident_edges;

  for (int r = 0; r < N; ++r) {
    for (int c = 0; c <= r; ++c) {
      row[id(r, c)] = r;
      column[id(r, c)] = c;
    }
  }

  // 上の頂点と、その左下・右下を辺で結ぶ。
  for (int r = 0; r + 1 < N; ++r) {
    for (int c = 0; c <= r; ++c) {
      const int upper = id(r, c);
      for (int next_c : {c, c + 1}) {
        const int lower = id(r + 1, next_c);
        const int edge_id = static_cast<int>(edges.size());
        edges.push_back({upper, lower});
        incident_edges[upper].push_back(edge_id);
        incident_edges[lower].push_back(edge_id);
      }
    }
  }

  State initial;
  for (int position = 0; position < CELL_COUNT; ++position) {
    cin >> initial.value[position];
    initial.height_score +=
        1LL * initial.value[position] * row[position];
    initial.hash ^= hash_token(position, initial.value[position]);
  }

  auto edge_error = [&](const State& state, int edge_id) {
    const auto [upper, lower] = edges[edge_id];
    return state.value[upper] > state.value[lower] ? 1 : 0;
  };

  auto edge_weight = [&](const State& state, int edge_id) {
    const auto [upper, lower] = edges[edge_id];
    return max(0, state.value[upper] - state.value[lower]);
  };

  for (int edge_id = 0; edge_id < static_cast<int>(edges.size());
       ++edge_id) {
    initial.errors += edge_error(initial, edge_id);
    initial.error_weight += edge_weight(initial, edge_id);
  }

  // swap の影響を受ける辺は、両端に接する高々12本だけ。
  auto affected_edges = [&](int first, int second) {
    array<int, 12> result{};
    int count = 0;
    for (int vertex : {first, second}) {
      for (int edge_id : incident_edges[vertex]) {
        bool already_added = false;
        for (int i = 0; i < count; ++i) {
          already_added |= result[i] == edge_id;
        }
        if (!already_added) result[count++] = edge_id;
      }
    }
    return pair{result, count};
  };

  // 同じ関数をもう一度呼ぶと元へ戻るので、apply と revert の両方に使える。
  auto swap_move = [&](State& state, const Move& move) {
    const auto [affected, count] =
        affected_edges(move.upper, move.lower);

    for (int i = 0; i < count; ++i) {
      state.errors -= edge_error(state, affected[i]);
      state.error_weight -= edge_weight(state, affected[i]);
    }

    const int upper_value = state.value[move.upper];
    const int lower_value = state.value[move.lower];
    state.hash ^= hash_token(move.upper, upper_value);
    state.hash ^= hash_token(move.lower, lower_value);
    state.hash ^= hash_token(move.upper, lower_value);
    state.hash ^= hash_token(move.lower, upper_value);
    swap(state.value[move.upper], state.value[move.lower]);

    state.height_score +=
        1LL * (upper_value - lower_value) *
        (row[move.lower] - row[move.upper]);

    for (int i = 0; i < count; ++i) {
      state.errors += edge_error(state, affected[i]);
      state.error_weight += edge_weight(state, affected[i]);
    }
  };

  auto evaluate = [](const State& state) {
    return -ERROR_UNIT * state.errors -
           WEIGHT_UNIT * state.error_weight + state.height_score;
  };

  // 実際に swap せず、差分評価だけで有望な手を選ぶ。
  auto error_delta = [&](const State& state, int upper, int lower) {
    const auto [affected, count] = affected_edges(upper, lower);
    int old_errors = 0;
    int new_errors = 0;
    long long old_weight = 0;
    long long new_weight = 0;

    auto value_after_swap = [&](int vertex) {
      if (vertex == upper) return state.value[lower];
      if (vertex == lower) return state.value[upper];
      return state.value[vertex];
    };

    for (int i = 0; i < count; ++i) {
      const auto [edge_upper, edge_lower] = edges[affected[i]];
      old_errors += state.value[edge_upper] > state.value[edge_lower];
      old_weight +=
          max(0, state.value[edge_upper] - state.value[edge_lower]);

      const int next_upper = value_after_swap(edge_upper);
      const int next_lower = value_after_swap(edge_lower);
      new_errors += next_upper > next_lower;
      new_weight += max(0, next_upper - next_lower);
    }

    return pair{new_errors - old_errors, new_weight - old_weight};
  };

  constexpr int BRANCH_WIDTH = 6;
  auto expand = [&](const State& state) {
    struct RatedMove {
      int errors;
      long long error_weight;
      int height_gain;
      Move move;
    };

    vector<RatedMove> rated;
    rated.reserve(edges.size());
    for (const auto& [upper, lower] : edges) {
      if (state.value[upper] < state.value[lower]) continue;
      const auto [errors_delta, weight_delta] =
          error_delta(state, upper, lower);
      rated.push_back({state.errors + errors_delta,
                       state.error_weight + weight_delta,
                       state.value[upper] - state.value[lower],
                       {upper, lower}});
    }

    auto better = [](const RatedMove& a, const RatedMove& b) {
      if (a.errors != b.errors) return a.errors < b.errors;
      if (a.error_weight != b.error_weight) {
        return a.error_weight < b.error_weight;
      }
      return a.height_gain > b.height_gain;
    };

    if (static_cast<int>(rated.size()) > BRANCH_WIDTH) {
      nth_element(
          rated.begin(), rated.begin() + BRANCH_WIDTH, rated.end(), better);
      rated.resize(BRANCH_WIDTH);
    }
    sort(rated.begin(), rated.end(), better);

    vector<Move> moves;
    moves.reserve(rated.size());
    for (const RatedMove& candidate : rated) {
      moves.push_back(candidate.move);
    }
    return moves;
  };

  constexpr int BEAM_WIDTH = 4;
  constexpr int MAX_OPERATIONS = 10000;
  constexpr double TIME_LIMIT_MS = 1800.0;

  TreeBeamSearch<State, Move, long long> beam(
      initial, evaluate(initial), BEAM_WIDTH);
  beam.reserve_nodes(1 + BEAM_WIDTH * MAX_OPERATIONS);
  beam.reserve_candidates(BEAM_WIDTH * BRANCH_WIDTH);

  long long best_score = evaluate(initial);
  vector<Move> answer;
  const auto start = chrono::steady_clock::now();

  for (int turn = 0; turn < MAX_OPERATIONS; ++turn) {
    const double elapsed_ms = chrono::duration<double, milli>(
                                  chrono::steady_clock::now() - start)
                                  .count();
    if (best_score >= 0 || elapsed_ms >= TIME_LIMIT_MS) break;

    const bool advanced = beam.step_with_key(
        expand,
        swap_move,
        swap_move,
        evaluate,
        [](const State& state) { return state.hash; });
    if (!advanced) break;

    if (beam.best_score() > best_score) {
      best_score = beam.best_score();
      answer = beam.restore();
    }
  }

  // restore が返した手順を再生して、履歴木との食い違いを検出する。
  State check = initial;
  for (const Move& move : answer) swap_move(check, move);
  assert(evaluate(check) == best_score);
  assert(static_cast<int>(answer.size()) <= MAX_OPERATIONS);

  cout << answer.size() << '\n';
  for (const Move& move : answer) {
    cout << row[move.upper] << ' ' << column[move.upper] << ' '
         << row[move.lower] << ' ' << column[move.lower] << '\n';
  }
}
