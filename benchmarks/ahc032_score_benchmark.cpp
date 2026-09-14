#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#define main ahc032_template_main
#include "../examples/search/ahc032_action_beam.cpp"
#undef main

// AHC032 "Mod Stamp" と同じ大きさ・入力分布・得点規則を使う、
// ActionBeamRunnerの固定seedベンチマーク。
// 実行時間だけでなく、探索で得た問題本来の得点を比較する。
//
// 公式generatorそのものではないため、AtCoderの提出得点とは比較しない。
// 全値を[0, 998244352]から一様生成し、同じseedなら常に同じ問題を作る。

struct SplitMix64 {
  std::uint64_t state;

  std::uint64_t next() {
    std::uint64_t value = (state += 0x9e3779b97f4a7c15ULL);
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
  }

  std::uint32_t below(std::uint32_t bound) {
    // 2^64がboundで割り切れない時の剰余biasをrejectionで除く。
    const std::uint64_t threshold = (std::uint64_t{0} - bound) % bound;
    while (true) {
      const std::uint64_t value = next();
      if (value >= threshold) {
        return static_cast<std::uint32_t>(value % bound);
      }
    }
  }
};

struct ModStampInstance {
  std::array<std::uint32_t, BOARD_SIZE * BOARD_SIZE> board{};
  std::array<std::array<std::uint32_t, STAMP_SIZE * STAMP_SIZE>,
             STAMP_COUNT>
      stamps{};
};

ModStampInstance make_instance(std::uint64_t seed) {
  SplitMix64 random{seed};
  ModStampInstance instance;
  for (std::uint32_t& value : instance.board) {
    value = random.below(MODULO);
  }
  for (auto& stamp : instance.stamps) {
    for (std::uint32_t& value : stamp) {
      value = random.below(MODULO);
    }
  }
  return instance;
}

long long board_score(
    const std::array<std::uint32_t, BOARD_SIZE * BOARD_SIZE>& board) {
  long long score = 0;
  for (std::uint32_t value : board) score += value;
  return score;
}

struct BenchmarkResult {
  long long score = 0;
  int operations = 0;
  double milliseconds = 0.0;
};

BenchmarkResult run_beam(ModStampProblem& problem, int width) {
  ActionBeamRunner<ModStampProblem> beam(
      problem, problem.make_initial_state(), problem.initial_score(), width);
  const auto start = std::chrono::steady_clock::now();
  const int advanced = beam.run(PLACEMENT_COUNT);
  const auto finish = std::chrono::steady_clock::now();
  if (advanced != PLACEMENT_COUNT) {
    throw std::runtime_error("beam stopped before completing the board");
  }
  problem.validate(beam.best());
  return {
      beam.best().finalized_score,
      beam.best().operations,
      std::chrono::duration<double, std::milli>(finish - start).count()};
}

int main(int argc, char** argv) {
  int case_count = 5;
  if (argc >= 2) case_count = std::stoi(argv[1]);
  if (case_count <= 0) {
    throw std::invalid_argument("case_count must be positive");
  }

  std::vector<int> widths{1, 100, 1000, 3000, 10000};
  if (argc >= 3) {
    widths.clear();
    for (int i = 2; i < argc; ++i) {
      const int width = std::stoi(argv[i]);
      if (width <= 0) {
        throw std::invalid_argument("beam width must be positive");
      }
      widths.push_back(width);
    }
  }

  std::vector<long long> total_scores(widths.size());
  std::vector<double> total_milliseconds(widths.size());
  long long initial_total = 0;

  std::cout << "AHC032-like Mod Stamp score benchmark\n";
  std::cout << "usage: ahc032_score_benchmark [case_count [width ...]]\n";
  std::cout << "seed initial_score width score gain operations milliseconds\n";
  for (int seed = 0; seed < case_count; ++seed) {
    const auto instance = make_instance(seed);
    ModStampProblem problem;
    problem.initial_board = instance.board;
    problem.stamps = instance.stamps;
    problem.prepare();
    const long long initial = board_score(problem.initial_board);
    initial_total += initial;
    for (std::size_t i = 0; i < widths.size(); ++i) {
      const BenchmarkResult result = run_beam(problem, widths[i]);
      total_scores[i] += result.score;
      total_milliseconds[i] += result.milliseconds;
      std::cout << seed << ' ' << initial << ' ' << widths[i] << ' '
                << result.score << ' ' << result.score - initial << ' '
                << result.operations << ' ' << std::fixed
                << std::setprecision(3) << result.milliseconds << '\n';
    }
  }

  const long double maximum_total =
      static_cast<long double>(case_count) * BOARD_SIZE * BOARD_SIZE *
      (MODULO - 1U);
  std::cout << "summary_width cases score_sum scaled_to_150 "
               "gain_vs_initial percent_of_cell_max milliseconds\n";
  for (std::size_t i = 0; i < widths.size(); ++i) {
    const long double percentage =
        100.0L * total_scores[i] / maximum_total;
    const long double scaled_to_150 =
        static_cast<long double>(total_scores[i]) * 150.0L / case_count;
    std::cout << widths[i] << ' ' << case_count << ' ' << total_scores[i]
              << ' ' << std::fixed << std::setprecision(0) << scaled_to_150
              << ' ' << total_scores[i] - initial_total << ' '
              << std::setprecision(6) << static_cast<double>(percentage) << ' '
              << std::setprecision(3) << total_milliseconds[i] << '\n';
  }
  std::cout << "reference_official_inputs terry_u16_second 11845290951426\n";
  std::cout << "reference_official_inputs editorial_first_equivalent "
               "11920072299359\n";
  std::cout << "reference_note: generated cases differ; scaled_to_150 is only "
               "a distribution-level estimate\n";
}
