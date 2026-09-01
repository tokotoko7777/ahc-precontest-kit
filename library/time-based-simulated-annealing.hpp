#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>

// タイマーを内蔵した焼きなまし。これ1ファイルだけで使える。
// 使い方:
// TimeBasedSimulatedAnnealing sa(1900.0, 100.0, 1.0, 123, 64);
// while (!sa.is_over()) {
//   double improvement = new_score - current_score;  // 最大化
//   if (sa.accept(improvement)) { ... }
// }
// 最後の 64 は時計を見る間隔。重い近傍なら1、軽い近傍なら64〜256が目安。
struct TimeBasedSimulatedAnnealing {
  double time_limit_ms;
  double start_temperature;
  double end_temperature;
  std::chrono::steady_clock::time_point start;
  std::mt19937_64 engine;
  mutable double log_start_temperature = 0.0;
  mutable double log_temperature_ratio = 0.0;
  int check_interval;
  mutable int calls_until_check = 0;
  mutable bool over = false;
  mutable double cached_elapsed_ms_value = 0.0;
  mutable double cached_progress_value = 0.0;
  mutable double cached_temperature_value;
  mutable double cached_inverse_temperature_value;

 private:
  bool linear_schedule_value = false;
  double cooling_power_value = 1.0;
  mutable double prepared_start_temperature = 0.0;
  mutable double prepared_end_temperature = 0.0;

  static void validate_temperatures(double start_value, double end_value) {
    if (!(start_value > 0.0) || !std::isfinite(start_value) ||
        !(end_value > 0.0) || !std::isfinite(end_value)) {
      throw std::invalid_argument("temperatures must be positive and finite");
    }
  }

  double shape_progress(double progress) const {
    if (cooling_power_value == 1.0) return progress;
    return std::pow(progress, cooling_power_value);
  }

  double temperature_from_prepared_settings(double progress) const {
    if (progress <= 0.0) return start_temperature;
    if (progress >= 1.0) return end_temperature;

    const double shaped_progress = shape_progress(progress);
    if (linear_schedule_value) {
      return start_temperature +
             (end_temperature - start_temperature) * shaped_progress;
    }
    return std::exp(
        log_start_temperature + log_temperature_ratio * shaped_progress);
  }

  void refresh_cached_temperature() const {
    cached_temperature_value =
        temperature_from_prepared_settings(cached_progress_value);
    cached_inverse_temperature_value = 1.0 / cached_temperature_value;
  }

  // 以前からpublicだった温度を直接書き換えたコードにも対応する。
  void synchronize_temperature_settings() const {
    if (start_temperature == prepared_start_temperature &&
        end_temperature == prepared_end_temperature) {
      return;
    }
    validate_temperatures(start_temperature, end_temperature);
    prepared_start_temperature = start_temperature;
    prepared_end_temperature = end_temperature;
    log_start_temperature = std::log(start_temperature);
    log_temperature_ratio =
        std::log(end_temperature) - log_start_temperature;
    refresh_cached_temperature();
  }

  double acceptance_exponent(double worsening_value) const {
    // 普通の温度では速い乗算を維持する。極小温度で1/Tがinfに
    // なった時だけ除算し、worsening_valueと温度の比を失わない。
    if (std::isfinite(cached_inverse_temperature_value)) {
      return worsening_value * cached_inverse_temperature_value;
    }
    return worsening_value / cached_temperature_value;
  }

  bool update_clock() const {
    synchronize_temperature_settings();
    calls_until_check = check_interval - 1;
    cached_elapsed_ms_value = elapsed_ms();
    cached_progress_value =
        std::clamp(cached_elapsed_ms_value / time_limit_ms, 0.0, 1.0);
    refresh_cached_temperature();
    over = cached_elapsed_ms_value >= time_limit_ms;
    return over;
  }

  bool accept_worsening(double exponent) {
    // 従来と同じく、悪化手では必ず乱数をちょうど1個消費する。
    const double random_value = random_01();
    // exp(-37) は random_01() の最小の正値 2^-53 より小さい。
    // random_value==0 の時だけunderflowを含めて従来式で確認する。
    if (exponent <= -37.0 && random_value != 0.0) return false;
    return random_value < std::exp(exponent);
  }

 public:
  TimeBasedSimulatedAnnealing(
      double time_limit_ms_value,
      double start_temperature_value,
      double end_temperature_value,
      std::uint64_t seed = 0,
      int check_interval_value = 1)
      : time_limit_ms(time_limit_ms_value),
        start_temperature(start_temperature_value),
        end_temperature(end_temperature_value),
        start(std::chrono::steady_clock::now()),
        engine(seed),
        check_interval(check_interval_value),
        cached_temperature_value(start_temperature_value),
        cached_inverse_temperature_value(0.0) {
    if (!(time_limit_ms > 0.0) || !std::isfinite(time_limit_ms)) {
      throw std::invalid_argument("time_limit_ms must be positive and finite");
    }
    if (check_interval <= 0) {
      throw std::invalid_argument("check_interval must be positive");
    }
    set_temperatures(start_temperature_value, end_temperature_value);
  }

  // 探索途中で温度範囲を変える時はこちらが分かりやすい。
  // 以前どおりpublic fieldを直接変えても、次回利用時に自動同期する。
  void set_temperatures(double new_start, double new_end) {
    validate_temperatures(new_start, new_end);
    start_temperature = new_start;
    end_temperature = new_end;
    prepared_start_temperature = start_temperature;
    prepared_end_temperature = end_temperature;
    log_start_temperature = std::log(start_temperature);
    log_temperature_ratio =
        std::log(end_temperature) - log_start_temperature;
    refresh_cached_temperature();
  }

  // 指数冷却が既定。必要な時だけ線形冷却へ切り替えられる。
  void use_linear_schedule() {
    synchronize_temperature_settings();
    linear_schedule_value = true;
    refresh_cached_temperature();
  }

  void use_geometric_schedule() {
    synchronize_temperature_settings();
    linear_schedule_value = false;
    refresh_cached_temperature();
  }

  // 1より大きいと高温を長く保ち、1未満なら早めに冷える。
  void set_cooling_power(double power) {
    if (!(power > 0.0) || !std::isfinite(power)) {
      throw std::invalid_argument("cooling power must be positive and finite");
    }
    synchronize_temperature_settings();
    cooling_power_value = power;
    refresh_cached_temperature();
  }

  void set_check_interval(int new_check_interval) {
    if (new_check_interval <= 0) {
      throw std::invalid_argument("check_interval must be positive");
    }
    check_interval = new_check_interval;
    calls_until_check = 0;
  }

  void reset() {
    synchronize_temperature_settings();
    start = std::chrono::steady_clock::now();
    calls_until_check = 0;
    over = false;
    cached_elapsed_ms_value = 0.0;
    cached_progress_value = 0.0;
    cached_temperature_value = start_temperature;
    cached_inverse_temperature_value = 1.0 / start_temperature;
  }

  double elapsed_ms() const {
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(now - start).count();
  }

  bool is_over() const {
    synchronize_temperature_settings();
    if (over) return true;
    if (calls_until_check > 0) {
      --calls_until_check;
      return false;
    }
    return update_clock();
  }

  // 間引きを無視して、時計・進捗率・温度を今すぐ更新する。
  bool is_over_now() const { return update_clock(); }

  // 現在時刻を読む精密な進捗率。熱いループでは cached_progress() を使う。
  double progress() const {
    return std::clamp(elapsed_ms() / time_limit_ms, 0.0, 1.0);
  }

  double cached_progress() const { return cached_progress_value; }

  double temperature(double progress) const {
    if (std::isnan(progress)) {
      throw std::invalid_argument("progress must not be NaN");
    }
    progress = std::clamp(progress, 0.0, 1.0);
    synchronize_temperature_settings();
    return temperature_from_prepared_settings(progress);
  }

  // 現在時刻を読む精密な温度。熱いループでは cached_temperature() を使う。
  double current_temperature() const { return temperature(progress()); }

  // accept() が実際に使う、最後の is_over() 確認時の温度。
  double cached_temperature() const {
    synchronize_temperature_settings();
    return cached_temperature_value;
  }

  double cached_elapsed_ms() const { return cached_elapsed_ms_value; }

  // accept() と同じ、最後に時計を確認した時の温度での採用確率。
  template <class Score>
  double acceptance_probability(Score improvement) const {
    const double value = static_cast<double>(improvement);
    if (std::isnan(value)) return 0.0;
    if (value >= 0.0) return 1.0;
    synchronize_temperature_settings();
    return std::exp(acceptance_exponent(value));
  }

  // d点悪化する手をprobabilityで採用したい時の温度を返す。
  static double temperature_for_acceptance(
      double typical_worsening,
      double probability) {
    if (!(typical_worsening > 0.0) ||
        !std::isfinite(typical_worsening) ||
        !(probability > 0.0 && probability < 1.0) ||
        !std::isfinite(probability)) {
      throw std::invalid_argument(
          "worsening must be positive and probability must be in (0, 1)");
    }
    const double result = -typical_worsening / std::log(probability);
    if (!(result > 0.0) || !std::isfinite(result)) {
      throw std::invalid_argument("calculated temperature is not finite");
    }
    return result;
  }

  // mt19937_64 の上位53bitから [0, 1) の double を作る。
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

    synchronize_temperature_settings();
    return accept_worsening(acceptance_exponent(value));
  }
};
