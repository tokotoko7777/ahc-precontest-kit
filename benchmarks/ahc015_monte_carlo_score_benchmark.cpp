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

#include "library/common-scenario-average.hpp"

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

Board run_rule_policy(const CandyCase& input) {
  Board board{};
  for (int turn = 0; turn < CELL_COUNT; ++turn) {
    place_by_rank(board, input.placement_rank[turn], input.flavor[turn]);
    if (turn + 1 < CELL_COUNT) {
      board = tilt_board(
          board, rule_direction(input.flavor[turn], input.flavor[turn + 1]));
    }
  }
  return board;
}

Board run_rollout(const CandyCase& input,
                  int sample_count,
                  std::uint64_t seed) {
  CandyRolloutProblem problem{input};
  CommonScenarioRolloutRunner<CandyRolloutProblem> runner(problem, seed);
  runner.reserve(4, sample_count);
  Board board{};
  for (int turn = 0; turn < CELL_COUNT; ++turn) {
    place_by_rank(board, input.placement_rank[turn], input.flavor[turn]);
    if (turn + 1 == CELL_COUNT) break;
    const int action = runner.choose_action({board, turn}, sample_count);
    board = tilt_board(board, action);
  }
  return board;
}

int main(int argc, char** argv) {
  int case_count = argc >= 2 ? std::stoi(argv[1]) : 5;
  if (case_count <= 0) throw std::invalid_argument("case_count must be positive");
  std::vector<int> samples{16, 64, 256, 512};
  if (argc >= 3) {
    samples.clear();
    for (int i = 2; i < argc; ++i) {
      const int value = std::stoi(argv[i]);
      if (value <= 0) throw std::invalid_argument("samples must be positive");
      samples.push_back(value);
    }
  }

  std::vector<long long> total(samples.size());
  std::vector<double> milliseconds(samples.size());
  long long rule_total = 0;
  std::cout << "AHC015 common-scenario Monte Carlo score benchmark\n";
  std::cout << "usage: ahc015_monte_carlo_score_benchmark "
               "[case_count [samples ...]]\n";
  std::cout << "seed rule_score samples rollout_score gain milliseconds\n";
  for (int seed = 0; seed < case_count; ++seed) {
    const CandyCase input = make_case(static_cast<std::uint64_t>(seed));
    const long long rule_score = official_score(run_rule_policy(input), input);
    rule_total += rule_score;
    for (std::size_t index = 0; index < samples.size(); ++index) {
      const auto start = std::chrono::steady_clock::now();
      const Board answer = run_rollout(
          input, samples[index], 0x9e3779b97f4a7c15ULL ^ seed);
      const auto finish = std::chrono::steady_clock::now();
      const long long score = official_score(answer, input);
      const double elapsed = std::chrono::duration<double, std::milli>(
                                 finish - start)
                                 .count();
      total[index] += score;
      milliseconds[index] += elapsed;
      std::cout << seed << ' ' << rule_score << ' ' << samples[index] << ' '
                << score << ' ' << score - rule_score << ' ' << std::fixed
                << std::setprecision(3) << elapsed << '\n';
    }
  }

  std::cout << "summary_samples cases score_sum average gain_vs_rule "
               "milliseconds\n";
  for (std::size_t index = 0; index < samples.size(); ++index) {
    std::cout << samples[index] << ' ' << case_count << ' ' << total[index]
              << ' ' << std::fixed << std::setprecision(3)
              << static_cast<double>(total[index]) / case_count << ' '
              << total[index] - rule_total << ' ' << milliseconds[index]
              << '\n';
  }
  std::cout << "reference_reported_official_200_cases "
               "problem_aware_monte_carlo 161276478 average 806382.390\n";
  std::cout << "reference_note: generated cases differ; compare only as a "
               "rough distribution-level guide\n";
}
