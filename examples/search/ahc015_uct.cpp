// GCC提出用の最適化。診断時は -DAHC_DISABLE_GCC_OPTIMIZE で無効化。
#if defined(__GNUC__) && !defined(__clang__) && !defined(AHC_DISABLE_GCC_OPTIMIZE)
#pragma GCC optimize("O3")
#endif

#include <bits/stdc++.h>
using namespace std;
#include "../../library/monte-carlo-tree-search.hpp"
// Pre-contest public solver source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/examples/search/ahc015_uct.cpp
// 問題: https://atcoder.jp/contests/ahc015/tasks/ahc015_a
// 盤面・軽い終局方策はkit内のahc015_monte_carlo_score_benchmark.cppから流用。
#ifndef AHC015_USE_UCT
#define AHC015_USE_UCT 1
#endif
#ifndef AHC015_UCT_DEPTH
#define AHC015_UCT_DEPTH 8
#endif
#ifndef AHC015_UCT_EXPLORATION
#define AHC015_UCT_EXPLORATION 0.3
#endif
#ifndef AHC015_ORDER_ACTIONS
#define AHC015_ORDER_ACTIONS 0
#endif
constexpr int BOARD_SIZE = 10;
constexpr int CELL_COUNT = BOARD_SIZE * BOARD_SIZE;
using Board = std::array<std::uint8_t, CELL_COUNT>;

int cell_id(int row, int column) { return row * BOARD_SIZE + column; }

void place_by_rank(Board& board, int rank, int flavor) {
  for (std::uint8_t& cell : board) {
    if (cell != 0) continue;
    if (--rank == 0) {
      cell = static_cast<std::uint8_t>(flavor);
      return;
    }
  }
  throw std::runtime_error("placement rank exceeds empty cells");
}

Board tilt_board(const Board& board, int direction) {
  Board result{};
  if (direction <= 1) {
    for (int column = 0; column < BOARD_SIZE; ++column) {
      int write = direction == 0 ? 0 : BOARD_SIZE - 1;
      const int step = direction == 0 ? 1 : -1;
      for (int k = 0; k < BOARD_SIZE; ++k) {
        const int row = direction == 0 ? k : BOARD_SIZE - 1 - k;
        const std::uint8_t candy = board[cell_id(row, column)];
        if (candy == 0) continue;
        result[cell_id(write, column)] = candy;
        write += step;
      }
    }
  } else {
    for (int row = 0; row < BOARD_SIZE; ++row) {
      int write = direction == 2 ? 0 : BOARD_SIZE - 1;
      const int step = direction == 2 ? 1 : -1;
      for (int k = 0; k < BOARD_SIZE; ++k) {
        const int column = direction == 2 ? k : BOARD_SIZE - 1 - k;
        const std::uint8_t candy = board[cell_id(row, column)];
        if (candy == 0) continue;
        result[cell_id(row, write)] = candy;
        write += step;
      }
    }
  }
  return result;
}

int component_square_sum(const Board& board) {
  std::array<std::uint8_t, CELL_COUNT> visited{};
  std::array<int, CELL_COUNT> stack{};
  constexpr std::array<int, 4> DR{{-1, 1, 0, 0}};
  constexpr std::array<int, 4> DC{{0, 0, -1, 1}};
  int answer = 0;
  for (int start = 0; start < CELL_COUNT; ++start) {
    if (board[start] == 0 || visited[start]) continue;
    const std::uint8_t flavor = board[start];
    int size = 0;
    int stack_size = 1;
    stack[0] = start;
    visited[start] = 1;
    while (stack_size > 0) {
      const int cell = stack[--stack_size];
      ++size;
      const int row = cell / BOARD_SIZE;
      const int column = cell % BOARD_SIZE;
      for (int direction = 0; direction < 4; ++direction) {
        const int next_row = row + DR[direction];
        const int next_column = column + DC[direction];
        if (next_row < 0 || next_row >= BOARD_SIZE || next_column < 0 ||
            next_column >= BOARD_SIZE) {
          continue;
        }
        const int next = cell_id(next_row, next_column);
        if (!visited[next] && board[next] == flavor) {
          visited[next] = 1;
          stack[stack_size++] = next;
        }
      }
    }
    answer += size * size;
  }
  return answer;
}


int rule_direction(int current_flavor, int next_flavor) {
  // 色1を上下の一方、色2・3を左右へ分ける。
  // 「現在の色×次に来る色」なので、別問題ではここが主な編集点。
  constexpr int RULE[3][3] = {
      {0, 1, 1},
      {0, 2, 3},
      {0, 2, 3},
  };
  return RULE[current_flavor - 1][next_flavor - 1];
}


struct CandyTreeProblem {
  struct State { Board board{}; int turn = 0; }; // 現ターンの飴は配置済み。
  using Action = int;
  array<int, CELL_COUNT> flavors{};
  array<int, 4> actions{{0, 1, 2, 3}};
  double denominator = 1;
  bool is_terminal(const State& state) const { return state.turn == CELL_COUNT; }
  array<int, 4> generate_actions(const State& state) const {
    auto order = actions;
    // 任意: 未試行の手を試す順番だけ、rolloutで使う軽い方策を先頭にする。
    // 合法手は全て残す。UCBの値や報酬を改変する機能ではない。
    if constexpr (AHC015_ORDER_ACTIONS) {
      const int first = state.turn + 1 < CELL_COUNT ? rule_direction(flavors[state.turn], flavors[state.turn+1]) : 0;
      swap(order[0], order[first]);
    }
    return order;
  }
  uint64_t sample_transition(State& state, int action, mt19937_64& rng) const {
    state.board = tilt_board(state.board, action);
    if (++state.turn == CELL_COUNT) return 0;
    const int rank = uniform_int_distribution<int>(1, CELL_COUNT - state.turn)(rng);
    place_by_rank(state.board, rank, flavors[state.turn]);
    return rank; // 同じ親・方向では、空きマス順位が次状態を一意に決める。
  }
  double rollout(State state, mt19937_64& rng) const {
    while (!is_terminal(state)) {
      const int direction = state.turn + 1 < CELL_COUNT ?
          rule_direction(flavors[state.turn], flavors[state.turn + 1]) : 0;
      sample_transition(state, direction, rng);
    }
    return component_square_sum(state.board) / denominator;
  }
  // 比較用のflat Monte Carlo。同じ未来を4手へ渡し、方策・評価・時間枠を揃える。
  int flat_action(const State& current, double milliseconds, mt19937_64& rng) const {
    const auto start = chrono::steady_clock::now();
    array<double, 4> sum{};
    int samples = 0;
    do {
      array<int, CELL_COUNT> ranks{};
      for (int t = current.turn + 1; t < CELL_COUNT; ++t)
        ranks[t] = uniform_int_distribution<int>(1, CELL_COUNT - t)(rng);
      for (int a = 0; a < 4; ++a) {
        Board board = tilt_board(current.board, a);
        for (int t = current.turn + 1; t < CELL_COUNT; ++t) {
          place_by_rank(board, ranks[t], flavors[t]);
          board = tilt_board(board, t + 1 < CELL_COUNT ? rule_direction(flavors[t], flavors[t + 1]) : 0);
        }
        sum[a] += component_square_sum(board);
      }
      ++samples;
    } while (chrono::duration<double, milli>(chrono::steady_clock::now() - start).count() < milliseconds);
    return static_cast<int>(max_element(sum.begin(), sum.end()) - sum.begin());
  }
};
#ifndef AHC015_UCT_NO_MAIN
int main() {
  ios::sync_with_stdio(false); cin.tie(nullptr);
  CandyTreeProblem problem;
  array<int, 4> count{};
  uint64_t seed = 1469598103934665603ULL;
  for (int& flavor : problem.flavors) { cin >> flavor; ++count[flavor]; seed = (seed ^ flavor) * 1099511628211ULL; }
  problem.denominator = 0;
  for (int f = 1; f <= 3; ++f) problem.denominator += count[f] * count[f];
  MonteCarloTreeSearch<CandyTreeProblem> search(problem, seed);
  mt19937_64 rng(seed);
  const auto start = chrono::steady_clock::now();
  CandyTreeProblem::State state;
  uint64_t iterations = 0;
  for (int turn = 0; turn < CELL_COUNT; ++turn) {
    int rank; if (!(cin >> rank)) return 1; // 未知の将来rankはここで先読みしない。
    state.turn = turn;
    place_by_rank(state.board, rank, problem.flavors[turn]);
    const double elapsed = chrono::duration<double, milli>(chrono::steady_clock::now() - start).count();
    const double allocation = max(0.01, (1750.0 - elapsed) / (CELL_COUNT - turn));
    int direction = 0;
    if constexpr (AHC015_USE_UCT) {
      MctsOptions options;
      options.time_limit_ms = allocation;
      options.max_depth = AHC015_UCT_DEPTH;
      options.exploration = AHC015_UCT_EXPLORATION;
      const auto action = search.choose_action(state, options);
      direction = action.value_or(0);
      iterations += search.iterations();
    } else direction = problem.flat_action(state, allocation, rng);
    state.board = tilt_board(state.board, direction);
    cout << "FBLR"[direction] << endl;
  }
  cerr << "iterations=" << iterations << '\n';
}
#endif
