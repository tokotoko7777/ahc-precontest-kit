#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "../../library/common-scenario-average.hpp"
#include "../../library/coalesced-rollout.hpp"

// AHC015 "Halloween Candy" の公式入力分布と公式得点を使う。
// https://atcoder.jp/contests/ahc015/tasks/ahc015_a
//
// 【問題に合わせて書き換える場所】
//   CandyRolloutProblem の State / Action / Scenario と3関数。
//   - generate_actions: 今比較する最初の手
//   - generate_scenario: 自分で決められない未来
//   - evaluate_action: その未来で最初の手を採点するplayout
//
// 【ライブラリが担当する場所】
//   同じ未来sampleを全Actionで使う、平均する、最良の1手を選ぶ。
//
// 未来の配置だけをMonte Carloでsampleし、自分の未来操作は
// 「今の色×次の色」の軽い問題固有方策で最後まで進める。

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

struct CandyCase {
  std::array<int, CELL_COUNT> flavor{};
  std::array<int, CELL_COUNT> placement_rank{};
};

CandyCase make_case(std::uint64_t seed) {
  std::mt19937_64 engine(seed);
  CandyCase result;
  for (int turn = 0; turn < CELL_COUNT; ++turn) {
    result.flavor[turn] =
        std::uniform_int_distribution<int>(1, 3)(engine);
    result.placement_rank[turn] =
        std::uniform_int_distribution<int>(1, CELL_COUNT - turn)(engine);
  }
  return result;
}

long long official_score(const Board& board, const CandyCase& input) {
  std::array<int, 4> count{};
  for (int flavor : input.flavor) ++count[flavor];
  long long denominator = 0;
  for (int flavor = 1; flavor <= 3; ++flavor) {
    denominator += 1LL * count[flavor] * count[flavor];
  }
  const long long numerator = component_square_sum(board);
  return std::llround(1'000'000.0L * numerator / denominator);
}

// direction: 0=F, 1=B, 2=L, 3=R。
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

struct CandyRolloutProblem {
  // TODO: 【問題ごと】現在までに確定している情報をStateへ書く。
  struct State {
    Board board{};
    int turn = 0;  // このturnの飴は配置済み、傾ける前。
  };
  // TODO: 【問題ごと】今選ぶ1手の型を書く。全候補ぶん持つので小さくする。
  using Action = int;
  // TODO: 【問題ごと】自分で決められない未知の未来1本を表す型を書く。
  struct Scenario {
    std::array<std::uint8_t, CELL_COUNT> rank{};
    int length = 0;
  };
  // TODO: 【問題ごと】1 rolloutの評価値の型を書く。
  using Score = int;

  // TODO: 【問題ごと】共通入力・事前計算と、今選べるAction表を置く。
  const CandyCase& input;
  std::array<Action, 4> actions{{0, 1, 2, 3}};

  // TODO: 【問題ごと】今比較する合法Actionを全て返す。
  const std::array<Action, 4>& generate_actions(const State&) const {
    return actions;
  }

  // TODO: 【問題ごと】自分では決められない未知情報だけをsampleする。
  // 未知なのは「次以降の飴が何番目の空きマスへ来るか」だけ。
  Scenario generate_scenario(const State& state,
                             std::mt19937_64& engine) const {
    Scenario scenario;
    scenario.length = CELL_COUNT - 1 - state.turn;
    for (int step = 0; step < scenario.length; ++step) {
      const int empty_count = CELL_COUNT - 1 - state.turn - step;
      scenario.rank[step] = static_cast<std::uint8_t>(
          std::uniform_int_distribution<int>(1, empty_count)(engine));
    }
    return scenario;
  }

  // TODO: 【問題ごと】最初のAction後を終端まで進め、最終評価を返す。
  // 最初の1手だけactionを使い、以後は軽い固定方策で終局まで進める。
  // 返値は公式得点の分子。全actionで分母が同じなので順位は一致する。
  Score evaluate_action(const State& state,
                        const Action& action,
                        const Scenario& scenario) const {
    Board board = tilt_board(state.board, action);
    for (int step = 0; step < scenario.length; ++step) {
      const int turn = state.turn + 1 + step;
      place_by_rank(board, scenario.rank[step], input.flavor[turn]);
      if (turn + 1 < CELL_COUNT) {
        board = tilt_board(
            board, rule_direction(input.flavor[turn], input.flavor[turn + 1]));
      }
    }
    return component_square_sum(board);
  }
};


#ifndef AHC015_SAMPLES
#define AHC015_SAMPLES 448 // TODO: 制限時間内の実問題scoreで調整する。
#endif
#ifndef AHC015_MERGE
#define AHC015_MERGE 1
#endif
#ifndef AHC015_CLASSIC
#define AHC015_CLASSIC 0
#endif
#ifndef AHC015_ENGINE_SEED
#define AHC015_ENGINE_SEED 15015
#endif
#ifndef AHC015_TURN_SEED
#define AHC015_TURN_SEED 1
#endif
// TODO(AHC015): 既知の味だけ最初に読む。配置順位は各ターンの直前に1個だけ読む。
int main() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);
  CandyCase input;
  for (int& flavor : input.flavor) {
    if (!(std::cin >> flavor) || flavor < 1 || flavor > 3) return 1;
  }
  CandyRolloutProblem problem{input};
  CommonScenarioRolloutRunner<CandyRolloutProblem> runner(problem, AHC015_ENGINE_SEED);
  runner.reserve(4, AHC015_SAMPLES);
  CoalescedRollout<Board, int> shared;
  shared.reserve(4);
  std::vector<Board> initial_boards;
  initial_boards.reserve(4);
  std::uint64_t transitions = 0;
  Board board{};
  constexpr char directions[] = "FBLR";
  for (int turn = 0; turn < CELL_COUNT; ++turn) {
    int rank;
    if (!(std::cin >> rank) || rank < 1 || rank > CELL_COUNT - turn) return 1;
    place_by_rank(board, rank, input.flavor[turn]);
    int action = 0;
    if (turn + 1 < CELL_COUNT) {
      const CandyRolloutProblem::State state{board, turn};
#if AHC015_TURN_SEED
      // サンプル数を変えても各手番の未来列の先頭を揃える。未来の実入力は使わない。
      runner.engine().seed(AHC015_ENGINE_SEED + 0x9e3779b97f4a7c15ULL * (turn + 1));
#endif
#if AHC015_CLASSIC
      action = runner.choose_action(state, AHC015_SAMPLES);
      transitions += std::uint64_t{4} * AHC015_SAMPLES * (CELL_COUNT - 1 - turn);
#else
      // TODO: 今の各Actionを1回だけ反映した仮状態。Scenarioに依存しない部分は先に作る。
      initial_boards.clear();
      for (int first_action : problem.generate_actions(state)) {
        initial_boards.push_back(tilt_board(board, first_action));
      }
      action = runner.choose_action_batched(state, AHC015_SAMPLES,
          [&](const auto&, const auto&, const CandyRolloutProblem::Scenario& scenario) -> const std::vector<int>& {
        // TODO: 共通の未来を1段進める。元の方策・配置・最終評価は通常版と同じ。
        const auto advance = [&](Board& future, int step) {
          const int future_turn = turn + 1 + step;
          place_by_rank(future, scenario.rank[step], input.flavor[future_turn]);
          if (future_turn + 1 < CELL_COUNT) {
            future = tilt_board(future, rule_direction(input.flavor[future_turn], input.flavor[future_turn + 1]));
          }
        };
        const auto& scores = shared.evaluate<AHC015_MERGE != 0>(
            initial_boards, scenario.length, advance, component_square_sum);
        transitions += shared.last_transitions();
        return scores;
      });
#endif
    }
    board = tilt_board(board, action);
    std::cout << directions[action] << std::endl;
  }
  std::cerr << "samples=" << AHC015_SAMPLES << " transitions=" << transitions << '\n';
  return 0;
}
