#include <bits/stdc++.h>
using namespace std;

// 提出時は次の2行を、それぞれのhpp全文へ置き換える。
#include "../../library/batched-timer.hpp"
#include "../../library/tree-beam-search.hpp"

// Pre-contest public solver source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/examples/search/ahc021_tree_beam.cpp
// Official problem: https://atcoder.jp/contests/ahc021/tasks/ahc021_a

// ============================================================================
// ここから問題ごとに書く部分。定数、State、Move、Problem、出力を含む。
// ============================================================================
constexpr int N = 30;
constexpr int CELL_COUNT = N * (N + 1) / 2;
constexpr long long ERROR_UNIT = 1'000'000'000'000LL;
constexpr long long WEIGHT_UNIT = 1'000'000LL;

#ifndef AHC021_TIME_LIMIT_MS
#define AHC021_TIME_LIMIT_MS 1800.0
#endif

struct PyramidMove {
  int upper;
  int lower;
};

struct PyramidState {
  array<int, CELL_COUNT> value{};
  int errors = 0;
  long long error_weight = 0;
  long long height_score = 0;
  uint64_t hash = 0;
};

uint64_t hash_token(int position, int value) {
  uint64_t x = static_cast<uint64_t>(position) * CELL_COUNT + value +
               0x9e3779b97f4a7c15ULL;
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  return x ^ (x >> 31);
}

struct PyramidProblem {
  // TODO(AHC021): DFS中に1個だけ持つ盤面と差分cacheをStateへ置く。
  using State = PyramidState;
  // TODO(AHC021): 1手とrevertに必要な情報をMoveへ置く。
  using Move = PyramidMove;
  using Score = long long;

  array<int, CELL_COUNT> row{};
  array<int, CELL_COUNT> column{};
  vector<pair<int, int>> edges;
  array<vector<int>, CELL_COUNT> incident_edges;

  PyramidProblem() {
    for (int r = 0; r < N; ++r) {
      for (int c = 0; c <= r; ++c) {
        row[id(r, c)] = r;
        column[id(r, c)] = c;
      }
    }
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
  }

  static int id(int row, int column) {
    return row * (row + 1) / 2 + column;
  }

  State read_initial_state() const {
    State state;
    for (int position = 0; position < CELL_COUNT; ++position) {
      cin >> state.value[position];
      state.height_score += 1LL * state.value[position] * row[position];
      state.hash ^= hash_token(position, state.value[position]);
    }
    for (int edge_id = 0; edge_id < static_cast<int>(edges.size());
         ++edge_id) {
      state.errors += edge_error(state, edge_id);
      state.error_weight += edge_weight(state, edge_id);
    }
    return state;
  }

  int edge_error(const State& state, int edge_id) const {
    const auto [upper, lower] = edges[edge_id];
    return state.value[upper] > state.value[lower] ? 1 : 0;
  }

  int edge_weight(const State& state, int edge_id) const {
    const auto [upper, lower] = edges[edge_id];
    return max(0, state.value[upper] - state.value[lower]);
  }

  pair<array<int, 12>, int> affected_edges(int first, int second) const {
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
    return {result, count};
  }

  pair<int, long long> error_delta(
      const State& state, int upper, int lower) const {
    const auto [affected, count] = affected_edges(upper, lower);
    int old_errors = 0;
    int new_errors = 0;
    long long old_weight = 0;
    long long new_weight = 0;

    const auto value_after_swap = [&](int vertex) {
      if (vertex == upper) return state.value[lower];
      if (vertex == lower) return state.value[upper];
      return state.value[vertex];
    };
    for (int i = 0; i < count; ++i) {
      const auto [edge_upper, edge_lower] = edges[affected[i]];
      old_errors += state.value[edge_upper] > state.value[edge_lower];
      old_weight += max(0, state.value[edge_upper] - state.value[edge_lower]);
      const int next_upper = value_after_swap(edge_upper);
      const int next_lower = value_after_swap(edge_lower);
      new_errors += next_upper > next_lower;
      new_weight += max(0, next_upper - next_lower);
    }
    return {new_errors - old_errors, new_weight - old_weight};
  }

  // TODO(AHC021): 現在Stateから試す合法Moveを列挙する。
  // 全870辺を安い差分で順位付けし、各親から上位6手だけ返す。
  vector<Move> generate_moves(const State& state) const {
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

    const auto better = [](const RatedMove& left, const RatedMove& right) {
      if (left.errors != right.errors) return left.errors < right.errors;
      if (left.error_weight != right.error_weight) {
        return left.error_weight < right.error_weight;
      }
      return left.height_gain > right.height_gain;
    };
    constexpr int BRANCH_WIDTH = 6;  // TODO(AHC021): 親ごとの候補数。
    if (static_cast<int>(rated.size()) > BRANCH_WIDTH) {
      nth_element(
          rated.begin(), rated.begin() + BRANCH_WIDTH, rated.end(), better);
      rated.resize(BRANCH_WIDTH);
    }
    sort(rated.begin(), rated.end(), better);

    vector<Move> moves;
    moves.reserve(rated.size());
    for (const RatedMove& candidate : rated) moves.push_back(candidate.move);
    return moves;
  }

  // TODO(AHC021): 盤面・評価cache・hashを1手だけ差分更新する。
  void apply_move(State& state, Move& move) const {
    const auto [affected, count] = affected_edges(move.upper, move.lower);
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
    state.height_score += 1LL * (upper_value - lower_value) *
                          (row[move.lower] - row[move.upper]);

    for (int i = 0; i < count; ++i) {
      state.errors += edge_error(state, affected[i]);
      state.error_weight += edge_weight(state, affected[i]);
    }
  }

  // TODO(AHC021): apply直前と完全に同じStateへ戻す。
  // swapは同じ操作を2回行うと元へ戻る。
  void revert_move(State& state, const Move& move) const {
    Move undo = move;
    apply_move(state, undo);
  }

  // TODO(AHC021): 子Stateの順位値そのものを返す。
  Score evaluate(const State& state) const {
    return -ERROR_UNIT * state.errors -
           WEIGHT_UNIT * state.error_weight + state.height_score;
  }

  // TODO(AHC021): 同じ盤面を同じkeyにして重複を除く。
  uint64_t make_key(const State& state) const {
    return state.hash;
  }
};

void print_answer(
    const PyramidProblem& problem,
    const vector<PyramidProblem::Move>& answer) {
  cout << answer.size() << '\n';
  for (const PyramidProblem::Move& move : answer) {
    cout << problem.row[move.upper] << ' ' << problem.column[move.upper]
         << ' ' << problem.row[move.lower] << ' '
         << problem.column[move.lower] << '\n';
  }
}

// ============================================================================
// ここから下は探索の呼び出し。履歴木・DFS・上位N件・重複除去はRunner側。
// ============================================================================
int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  PyramidProblem problem;
  PyramidProblem::State initial = problem.read_initial_state();
  constexpr int BEAM_WIDTH = 4;       // TODO(AHC021): ビーム幅。
  constexpr int MAX_OPERATIONS = 10000;
  TreeBeamRunner<PyramidProblem> beam(
      problem, initial, problem.evaluate(initial), BEAM_WIDTH);
  beam.reserve_nodes(1 + BEAM_WIDTH * MAX_OPERATIONS);
  beam.reserve_candidates(BEAM_WIDTH * 6);

  long long best_score = problem.evaluate(initial);
  vector<PyramidProblem::Move> answer;
  BatchedTimer timer(AHC021_TIME_LIMIT_MS, 16);
  for (int turn = 0; turn < MAX_OPERATIONS; ++turn) {
    if (best_score >= 0 || timer.is_over()) break;
    if (!beam.step_with_key()) break;
    if (beam.best_score() > best_score) {
      best_score = beam.best_score();
      beam.restore(0, answer);
    }
  }

  PyramidProblem::State check = initial;
  for (PyramidProblem::Move move : answer) {
    problem.apply_move(check, move);
  }
  assert(problem.evaluate(check) == best_score);
  assert(static_cast<int>(answer.size()) <= MAX_OPERATIONS);
  print_answer(problem, answer);
}
