// GCC提出用の最適化。診断時は -DAHC_DISABLE_GCC_OPTIMIZE で無効化。
#if defined(__GNUC__) && !defined(__clang__) && !defined(AHC_DISABLE_GCC_OPTIMIZE)
#pragma GCC optimize("O3")
#endif

// 実戦例のProblemとRunnerをそのまま使い、独自生成入力で幅を比較する。
#define main ahc021_submission_main
#include "../examples/search/ahc021_tree_beam.cpp"
#undef main

std::array<int, CELL_COUNT> make_case(std::uint64_t seed) {
  std::array<int, CELL_COUNT> permutation{};
  std::iota(permutation.begin(), permutation.end(), 0);
  std::mt19937_64 engine(seed);
  std::shuffle(permutation.begin(), permutation.end(), engine);
  return permutation;
}

int main(int argc, char** argv) {
  int case_count = argc >= 2 ? std::stoi(argv[1]) : 5;
  if (case_count <= 0) {
    throw std::invalid_argument("case_count must be positive");
  }
  std::vector<int> widths{1, 10, 40};
  if (argc >= 3) {
    widths.clear();
    for (int i = 2; i < argc; ++i) {
      const int width = std::stoi(argv[i]);
      if (width <= 0) throw std::invalid_argument("width must be positive");
      widths.push_back(width);
    }
  }

  std::vector<long long> total_score(widths.size());
  std::vector<long long> total_operations(widths.size());
  std::vector<double> total_milliseconds(widths.size());
  std::cout << "AHC021 apply/revert tree beam score benchmark\n";
  std::cout << "usage: ahc021_tree_beam_score_benchmark "
               "[case_count [width ...]]\n";
  std::cout << "seed width score operations milliseconds\n";
  for (int seed = 0; seed < case_count; ++seed) {
    const auto input = make_case(static_cast<std::uint64_t>(seed));
    for (std::size_t index = 0; index < widths.size(); ++index) {
      const PyramidResult result = solve_case(input, widths[index]);
      total_score[index] += result.score;
      total_operations[index] += result.operations;
      total_milliseconds[index] += result.milliseconds;
      std::cout << seed << ' ' << widths[index] << ' ' << result.score << ' '
                << result.operations << ' ' << std::fixed
                << std::setprecision(3) << result.milliseconds << '\n';
    }
  }
  std::cout << "summary_width cases score_sum average_score "
               "average_operations milliseconds\n";
  for (std::size_t index = 0; index < widths.size(); ++index) {
    std::cout << widths[index] << ' ' << case_count << ' '
              << total_score[index] << ' ' << std::fixed
              << std::setprecision(3)
              << static_cast<double>(total_score[index]) / case_count << ' '
              << static_cast<double>(total_operations[index]) / case_count
              << ' ' << total_milliseconds[index] << '\n';
  }
  std::cout << "reference_reported_official_150_cases early_top_score "
               "13426585 average 89510.567\n";
  std::cout << "reference_note: generated shuffles differ; compare only as a "
               "rough distribution-level guide\n";
}
