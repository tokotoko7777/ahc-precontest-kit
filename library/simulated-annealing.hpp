#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>

// 進捗率を自分で渡す焼きなまし。
// 使い方:
// SimulatedAnnealing sa(100.0, 1.0, 123);
// while (!timer.is_over()) {
//   // BatchedTimer なら同じ進捗率が続く間は温度を再計算しない。
//   sa.set_progress(timer.cached_progress());
//   double improvement = new_score - current_score;  // 最大化
//   if (sa.accept(improvement)) { ... }
// }
//
// 従来どおり sa.accept(improvement, progress) と書いてもよい。
struct SimulatedAnnealing {
  double start_temperature;
  double end_temperature;
  std::mt19937_64 engine;
  double log_start_temperature = 0.0;
  double log_temperature_ratio = 0.0;
  double cached_progress_value = 0.0;
  double cached_temperature_value;
  double cached_inverse_temperature_value;

  SimulatedAnnealing(
      double start_temperature_value,
      double end_temperature_value,
      std::uint64_t seed = 0)
      : start_temperature(start_temperature_value),
        end_temperature(end_temperature_value),
        engine(seed),
        cached_temperature_value(start_temperature_value),
        cached_inverse_temperature_value(0.0) {
    set_temperatures(start_temperature_value, end_temperature_value);
  }

  // 探索途中で温度範囲を変える時は、public fieldを直接変えずこれを使う。
  void set_temperatures(double new_start, double new_end) {
    if (!(new_start > 0.0) || !std::isfinite(new_start) ||
        !(new_end > 0.0) || !std::isfinite(new_end)) {
      throw std::invalid_argument("temperatures must be positive and finite");
    }
    start_temperature = new_start;
    end_temperature = new_end;
    log_start_temperature = std::log(start_temperature);
    log_temperature_ratio =
        std::log(end_temperature) - log_start_temperature;
    cached_temperature_value = temperature(cached_progress_value);
    cached_inverse_temperature_value = 1.0 / cached_temperature_value;
  }

  double temperature(double progress) const {
    progress = std::clamp(progress, 0.0, 1.0);
    if (progress <= 0.0) return start_temperature;
    if (progress >= 1.0) return end_temperature;
    return std::exp(
        log_start_temperature + log_temperature_ratio * progress);
  }

  // 同じ progress を繰り返し渡しても温度は最初の1回しか計算しない。
  void set_progress(double progress) {
    if (std::isnan(progress)) {
      throw std::invalid_argument("progress must not be NaN");
    }
    progress = std::clamp(progress, 0.0, 1.0);
    if (progress == cached_progress_value) return;
    cached_progress_value = progress;
    cached_temperature_value = temperature(progress);
    cached_inverse_temperature_value = 1.0 / cached_temperature_value;
  }

  double cached_progress() const { return cached_progress_value; }

  double current_temperature() const { return cached_temperature_value; }

  // mt19937_64 の上位53bitから [0, 1) の double を作る。
  // uniform_real_distribution を熱いループで毎回作らない。
  double random_01() {
    constexpr double inverse = 1.0 / 9007199254740992.0;  // 2^53
    return static_cast<double>(engine() >> 11) * inverse;
  }

  // improvement は「変更後がどれだけ良くなるか」。
  // 最大化: new_score - current_score
  // 最小化: current_cost - new_cost
  template <class Score>
  bool accept(Score improvement) {
    const double value = static_cast<double>(improvement);
    if (std::isnan(value)) return false;
    if (value >= 0.0) return true;

    const double probability =
        std::exp(value * cached_inverse_temperature_value);
    return random_01() < probability;
  }

  template <class Score>
  bool accept(Score improvement, double progress) {
    set_progress(progress);
    return accept(improvement);
  }
};
