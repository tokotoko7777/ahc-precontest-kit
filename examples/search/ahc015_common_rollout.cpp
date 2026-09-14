#include <bits/stdc++.h>
using namespace std;

// 提出時は次の1行を、このhppの全文へ置き換える。
#include "../../library/common-scenario-average.hpp"

// Pre-contest public solver source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/examples/search/ahc015_common_rollout.cpp
// Official problem: https://atcoder.jp/contests/ahc015/tasks/ahc015_a

// ============================================================================
// ここから問題ごとに書く部分。盤面操作、評価、Problemを含む。
// ============================================================================
constexpr int BOARD_SIZE = 10;
constexpr int CELL_COUNT = BOARD_SIZE * BOARD_SIZE;
using Board = array<uint8_t, CELL_COUNT>;

int cell_id(int row, int column) {
  return row * BOARD_SIZE + column;
}

void place_by_rank(Board& board, int rank, int flavor) {
  for (uint8_t& cell : board) {
    if (cell != 0) continue;
    if (--rank == 0) {
      cell = static_cast<uint8_t>(flavor);
      return;
    }
  }
  throw runtime_error("placement rank exceeds empty cells");
}

// direction: 0=F, 1=B, 2=L, 3=R。
Board tilt_board(const Board& board, int direction) {
  Board result{};
  if (direction <= 1) {
    for (int column = 0; column < BOARD_SIZE; ++column) {
      int write = direction == 0 ? 0 : BOARD_SIZE - 1;
      const int step = direction == 0 ? 1 : -1;
      for (int k = 0; k < BOARD_SIZE; ++k) {
        const int row = direction == 0 ? k : BOARD_SIZE - 1 - k;
        const uint8_t candy = board[cell_id(row, column)];
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
        const uint8_t candy = board[cell_id(row, column)];
        if (candy == 0) continue;
        result[cell_id(row, write)] = candy;
        write += step;
      }
    }
  }
  return result;
}

int component_square_sum(const Board& board) {
  array<uint8_t, CELL_COUNT> visited{};
  array<int, CELL_COUNT> stack{};
  constexpr array<int, 4> DR{{-1, 1, 0, 0}};
  constexpr array<int, 4> DC{{0, 0, -1, 1}};
  int answer = 0;
  for (int start = 0; start < CELL_COUNT; ++start) {
    if (board[start] == 0 || visited[start]) continue;
    const uint8_t flavor = board[start];
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

int partial_board_value(const Board& board) {
  int edge_preference = 0;
  int same_neighbors = 0;
  for (int row = 0; row < BOARD_SIZE; ++row) {
    for (int column = 0; column < BOARD_SIZE; ++column) {
      const int flavor = board[cell_id(row, column)];
      if (flavor == 1) edge_preference += BOARD_SIZE - 1 - row;
      if (flavor == 2) edge_preference += row;
      if (flavor == 3) edge_preference += BOARD_SIZE - 1 - column;
      if (column + 1 < BOARD_SIZE && flavor != 0 &&
          board[cell_id(row, column + 1)] == flavor) {
        ++same_neighbors;
      }
      if (row + 1 < BOARD_SIZE && flavor != 0 &&
          board[cell_id(row + 1, column)] == flavor) {
        ++same_neighbors;
      }
    }
  }
  return 100 * component_square_sum(board) +
         3 * edge_preference + 8 * same_neighbors;
}

int quick_policy_direction(const Board& board, int flavor) {
  constexpr array<int, 4> HOME_DIRECTION{{0, 0, 1, 2}};
  int best_direction = 0;
  int best_value = numeric_limits<int>::min();
  for (int direction = 0; direction < 4; ++direction) {
    const Board candidate = tilt_board(board, direction);
    int value = partial_board_value(candidate);
    if (direction == HOME_DIRECTION[flavor]) value += 40;
    if (value > best_value) {
      best_value = value;
      best_direction = direction;
    }
  }
  return best_direction;
}

struct CandyRolloutProblem {
  // TODO(AHC015): 現在まで確定した実状態を書く。
  struct State {
    Board board{};
    int turn = 0;  // このturnの飴は配置済み、まだ傾けていない。
  };

  // TODO(AHC015): 今比較する1手を書く。0=F, 1=B, 2=L, 3=R。
  using Action = int;

  // TODO(AHC015): 自分では決められない未知の未来1本を書く。
  struct Scenario {
    array<uint8_t, 30> placement_rank{};
    int length = 0;
  };

  using Score = int;

  array<int, CELL_COUNT> flavors{};
  array<Action, 4> actions{{0, 1, 2, 3}};

  void read_flavors() {
    for (int& flavor : flavors) cin >> flavor;
  }

  // TODO(AHC015): 今比較する合法Actionを全て返す。
  const array<Action, 4>& generate_actions(const State&) const {
    return actions;
  }

  // TODO(AHC015): 全Actionで共有する未知の配置順位だけをsampleする。
  Scenario generate_scenario(const State& state, mt19937_64& engine) const {
    Scenario scenario;
    const int remaining = CELL_COUNT - 1 - state.turn;
    scenario.length = min(30, remaining);
    for (int step = 0; step < scenario.length; ++step) {
      const int empty_count = CELL_COUNT - 1 - state.turn - step;
      scenario.placement_rank[step] = static_cast<uint8_t>(
          uniform_int_distribution<int>(1, empty_count)(engine));
    }
    return scenario;
  }

  // TODO(AHC015): 最初のActionだけ変え、同じScenarioを軽い方策で最後まで進める。
  // Runnerが同一Scenario集合を4方向へ使うため、候補差の乱数ばらつきが減る。
  Score evaluate_action(
      const State& state,
      const Action& first_action,
      const Scenario& scenario) const {
    Board board = tilt_board(state.board, first_action);
    for (int step = 0; step < scenario.length; ++step) {
      const int future_turn = state.turn + 1 + step;
      place_by_rank(
          board, scenario.placement_rank[step], flavors[future_turn]);
      const int direction =
          quick_policy_direction(board, flavors[future_turn]);
      board = tilt_board(board, direction);
    }
    return partial_board_value(board);
  }
};

// ============================================================================
// ここから下は逐次入出力とRunnerの呼び出し。
// Scenario生成・全Actionでの共有・平均・最良選択はRunnerが担当する。
// ============================================================================
int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  CandyRolloutProblem problem;
  problem.read_flavors();

  uint64_t input_hash = 1469598103934665603ULL;
  for (int flavor : problem.flavors) {
    input_hash ^= static_cast<uint64_t>(flavor);
    input_hash *= 1099511628211ULL;
  }
  CommonScenarioRolloutRunner<CandyRolloutProblem> rollout(
      problem, input_hash);
  rollout.reserve(4, 24);

  Board board{};
  constexpr array<char, 4> DIRECTION_CHAR{{'F', 'B', 'L', 'R'}};
  for (int turn = 0; turn < CELL_COUNT; ++turn) {
    int placement_rank;
    cin >> placement_rank;
    place_by_rank(board, placement_rank, problem.flavors[turn]);

    const int remaining = CELL_COUNT - 1 - turn;
    const int direction = remaining == 0
                              ? quick_policy_direction(
                                    board, problem.flavors[turn])
                              : rollout.choose_action(
                                    {board, turn}, remaining > 50 ? 16 : 24);
    board = tilt_board(board, direction);
    cout << DIRECTION_CHAR[direction] << endl;
  }
}
