#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "library/time-based-simulated-annealing.hpp"

// AHC001 "AtCoder Ad" の公式入力分布と公式得点を使う。
// https://atcoder.jp/contests/ahc001/tasks/ahc001_a
//
// 【問題に合わせて書き換える場所】
//   AdvertisementProblem の State / Move と3関数。
//   - propose_move: 近傍を1個作る。作れない時はnullopt
//   - evaluate_move: Stateを変更せず改善量を返す
//   - apply_move: 採用された近傍だけを反映する
//
// 【ライブラリが担当する場所】
//   時間、温度、確率的採否、現在解・最良解、試行数の管理。
//   問題側は焼きなましのwhileループやexpを書かない。

constexpr int SPACE_SIZE = 10000;
constexpr long long SPACE_AREA = 1LL * SPACE_SIZE * SPACE_SIZE;

struct Request {
  int x = 0;
  int y = 0;
  long long desired_area = 1;
};

struct Rectangle {
  int left = 0;
  int bottom = 0;
  int right = 1;
  int top = 1;

  long long area() const {
    return 1LL * (right - left) * (top - bottom);
  }

  bool contains(int x, int y) const {
    return left <= x && x < right && bottom <= y && y < top;
  }

  bool overlaps(const Rectangle& other) const {
    return std::max(left, other.left) < std::min(right, other.right) &&
           std::max(bottom, other.bottom) < std::min(top, other.top);
  }
};

struct AdvertisementCase {
  std::vector<Request> requests;
};

AdvertisementCase make_case(std::uint64_t seed) {
  std::mt19937_64 engine(seed);
  std::uniform_real_distribution<double> real(0.0, 1.0);
  const int n = static_cast<int>(std::llround(50.0 * std::pow(4.0, real(engine))));

  AdvertisementCase input;
  input.requests.resize(static_cast<std::size_t>(n));
  std::unordered_set<std::uint64_t> used_points;
  used_points.reserve(static_cast<std::size_t>(n) * 2);
  for (Request& request : input.requests) {
    while (true) {
      const int x = std::uniform_int_distribution<int>(0, SPACE_SIZE - 1)(engine);
      const int y = std::uniform_int_distribution<int>(0, SPACE_SIZE - 1)(engine);
      const std::uint64_t key =
          static_cast<std::uint64_t>(x) * SPACE_SIZE + y;
      if (used_points.insert(key).second) {
        request.x = x;
        request.y = y;
        break;
      }
    }
  }

  std::unordered_set<int> used_cuts;
  used_cuts.reserve(static_cast<std::size_t>(n) * 2);
  std::vector<int> cuts;
  cuts.reserve(static_cast<std::size_t>(n + 1));
  cuts.push_back(0);
  while (static_cast<int>(cuts.size()) < n) {
    const int cut =
        std::uniform_int_distribution<int>(1, SPACE_AREA - 1)(engine);
    if (used_cuts.insert(cut).second) cuts.push_back(cut);
  }
  cuts.push_back(static_cast<int>(SPACE_AREA));
  std::sort(cuts.begin(), cuts.end());
  for (int i = 0; i < n; ++i) {
    input.requests[i].desired_area = cuts[i + 1] - cuts[i];
  }
  return input;
}

double satisfaction(long long desired, long long actual) {
  const double ratio =
      static_cast<double>(std::min(desired, actual)) /
      static_cast<double>(std::max(desired, actual));
  return 1.0 - (1.0 - ratio) * (1.0 - ratio);
}

struct AdvertisementProblem {
  // TODO: 【問題ごと】現在解と、差分評価に必要なcacheをここへ書く。
  struct State {
    std::vector<Rectangle> rectangle;
  };

  // TODO: 【問題ごと】近傍1回分の変更内容を小さくまとめる。
  struct Move {
    int index = 0;
    Rectangle next;
  };

  // TODO: 【問題ごと】得点型を選ぶ。大きいほど良い値にする。
  using Score = double;

  // TODO: 【問題ごと】全Stateで共通の入力・事前計算をProblemへ置く。
  const AdvertisementCase& input;

  // TODO: 【問題ごと】必ず合法な初期解を作る。
  State initial_state() const {
    State state;
    state.rectangle.reserve(input.requests.size());
    for (const Request& request : input.requests) {
      state.rectangle.push_back(
          {request.x, request.y, request.x + 1, request.y + 1});
    }
    return state;
  }

  // TODO: 【問題ごと】初期得点と完成解検査用の全再計算を書く。
  Score score(const State& state) const {
    Score result = 0.0;
    for (std::size_t i = 0; i < state.rectangle.size(); ++i) {
      result += satisfaction(
          input.requests[i].desired_area, state.rectangle[i].area());
    }
    return result;
  }

  static int random_int(std::mt19937_64& engine, int left, int right) {
    return std::uniform_int_distribution<int>(left, right - 1)(engine);
  }

  // TODO: 【問題ごと】次に試す近傍を1個作る。作れなければnullopt。
  // 他の長方形を越えない範囲で、選んだ1辺の新しい座標を作る。
  // 70%は希望面積に近い座標、30%は広い乱択で局所解を崩す。
  std::optional<Move> propose_move(const State& state,
                                   std::mt19937_64& engine,
                                   double progress) const {
    const int n = static_cast<int>(state.rectangle.size());
    const int index = random_int(engine, 0, n);
    const int side = random_int(engine, 0, 4);
    const Rectangle& current = state.rectangle[index];
    const Request& request = input.requests[index];
    Rectangle next = current;

    int minimum = 0;
    int maximum = SPACE_SIZE;
    int opposite = 0;
    int other_length = 1;
    if (side <= 1) {
      other_length = current.top - current.bottom;
      if (side == 0) {
        maximum = request.x + 1;
        opposite = current.right;
        for (int j = 0; j < n; ++j) {
          if (j == index) continue;
          const Rectangle& other = state.rectangle[j];
          const bool vertical_overlap =
              std::max(current.bottom, other.bottom) <
              std::min(current.top, other.top);
          if (vertical_overlap && other.right <= current.left) {
            minimum = std::max(minimum, other.right);
          }
        }
      } else {
        minimum = request.x + 1;
        opposite = current.left;
        for (int j = 0; j < n; ++j) {
          if (j == index) continue;
          const Rectangle& other = state.rectangle[j];
          const bool vertical_overlap =
              std::max(current.bottom, other.bottom) <
              std::min(current.top, other.top);
          if (vertical_overlap && current.right <= other.left) {
            maximum = std::min(maximum, other.left);
          }
        }
      }
    } else {
      other_length = current.right - current.left;
      if (side == 2) {
        maximum = request.y + 1;
        opposite = current.top;
        for (int j = 0; j < n; ++j) {
          if (j == index) continue;
          const Rectangle& other = state.rectangle[j];
          const bool horizontal_overlap =
              std::max(current.left, other.left) <
              std::min(current.right, other.right);
          if (horizontal_overlap && other.top <= current.bottom) {
            minimum = std::max(minimum, other.top);
          }
        }
      } else {
        minimum = request.y + 1;
        opposite = current.bottom;
        for (int j = 0; j < n; ++j) {
          if (j == index) continue;
          const Rectangle& other = state.rectangle[j];
          const bool horizontal_overlap =
              std::max(current.left, other.left) <
              std::min(current.right, other.right);
          if (horizontal_overlap && current.top <= other.bottom) {
            maximum = std::min(maximum, other.bottom);
          }
        }
      }
    }
    if (minimum >= maximum) return std::nullopt;

    int coordinate;
    if (random_int(engine, 0, 100) < 70) {
      const long long wanted_length = std::clamp<long long>(
          (request.desired_area + other_length / 2) / other_length,
          1,
          SPACE_SIZE);
      long long target = side == 0 || side == 2
                             ? opposite - wanted_length
                             : opposite + wanted_length;
      const int noise = std::max(
          1, static_cast<int>((1.0 - progress) * 200.0));
      target += random_int(engine, -noise, noise + 1);
      coordinate = std::clamp<int>(
          static_cast<int>(target), minimum, maximum - 1);
    } else {
      coordinate = random_int(engine, minimum, maximum);
    }

    if (side == 0) next.left = coordinate;
    if (side == 1) next.right = coordinate;
    if (side == 2) next.bottom = coordinate;
    if (side == 3) next.top = coordinate;
    if (next.left >= next.right || next.bottom >= next.top ||
        !next.contains(request.x, request.y) ||
        next.area() == current.area()) {
      return std::nullopt;
    }
    for (int j = 0; j < n; ++j) {
      if (j != index && next.overlaps(state.rectangle[j])) {
        return std::nullopt;
      }
    }
    return Move{index, next};
  }

  // TODO: 【問題ごと】Stateを変更せず、正=改善となる得点差を返す。
  std::optional<Score> evaluate_move(const State& state, const Move& move,
                                    double /* threshold */) const {
    // 差分はO(1)。閾値を使わず正確な改善量を返す。
    const long long desired = input.requests[move.index].desired_area;
    return satisfaction(desired, move.next.area()) -
           satisfaction(desired, state.rectangle[move.index].area());
  }

  // TODO: 【問題ごと】採用されたMoveだけをStateへ反映する。
  void apply_move(State& state, const Move& move) const {
    state.rectangle[move.index] = move.next;
  }

  void validate(const State& state) const {
    const int n = static_cast<int>(state.rectangle.size());
    for (int i = 0; i < n; ++i) {
      const Rectangle& rectangle = state.rectangle[i];
      if (rectangle.left < 0 || rectangle.bottom < 0 ||
          rectangle.right > SPACE_SIZE || rectangle.top > SPACE_SIZE ||
          rectangle.left >= rectangle.right ||
          rectangle.bottom >= rectangle.top ||
          !rectangle.contains(input.requests[i].x, input.requests[i].y)) {
        throw std::runtime_error("invalid advertisement rectangle");
      }
      for (int j = 0; j < i; ++j) {
        if (rectangle.overlaps(state.rectangle[j])) {
          throw std::runtime_error("advertisement rectangles overlap");
        }
      }
    }
  }

  long long official_score(const State& state) const {
    return std::llround(1'000'000'000.0 * score(state) /
                        state.rectangle.size());
  }
};

struct SearchResult {
  long long score = 0;
  std::uint64_t iterations = 0;
  std::uint64_t accepted = 0;
};

SearchResult run_search(const AdvertisementCase& input,
                        double time_limit_ms,
                        double start_temperature,
                        double end_temperature,
                        std::uint64_t seed) {
  AdvertisementProblem problem{input};
  AdvertisementProblem::State initial = problem.initial_state();
  const double initial_score = problem.score(initial);
  TimeBasedAnnealingRunner<AdvertisementProblem> runner(
      problem,
      std::move(initial),
      initial_score,
      time_limit_ms,
      start_temperature,
      end_temperature,
      seed,
      64);
  runner.run();
  problem.validate(runner.best_state());
  const double recalculated = problem.score(runner.best_state());
  if (std::abs(recalculated - runner.best_score()) > 1e-8) {
    throw std::runtime_error("incremental annealing score is inconsistent");
  }
  return {problem.official_score(runner.best_state()),
          runner.iterations(),
          runner.accepted_moves()};
}

int main(int argc, char** argv) {
  const int case_count = argc >= 2 ? std::stoi(argv[1]) : 5;
  const double time_limit_ms = argc >= 3 ? std::stod(argv[2]) : 1000.0;
  if (case_count <= 0 || !(time_limit_ms > 0.0)) {
    throw std::invalid_argument("case_count and time_limit_ms must be positive");
  }

  long long hill_total = 0;
  long long annealing_total = 0;
  std::uint64_t hill_iterations = 0;
  std::uint64_t annealing_iterations = 0;
  std::cout << "AHC001 time-based annealing score benchmark\n";
  std::cout << "usage: ahc001_annealing_score_benchmark "
               "[case_count [time_limit_ms]]\n";
  std::cout << "seed companies hill_score annealing_score gain "
               "hill_iterations annealing_iterations\n";
  for (int seed = 0; seed < case_count; ++seed) {
    const AdvertisementCase input = make_case(seed);
    // ほぼ0度なら悪化手を受け入れず、同じ近傍の山登り比較になる。
    const SearchResult hill =
        run_search(input, time_limit_ms, 1e-12, 1e-12, seed);
    const SearchResult annealing =
        run_search(input, time_limit_ms, 0.05, 1e-6, seed);
    hill_total += hill.score;
    annealing_total += annealing.score;
    hill_iterations += hill.iterations;
    annealing_iterations += annealing.iterations;
    std::cout << seed << ' ' << input.requests.size() << ' ' << hill.score
              << ' ' << annealing.score << ' '
              << annealing.score - hill.score << ' ' << hill.iterations
              << ' ' << annealing.iterations << '\n';
  }
  std::cout << "summary cases time_limit_ms hill_average annealing_average "
               "gain average_hill_iterations average_annealing_iterations\n";
  std::cout << case_count << ' ' << std::fixed << std::setprecision(3)
            << time_limit_ms << ' '
            << static_cast<double>(hill_total) / case_count << ' '
            << static_cast<double>(annealing_total) / case_count << ' '
            << static_cast<double>(annealing_total - hill_total) / case_count
            << ' ' << hill_iterations / case_count << ' '
            << annealing_iterations / case_count << '\n';
  std::cout << "reference_reported_official_1000_cases strong_annealing "
               "average_about 991270000\n";
  std::cout << "reference_note: generated cases differ; compare only as a "
               "rough distribution-level guide\n";
}
