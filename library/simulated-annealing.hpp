#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <random>

struct SimulatedAnnealing {
  double start_temperature;
  double end_temperature;
  std::mt19937_64 engine;

  SimulatedAnnealing(
      double start_temperature,
      double end_temperature,
      std::uint64_t seed = 0)
      : start_temperature(start_temperature),
        end_temperature(end_temperature),
        engine(seed) {
    assert(start_temperature > 0.0);
    assert(end_temperature > 0.0);
  }

  double temperature(double progress) const {
    progress = std::clamp(progress, 0.0, 1.0);
    return start_temperature *
           std::pow(end_temperature / start_temperature, progress);
  }

  // improvement は「変更後がどれだけ良くなるか」。
  // 最大化: new_score - current_score
  // 最小化: current_cost - new_cost
  bool accept(double improvement, double progress) {
    if (improvement >= 0.0) return true;

    const double probability =
        std::exp(improvement / temperature(progress));
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(engine) < probability;
  }
};

