#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <random>

// タイマーを内蔵した焼きなまし。これ1ファイルだけで使える。
// 使い方:
// TimeBasedSimulatedAnnealing sa(1900.0, 100.0, 1.0, 123);
// while (!sa.is_over()) {
//   double improvement = new_score - current_score;  // 最大化
//   if (sa.accept(improvement)) { ... }
// }
struct TimeBasedSimulatedAnnealing {
  double time_limit_ms;
  double start_temperature;
  double end_temperature;
  std::chrono::steady_clock::time_point start;
  std::mt19937_64 engine;

  TimeBasedSimulatedAnnealing(
      double time_limit_ms,
      double start_temperature,
      double end_temperature,
      std::uint64_t seed = 0)
      : time_limit_ms(time_limit_ms),
        start_temperature(start_temperature),
        end_temperature(end_temperature),
        start(std::chrono::steady_clock::now()),
        engine(seed) {
    assert(time_limit_ms > 0.0);
    assert(start_temperature > 0.0);
    assert(end_temperature > 0.0);
  }

  void reset() { start = std::chrono::steady_clock::now(); }

  double elapsed_ms() const {
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(now - start).count();
  }

  bool is_over() const { return elapsed_ms() >= time_limit_ms; }

  double progress() const {
    return std::clamp(elapsed_ms() / time_limit_ms, 0.0, 1.0);
  }

  double temperature(double progress) const {
    progress = std::clamp(progress, 0.0, 1.0);
    return start_temperature *
           std::pow(end_temperature / start_temperature, progress);
  }

  double current_temperature() const { return temperature(progress()); }

  // improvement は「変更後がどれだけ良くなるか」。
  // 最大化: new_score - current_score
  // 最小化: current_cost - new_cost
  template <class Score>
  bool accept(Score improvement) {
    const double value = static_cast<double>(improvement);
    if (value >= 0.0) return true;

    const double probability = std::exp(value / current_temperature());
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(engine) < probability;
  }
};
