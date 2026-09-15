#include <bits/stdc++.h>
using namespace std;
// BEGIN LIBRARY: batched-timer.hpp
#include <algorithm>
#include <cassert>
#include <chrono>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/batched-timer.hpp

// 時計を見る回数を減らすタイマー。1反復がとても軽い探索向け。
// 最初の呼び出しと、その後 check_interval 回ごとに時計を見る。
//
// 使い方:
// BatchedTimer timer(1900.0, 256);
// while (!timer.is_over()) {
//   // 軽い探索を1回進める
// }
struct BatchedTimer {
  double time_limit_ms;
  int check_interval;
  int calls_until_check = 0;
  bool over = false;
  double last_elapsed_ms = 0.0;
  std::chrono::steady_clock::time_point start;

  BatchedTimer(double time_limit_ms_value, int check_interval_value)
      : time_limit_ms(time_limit_ms_value),
        check_interval(check_interval_value),
        start(std::chrono::steady_clock::now()) {
    assert(time_limit_ms_value > 0.0);
    assert(check_interval_value > 0);
  }

  void reset() {
    calls_until_check = 0;
    over = false;
    last_elapsed_ms = 0.0;
    start = std::chrono::steady_clock::now();
  }

  double elapsed_ms() const {
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(now - start).count();
  }

  bool is_over() {
    if (over) return true;
    if (calls_until_check > 0) {
      --calls_until_check;
      return false;
    }
    calls_until_check = check_interval - 1;
    last_elapsed_ms = elapsed_ms();
    over = last_elapsed_ms >= time_limit_ms;
    return over;
  }

  // 最後に時計を見た時点の進捗率。時計APIは呼ばない。
  double cached_progress() const {
    return std::clamp(last_elapsed_ms / time_limit_ms, 0.0, 1.0);
  }
};
// END LIBRARY: batched-timer.hpp
// BEGIN LIBRARY: random.hpp
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/random.hpp

// 使い方:
// Random random(123);
// int x = random.next_int(0, 10);        // 0以上10未満
// long long y = random.next_int(0LL, 1LL << 40);
// double p = random.next_real();         // 0以上1未満
struct Random {
  std::mt19937_64 engine;

  explicit Random(std::uint64_t seed = 0) : engine(seed) {}

  std::uint64_t next_u64() { return engine(); }

  // [left, right) の整数を返す。int、long long などを選べる。
  template <class Int>
  Int next_int(Int left, Int right) {
    assert(left < right);
    std::uniform_int_distribution<Int> distribution(left, right - 1);
    return distribution(engine);
  }

  // [left, right) の小数を返す。型を省略すると double。
  template <class Real = double>
  Real next_real(Real left = Real(0), Real right = Real(1)) {
    assert(left < right);
    std::uniform_real_distribution<Real> distribution(left, right);
    return distribution(engine);
  }

  double next_double() { return next_real<double>(); }

  template <class T>
  void shuffle(std::vector<T>& values) {
    std::shuffle(values.begin(), values.end(), engine);
  }

  template <class T>
  T& choice(std::vector<T>& values) {
    assert(!values.empty());
    return values[next_int<std::size_t>(0, values.size())];
  }

  template <class T>
  const T& choice(const std::vector<T>& values) {
    assert(!values.empty());
    return values[next_int<std::size_t>(0, values.size())];
  }

  // weights[i] に比例する確率で添字 i を返す。
  template <class Weight>
  int weighted_index(const std::vector<Weight>& weights) {
    assert(!weights.empty());

    long double total = 0.0L;
    for (const Weight& weight : weights) {
      assert(weight >= Weight(0));
      total += static_cast<long double>(weight);
    }
    assert(total > 0.0L);

    const long double target = next_real<long double>(0.0L, total);
    long double sum = 0.0L;
    for (int i = 0; i < static_cast<int>(weights.size()); ++i) {
      sum += static_cast<long double>(weights[i]);
      if (target < sum) return i;
    }
    return static_cast<int>(weights.size()) - 1;
  }
};
// END LIBRARY: random.hpp
// BEGIN LIBRARY: axis-aligned-rectangle.hpp
#include <algorithm>
#include <cassert>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/axis-aligned-rectangle.hpp

// 軸に平行な半開長方形 [left, right) × [bottom, top)。
// Coordinate は int、long long などを選べる。
// 辺が接するだけなら overlaps() は false。
//
// 使い方:
// AxisAlignedRectangle<int> rectangle{0, 0, 10, 20};
// long long area = rectangle.area();
// bool hit = rectangle.overlaps(other);
template <class Coordinate>
struct AxisAlignedRectangle {
  Coordinate left;
  Coordinate bottom;
  Coordinate right;
  Coordinate top;

  bool is_valid() const { return left < right && bottom < top; }

  Coordinate width() const {
    assert(is_valid());
    return right - left;
  }

  Coordinate height() const {
    assert(is_valid());
    return top - bottom;
  }

  long long area() const {
    return 1LL * width() * height();
  }

  bool contains(Coordinate x, Coordinate y) const {
    return left <= x && x < right && bottom <= y && y < top;
  }

  bool overlaps(const AxisAlignedRectangle& other) const {
    return std::max(left, other.left) < std::min(right, other.right) &&
           std::max(bottom, other.bottom) < std::min(top, other.top);
  }
};
// END LIBRARY: axis-aligned-rectangle.hpp
// BEGIN LIBRARY: largest-empty-rectangle.hpp
#include <algorithm>
#include <stdexcept>
#include <vector>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/largest-empty-rectangle.hpp

// 整数格子の指定セル(x,y)を含む、障害物と重ならない最大面積の長方形。
// bounds内に限定する。境界が接するだけなら重なりではない。
// TODO: Rectへleft,bottom,right,topとarea()を書く。座標型は同じ整数型にする。
//   area()は面積がoverflowしない型を返すこと。
// TODO: obstaclesへ使用中の半開長方形を入れる。動かす対象自身は除く。
//   各障害物は正の面積を持つこと。障害物同士の重なりは許す。
// auto answer = largest_empty_rectangle(bounds, x, y, obstacles);
// 不正なbounds、範囲外/塞がれた指定セルはinvalid_argument。
// 計算量O(M^2 + M log M)、追加メモリO(M)。Mは障害物数。
// AHC001のような、点を含む広告の再配置で使える。
// 最大「面積」であり、任意の評価関数を最大化する関数ではない。
template <class Rect>
Rect largest_empty_rectangle(const Rect& bounds, decltype(Rect::left) x,
                             decltype(Rect::bottom) y,
                             const std::vector<Rect>& obstacles) {
  using Coordinate = decltype(Rect::left);
  if (!(bounds.left <= x && x < bounds.right &&
        bounds.bottom <= y && y < bounds.top)) {
    throw std::invalid_argument("anchor cell is outside bounds");
  }
  Coordinate low = bounds.left, high = bounds.right;
  // 指定セルと同じ高さを塞ぐ障害物で、左右の到達可能範囲を先に絞る。
  for (const auto& o : obstacles) {
    if (!(o.left < o.right && o.bottom < o.top)) {
      throw std::invalid_argument("invalid obstacle");
    }
    if (o.bottom <= y && y < o.top) {
      if (o.right <= x) low = std::max(low, o.right);
      else if (x < o.left) high = std::min(high, o.left);
      else throw std::invalid_argument("anchor cell is blocked");
    }
  }
  std::vector<Coordinate> lefts{low};
  std::vector<const Rect*> rights;
  rights.reserve(obstacles.size());
  for (const auto& o : obstacles) {
    if (low < o.right && o.right <= x) lefts.push_back(o.right);
    if (x < o.left && o.left < high) rights.push_back(&o);
  }
  std::sort(lefts.begin(), lefts.end());
  lefts.erase(std::unique(lefts.begin(), lefts.end()), lefts.end());
  std::sort(rights.begin(), rights.end(), [](const Rect* a, const Rect* b) {
    return a->left < b->left;
  });
  Rect best{x, y, static_cast<Coordinate>(x + 1), static_cast<Coordinate>(y + 1)};
  for (Coordinate left : lefts) {
    Coordinate bottom = bounds.bottom, top = bounds.top;
    const auto restrict_y = [&](const Rect& o) {
      if (o.top <= y) bottom = std::max(bottom, o.top);
      else if (y < o.bottom) top = std::min(top, o.bottom);
    };
    for (const auto& o : obstacles) {
      if (left < o.right && o.left <= x) restrict_y(o);
    }
    const auto consider = [&](Coordinate right) {
      Rect candidate{left, bottom, right, top};
      if (best.area() < candidate.area()) best = candidate;
    };
    for (const Rect* o : rights) {
      consider(o->left); // 障害物へ接する所までは伸ばせる。
      restrict_y(*o);   // そこを越えるなら上下の幅を狭める。
    }
    consider(high);
  }
  return best;
}
// END LIBRARY: largest-empty-rectangle.hpp
// BEGIN LIBRARY: time-based-simulated-annealing.hpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/time-based-simulated-annealing.hpp
// 前計算の着想: https://github.com/asi1024/MarathonLibrary/blob/13fe8241ac04fcdcfb8da6e84b050bd4b23ad3a9/snippets/simulated_annealing.h
// この実装は近似値だけで採否を決めず、区間内だけ通常のlogで確認する独自実装。

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
  // 各分割点のlogを上下へ1 ULP広げた境界。未使用なら確保しない。
  std::vector<std::pair<double, double>> threshold_log_bounds_;

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

  // TODO: 【任意】軽い差分評価の試行数が多い時だけ、探索前に4096等を指定する。
  // 0で無効（既定）。2のべき乗、最大2^20。約16*(bins+1) bytesを使う。
  // 構築はO(bins)。時刻・乱数列はリセットしない。accept()には影響しない。
  // 固定表を順番に巡回せず、各試行で従来と同じ新しい乱数を1個使う。
  void set_threshold_table_size(std::size_t bins) {
    if (bins == 0) {
      threshold_log_bounds_.clear();
      return;
    }
    if (bins > (std::size_t{1} << 20) || (bins & (bins - 1)) != 0) {
      throw std::invalid_argument("threshold table size must be a power of two up to 2^20");
    }
    std::vector<std::pair<double, double>> bounds(bins + 1);
    const double infinity = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i <= bins; ++i) {
      const double value = i == 0 ? -infinity :
          std::log(static_cast<double>(i) / static_cast<double>(bins));
      bounds[i] = {std::nextafter(value, -infinity), std::nextafter(value, infinity)};
    }
    threshold_log_bounds_.swap(bounds);
  }

  std::size_t threshold_table_size() const {
    return threshold_log_bounds_.empty() ? 0 : threshold_log_bounds_.size() - 1;
  }

  // TODO: 【任意】対数表の前計算だけをON/OFFする。スコア途中打ち切りとは独立。
  // falseでもrun_with_threshold(true)で途中打ち切りできる。
  // 距離表など「問題固有の前計算」を行う設定ではない。
  void set_threshold_precomputation(bool enabled, std::size_t bins = 4096) {
    if (enabled && bins == 0) {
      throw std::invalid_argument("enabled precomputation needs a nonzero table size");
    }
    set_threshold_table_size(enabled ? bins : 0);
  }

  bool threshold_precomputation_enabled() const {
    return !threshold_log_bounds_.empty();
  }

  // 受理閾値が入る区間。lowerは安全な枝刈り用で、最終採否そのものではない。
  // TODO: evaluate_move_with_thresholdへlowerを渡し、最後はaccept()で確認する。
  // 値が区間内なら元のT*log(u)を計算するので、希少な悪化手も切り捨てない。
  struct AcceptanceWindow {
    double lower, upper, uniform_value, temperature;

    template <class Score>
    bool accept(Score improvement) const {
      const double value = static_cast<double>(improvement);
      if (std::isnan(value) || value <= lower) return false;
      if (value > upper) return true;
      return value > temperature * std::log(uniform_value);
    }
  };

  AcceptanceWindow draw_acceptance_window() {
    synchronize_temperature_settings();
    const double u = random_01();
    const double t = cached_temperature_value;
    if (threshold_log_bounds_.empty()) {
      const double threshold = t * std::log(u);
      return {threshold, threshold, u, t};
    }
    const std::size_t bins = threshold_table_size();
    // binsが2のべき乗なので、[0,1)の53-bit乱数の区間を正確に選べる。
    const auto index = static_cast<std::size_t>(u * static_cast<double>(bins));
    return {t * threshold_log_bounds_[index].first,
            t * threshold_log_bounds_[index + 1].second, u, t};
  }

  // この試行が採用されるために必要な最小improvementを先に返す。
  // improvement > thresholdなら採用。差分評価が重い時は、この閾値を
  // 評価関数へ渡し、超えないと証明できた時点で計算を打ち切れる。
  // 良化手を含む全試行で乱数を1個消費するためaccept()と乱数列は異なるが、
  // 各手の採用確率は同じ。
  double draw_acceptance_threshold() {
    synchronize_temperature_settings();
    return cached_temperature_value * std::log(random_01());
  }

  template <class Score>
  bool accept_with_threshold(Score improvement, double threshold) const {
    const double value = static_cast<double>(improvement);
    if (std::isnan(value) || std::isnan(threshold)) return false;
    return value > threshold;
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

// 問題依存部分をProblemへ集める、時間ベース焼きなましの薄いRunner。
//
// 【使う人がmain.cpp側へ書く場所】
// 次のTODOだけを自分の問題に合わせる。Runner本体は通常変更しない。
// 空関数を配置済みの雛形: template/search/time-based-annealing.cpp
//
//   TODO: 【問題ごと】Stateへ現在解1個と差分更新用cacheを書く。
//   using State = ...;
//   TODO: 【問題ごと】Moveへ近傍1回分の小さい情報を書く。
//   using Move = ...;
//   TODO: 【問題ごと】Scoreを選ぶ。Runnerでは大きいほど良い値にする。
//   using Score = ...;
//
//   TODO: 【問題ごと】次に試す近傍を1個作る。
//   optional<Move> propose_move(const State&, mt19937_64&, double progress)
//     -> 近傍を1個作る。作れない試行はnullopt。
//   TODO: 【問題ごと】近傍による改善量を差分計算する。
//   Score evaluate_move(const State&, const Move&)
//     -> その手の改善量。正なら良化、負なら悪化。
//   TODO: 【任意】閾値で差分計算を途中終了する。
//   optional<Score> evaluate_move_with_threshold(
//       const State&, const Move&, double acceptance_threshold)
//     -> 閾値を超えないと証明できた時だけnullopt。その他は正確な改善量。
//   TODO: 【問題ごと】採用された近傍だけを現在解へ反映する。
//   void apply_move(State&, Move&)
//     -> 採用済みの手だけをStateへ反映する。Move内のvector等はmoveしてよい。
//
// Runnerは時計、温度、採否、現在解、最良解、件数統計を担当する。
// evaluate_moveはStateを書き換えない。この形なら不採用時のrevertは不要。
// ↓↓↓ ここから下はライブラリ本体。通常は編集しない。↓↓↓
template <class Problem>
struct TimeBasedAnnealingRunner {
  using State = typename Problem::State;
  using Move = typename Problem::Move;
  using Score = typename Problem::Score;

  TimeBasedAnnealingRunner(Problem& problem,
                           State initial_state,
                           Score initial_score,
                           double time_limit_ms,
                           double start_temperature,
                           double end_temperature,
                           std::uint64_t seed = 0,
                           int check_interval = 1)
      : problem_(problem),
        current_state_(std::move(initial_state)),
        best_state_(current_state_),
        current_score_(std::move(initial_score)),
        best_score_(current_score_),
        annealing_(time_limit_ms,
                   start_temperature,
                   end_temperature,
                   seed,
                   check_interval),
        move_engine_(seed ^ 0xd1b54a32d192ed03ULL) {}

  // 制限時間内なら1試行進める。時間終了後はfalse。
  bool step() {
    if (annealing_.is_over()) return false;
    ++iterations_;
    std::optional<Move> move = problem_.propose_move(
        static_cast<const State&>(current_state_),
        move_engine_,
        annealing_.cached_progress());
    if (!move.has_value()) return true;

    ++valid_moves_;
    const Score improvement = problem_.evaluate_move(
        static_cast<const State&>(current_state_), *move);
    if (!annealing_.accept(improvement)) return true;

    problem_.apply_move(current_state_, *move);
    current_score_ += improvement;
    ++accepted_moves_;
    if (best_score_ < current_score_) {
      best_score_ = current_score_;
      best_state_ = current_state_;
      ++best_updates_;
    }
    return true;
  }

  // 差分評価が重く、途中で採用不能と証明できる問題向け。
  // Problem::evaluate_move_with_thresholdは、improvementがthresholdを
  // 超えないと証明できた時だけnulloptを返す。分からない場合は最後まで
  // 計算し、正確なimprovementを返せば通常の焼きなましと同じ分布になる。
  // 区間表を有効にした場合は実際の閾値以下の下限を渡し、最終採否を別途確認する。
  bool step_with_threshold() {
    return step_threshold_impl<true>();
  }

  // true: evaluate_move_with_thresholdで安全に途中打ち切り。
  // false: evaluate_moveで最後まで計算する。閾値の抽選・最終採否は同じ。
  // このbool版を使うProblemには上記の両方の関数を書く（雛形に配置済み）。
  bool step_with_threshold(bool enable_score_early_stop) {
    return enable_score_early_stop ? step_threshold_impl<true>()
                                   : step_threshold_impl<false>();
  }

 private:
  template <bool EnableScoreEarlyStop>
  bool step_threshold_impl() {
    if (annealing_.is_over()) return false;
    ++iterations_;
    std::optional<Move> move = problem_.propose_move(
        static_cast<const State&>(current_state_),
        move_engine_,
        annealing_.cached_progress());
    if (!move.has_value()) return true;

    ++valid_moves_;
    const auto threshold = annealing_.draw_acceptance_window();
    std::optional<Score> improvement;
    if constexpr (EnableScoreEarlyStop) {
      improvement = problem_.evaluate_move_with_threshold(
          static_cast<const State&>(current_state_), *move, threshold.lower);
    } else {
      improvement = problem_.evaluate_move(
          static_cast<const State&>(current_state_), *move);
    }
    if (!improvement.has_value()) {
      ++threshold_pruned_moves_;
      return true;
    }
    if (!threshold.accept(*improvement)) return true;

    problem_.apply_move(current_state_, *move);
    current_score_ += *improvement;
    ++accepted_moves_;
    if (best_score_ < current_score_) {
      best_score_ = current_score_;
      best_state_ = current_state_;
      ++best_updates_;
    }
    return true;
  }

 public:
  std::uint64_t run() {
    while (step()) {
    }
    return iterations_;
  }

  std::uint64_t run_with_threshold() {
    while (step_with_threshold()) {
    }
    return iterations_;
  }

  // TODO: スコア途中打ち切りだけをON/OFFする。前計算とは別の設定。
  // falseでも前計算した区間表を最終採否に使える。
  // 分岐は探索開始時の1回だけ。通常のrun()とは異なり、ON/OFFで同じ乱数を消費する。
  std::uint64_t run_with_threshold(bool enable_score_early_stop) {
    if (enable_score_early_stop) return run_with_threshold();
    while (step_threshold_impl<false>()) {
    }
    return iterations_;
  }

  // 現在解が深く悪化した時、保存済みの最良解から探索を再開する。
  // 温度、時計、乱数列、試行件数はそのまま継続する。
  void restart_from_best() {
    current_state_ = best_state_;
    current_score_ = best_score_;
    ++restarts_;
  }

  const State& current_state() const { return current_state_; }
  const State& best_state() const { return best_state_; }
  const Score& current_score() const { return current_score_; }
  const Score& best_score() const { return best_score_; }
  std::uint64_t iterations() const { return iterations_; }
  std::uint64_t valid_moves() const { return valid_moves_; }
  std::uint64_t accepted_moves() const { return accepted_moves_; }
  std::uint64_t best_updates() const { return best_updates_; }
  std::uint64_t restarts() const { return restarts_; }
  std::uint64_t threshold_pruned_moves() const {
    return threshold_pruned_moves_;
  }

  TimeBasedSimulatedAnnealing& annealing() { return annealing_; }
  const TimeBasedSimulatedAnnealing& annealing() const { return annealing_; }

 private:
  Problem& problem_;
  State current_state_;
  State best_state_;
  Score current_score_;
  Score best_score_;
  TimeBasedSimulatedAnnealing annealing_;
  std::mt19937_64 move_engine_;
  std::uint64_t iterations_ = 0;
  std::uint64_t valid_moves_ = 0;
  std::uint64_t accepted_moves_ = 0;
  std::uint64_t best_updates_ = 0;
  std::uint64_t restarts_ = 0;
  std::uint64_t threshold_pruned_moves_ = 0;
};
// END LIBRARY: time-based-simulated-annealing.hpp
// BEGIN LIBRARY: scope-profiler.hpp
#include <chrono>
#include <cstdint>
#include <ostream>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/scope-profiler.hpp
// 着想: https://github.com/asi1024/MarathonLibrary/blob/13fe8241ac04fcdcfb8da6e84b050bd4b23ad3a9/snippets/profiler.h

// 処理別の回数と合計時間を測る。-DAHC_ENABLE_PROFILING を付けた時だけ有効。
// TODO: ScopeProfiler evaluation("evaluate"); のように処理名を付ける。
// TODO: 測りたいブロックの先頭へ auto guard = evaluation.measure(); と書く。
// TODO: 終了時に evaluation.report(cerr); を呼ぶ。stdoutへは出さない。
// 無効ビルドでは時計を読まない・記録しない・出力しない。最適化時は空の処理になる。
// 機種別のCPU周波数は不要。ネストした区間は内側の時間も含む（exclusive時間ではない）。
// 同じProfilerを複数threadから使わない。ProfilerはGuardより長生きさせる。
struct ScopeProfiler {
#ifdef AHC_ENABLE_PROFILING
  const char* name; // 文字列リテラルなど、このProfilerより長生きする名前を渡す。
  std::uint64_t count = 0;
  double total_ms = 0.0;
  explicit ScopeProfiler(const char* label) : name(label) {}
#else
  explicit ScopeProfiler(const char*) {}
#endif

  struct Guard {
#ifdef AHC_ENABLE_PROFILING
    ScopeProfiler& owner;
    std::chrono::steady_clock::time_point started;
    explicit Guard(ScopeProfiler& profiler)
        : owner(profiler), started(std::chrono::steady_clock::now()) {}
#else
    explicit Guard(ScopeProfiler&) {}
#endif
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
    ~Guard() {
#ifdef AHC_ENABLE_PROFILING
      owner.total_ms += std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - started).count();
      ++owner.count;
#endif
    }
  };

  Guard measure() { return Guard(*this); }
  std::uint64_t calls() const {
#ifdef AHC_ENABLE_PROFILING
    return count;
#else
    return 0;
#endif
  }
  double elapsed_ms() const {
#ifdef AHC_ENABLE_PROFILING
    return total_ms;
#else
    return 0.0;
#endif
  }
  void report(std::ostream& output) const {
#ifdef AHC_ENABLE_PROFILING
    output << name << ": calls=" << count << " total_ms=" << total_ms << '\n';
#else
    (void)output;
#endif
  }
};
// END LIBRARY: scope-profiler.hpp
// Pre-contest public solver source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/examples/search/ahc001_region_sa.cpp
// Official problem: https://atcoder.jp/contests/ahc001/tasks/ahc001_a
// Ideas: https://hakomof.hatenablog.com/entry/2021/03/14/202411
//        https://blog.terry-u16.net/entry/ahc001

#ifndef AHC001_TIME_LIMIT_MS
#define AHC001_TIME_LIMIT_MS 4750
#endif
#ifndef AHC001_ITERATIONS
#define AHC001_ITERATIONS 0
#endif
#ifndef AHC001_THRESHOLD_TABLE_SIZE
#define AHC001_THRESHOLD_TABLE_SIZE 0
#endif
#ifndef AHC001_PRECOMPUTE_THRESHOLD
#define AHC001_PRECOMPUTE_THRESHOLD (AHC001_THRESHOLD_TABLE_SIZE > 0)
#endif
#ifndef AHC001_SCORE_EARLY_STOP
#define AHC001_SCORE_EARLY_STOP 1
#endif
using Rect = AxisAlignedRectangle<int>;

// TODO(AHC001): 固定入力、初期解の再帰分割、領域内の最終サイズ選択。
struct Request {
  int x;
  int y;
  long long desired_area;
};

struct SplitCandidate {
  double error;
  int axis;
  int split_count;
  int cut;
};

double satisfaction(long long desired, long long actual) {
  const double ratio =
      static_cast<double>(min(desired, actual)) / static_cast<double>(max(desired, actual));
  return 1.0 - (1.0 - ratio) * (1.0 - ratio);
}

// A quick approximation used while comparing many recursive partitions.
long long quick_best_area(int width, int height, long long desired) {
  if (width > height) swap(width, height);
  long long best = 1;

  auto try_width = [&](int w) {
    if (w < 1 || w > width) return;
    const long long floor_height = min<long long>(height, desired / w);
    if (floor_height >= 1) {
      const long long area = 1LL * w * floor_height;
      if (abs(area - desired) < abs(best - desired)) best = area;
    }
    const long long ceil_height = min<long long>(height, (desired + w - 1) / w);
    if (ceil_height >= 1) {
      const long long area = 1LL * w * ceil_height;
      if (abs(area - desired) < abs(best - desired)) best = area;
    }
  };

  try_width(1);
  try_width(width);
  try_width(static_cast<int>(sqrt(static_cast<double>(desired))));
  try_width(static_cast<int>(desired / max(1, height)));
  try_width(static_cast<int>((desired + height - 1) / max(1, height)));
  for (int sample = 1; sample <= 24; ++sample) {
    try_width(1 + (width - 1) * sample / 24);
  }
  return best;
}

long long exact_best_area(int width, int height, long long desired,
                          int& best_width, int& best_height) {
  bool swapped = false;
  if (width > height) {
    swap(width, height);
    swapped = true;
  }

  long long best_area = 1;
  int answer_width = 1;
  int answer_height = 1;
  for (int w = 1; w <= width; ++w) {
    const long long floor_height = min<long long>(height, desired / w);
    const long long ceil_height = min<long long>(height, (desired + w - 1) / w);
    for (long long h : {floor_height, ceil_height}) {
      if (h < 1) continue;
      const long long area = 1LL * w * h;
      // 面積差でなく満足度を最大化する。積は入力制約から1e16以下。
      if (min(area, desired) * max(best_area, desired) >
          min(best_area, desired) * max(area, desired)) {
        best_area = area;
        answer_width = w;
        answer_height = static_cast<int>(h);
      }
    }
  }

  if (swapped) swap(answer_width, answer_height);
  best_width = answer_width;
  best_height = answer_height;
  return best_area;
}


vector<Rect> make_initial_regions(const vector<Request>& requests,
                                  double limit_ms, uint64_t input_hash) {
  const int n = static_cast<int>(requests.size());
  Random random(input_hash);
  BatchedTimer timer(limit_ms, 1);
  vector<AxisAlignedRectangle<int>> best_regions(n);
  double best_proxy_score = -1.0;
  int builds = 0;

  while (!timer.is_over()) {
    vector<AxisAlignedRectangle<int>> regions(n);

    function<void(vector<int>&, AxisAlignedRectangle<int>)> divide =
        [&](vector<int>& indices, AxisAlignedRectangle<int> space) {
          if (indices.size() == 1) {
            regions[indices[0]] = space;
            return;
          }

          vector<int> by_x = indices;
          vector<int> by_y = indices;
          sort(by_x.begin(), by_x.end(), [&](int a, int b) {
            return requests[a].x < requests[b].x;
          });
          sort(by_y.begin(), by_y.end(), [&](int a, int b) {
            return requests[a].y < requests[b].y;
          });

          long long total_desired = 0;
          for (int index : indices) total_desired += requests[index].desired_area;

          vector<SplitCandidate> candidates;
          candidates.reserve(indices.size() * 2);
          for (int axis = 0; axis < 2; ++axis) {
            const vector<int>& order = axis == 0 ? by_x : by_y;
            const int low = axis == 0 ? space.left : space.bottom;
            const int high = axis == 0 ? space.right : space.top;
            const int other_length = axis == 0 ? space.height() : space.width();
            long long left_desired = 0;

            for (int count = 1; count < static_cast<int>(order.size()); ++count) {
              left_desired += requests[order[count - 1]].desired_area;
              const int previous_coordinate =
                  axis == 0 ? requests[order[count - 1]].x
                            : requests[order[count - 1]].y;
              const int next_coordinate =
                  axis == 0 ? requests[order[count]].x
                            : requests[order[count]].y;
              if (previous_coordinate == next_coordinate) continue;

              const int minimum_cut = previous_coordinate + 1;
              const int maximum_cut = next_coordinate;
              const long double fraction =
                  static_cast<long double>(left_desired) / total_desired;
              int cut = static_cast<int>(llround(low + (high - low) * fraction));
              cut = clamp(cut, minimum_cut, maximum_cut);

              if (builds > 0 && minimum_cut < maximum_cut &&
                  random.next_int(0, 4) == 0) {
                const int pull = random.next_int(0, 2) == 0
                                     ? minimum_cut
                                     : maximum_cut;
                cut = (3 * cut + pull) / 4;
                cut = clamp(cut, minimum_cut, maximum_cut);
              }

              const long long parent_area = space.area();
              const long long left_area = 1LL * (cut - low) * other_length;
              const long double target_area = parent_area * fraction;
              const double allocation_error =
                  static_cast<double>(abs(static_cast<long double>(left_area) - target_area) /
                  max<long double>(1.0L, parent_area));
              const double depth_error =
                  0.002 * abs(2 * count - static_cast<int>(order.size())) /
                  static_cast<double>(order.size());
              candidates.push_back(
                  {allocation_error + depth_error, axis, count, cut});
            }
          }

          assert(!candidates.empty());
          const int keep = min(10, static_cast<int>(candidates.size()));
          nth_element(candidates.begin(), candidates.begin() + keep - 1,
                      candidates.end(), [](const auto& a, const auto& b) {
                        return a.error < b.error;
                      });
          sort(candidates.begin(), candidates.begin() + keep,
               [](const auto& a, const auto& b) { return a.error < b.error; });

          int chosen_rank = 0;
          if (builds > 0) {
            const int roll = random.next_int(0, 100);
            if (roll >= 58) chosen_rank = min(1, keep - 1);
            if (roll >= 80) chosen_rank = min(2, keep - 1);
            if (roll >= 91) chosen_rank = min(4, keep - 1);
            if (roll >= 97) chosen_rank = random.next_int(0, keep);
          }
          const SplitCandidate chosen = candidates[chosen_rank];
          const vector<int>& order = chosen.axis == 0 ? by_x : by_y;
          vector<int> first(order.begin(), order.begin() + chosen.split_count);
          vector<int> second(order.begin() + chosen.split_count, order.end());

          auto first_space = space;
          auto second_space = space;
          if (chosen.axis == 0) {
            first_space.right = chosen.cut;
            second_space.left = chosen.cut;
          } else {
            first_space.top = chosen.cut;
            second_space.bottom = chosen.cut;
          }
          divide(first, first_space);
          divide(second, second_space);
        };

    vector<int> all(n);
    iota(all.begin(), all.end(), 0);
    divide(all, {0, 0, 10000, 10000});
    ++builds;

    double proxy_score = 0.0;
    for (int i = 0; i < n; ++i) {
      const long long area = quick_best_area(
          max(1, regions[i].width()), max(1, regions[i].height()),
          requests[i].desired_area);
      proxy_score += satisfaction(requests[i].desired_area, area);
    }
    if (proxy_score > best_proxy_score) {
      best_proxy_score = proxy_score;
      best_regions = regions;
    }
  }


  return best_regions;
}

// ============================================================================
// TODO(AHC001): ここから問題固有のState / Moveと近傍。Runner本体は編集しない。
// 「担当領域」は要求面積より大きくてもよい。出力時に内部で最適サイズへ縮める。
// ============================================================================
struct RegionProblem {
  using Score = double; // 大きいほど良い、各領域の満足度の合計。
  struct State {
    vector<Rect> regions; // TODO: 点を含む非重複の担当領域。
    vector<double> quality; // TODO: 各領域の代理評価値をcache。
  };
  struct Move {
    int kind = 0; // 0:境界を押し動かす、1:1領域再配置、2:2領域再構築。
    int index = 0, side = 0, coordinate = 0, other = 0;
    uint64_t salt = 0;
  };
  struct Change { int index; Rect region; double quality; };
  const vector<Request>& requests;
  vector<vector<int>> neighbors;
  vector<Change> pending; // 仮変更。Stateではないので、不採用で現在解は壊れない。
  vector<Rect> obstacles; // 再配置用scratchを再利用する。
  // TODO: 【診断時だけ】-DAHC_ENABLE_PROFILINGで処理別時間を測る。
  ScopeProfiler evaluation_profile{"evaluation (including rebuild)"};
  ScopeProfiler rebuild_profile{"rebuild"};

  explicit RegionProblem(const vector<Request>& input) : requests(input) {
    const int n = static_cast<int>(requests.size());
    pending.reserve(input.size());
    obstacles.reserve(input.size());
    neighbors.resize(input.size());
    for (int i = 0; i < n; ++i) {
      vector<pair<int,int>> distances;
      for (int j = 0; j < n; ++j) if (i != j) {
        distances.emplace_back(abs(input[i].x - input[j].x) +
                               abs(input[i].y - input[j].y), j);
      }
      sort(distances.begin(), distances.end());
      for (int k = 0; k < min(10, n - 1); ++k) neighbors[i].push_back(distances[k].second);
    }
  }
  // TODO: 最終出力用の広告ではなく、担当領域の容量を評価する。
  // 容量が要求以上なら1。整数の縦横サイズによる丸めは最後に厳密化する。
  double quality(int id, const Rect& rectangle) const {
    const double ratio = min(1.0, static_cast<double>(rectangle.area()) /
                                  static_cast<double>(requests[id].desired_area));
    return 1.0 - (1.0 - ratio) * (1.0 - ratio);
  }
  State make_state(vector<Rect> regions) const {
    State state{std::move(regions), {}};
    state.quality.resize(requests.size());
    for (int i = 0; i < static_cast<int>(requests.size()); ++i) {
      state.quality[i] = quality(i, state.regions[i]);
    }
    return state;
  }
  double score(const State& state) const {
    return accumulate(state.quality.begin(), state.quality.end(), 0.0);
  }
  // TODO: 近傍の材料だけを返す。現在Stateは変更しない。
  optional<Move> propose_move(const State& state, mt19937_64& random, double progress) const {
    const int n = static_cast<int>(requests.size());
    const auto pick = [&](int count) { return static_cast<int>(random() % static_cast<uint64_t>(count)); };
    int id = pick(n);
    if (pick(2) == 0) { const int j = pick(n); if (state.quality[j] < state.quality[id]) id = j; }
    Move move;
    move.index = id;
    const int kind = pick(100);
    move.kind = kind < 90 ? 0 : kind < 93 ? 1 : 2;
    move.side = pick(4);
    move.salt = random();
    if (move.kind == 2) {
      if (neighbors[id].empty()) return nullopt;
      move.other = neighbors[id][pick(static_cast<int>(neighbors[id].size()))];
    } else if (move.kind == 0) {
      // TODO: 初期は大きく、終盤は小さく境界を動かす。
      const int range = 2 + static_cast<int>(512 * (1 - progress) * (1 - progress));
      int amount = 1 + pick(range);
      if (pick(4) == 0) amount = -amount;
      const auto& r = state.regions[id];
      const auto& p = requests[id];
      if (move.side == 0) move.coordinate = clamp(r.left - amount, 0, p.x);
      if (move.side == 1) move.coordinate = clamp(r.right + amount, p.x + 1, 10000);
      if (move.side == 2) move.coordinate = clamp(r.bottom - amount, 0, p.y);
      if (move.side == 3) move.coordinate = clamp(r.top + amount, p.y + 1, 10000);
    }
    return move;
  }
  bool legal(int id, const Rect& r) const {
    return r.is_valid() && 0 <= r.left && r.right <= 10000 &&
           0 <= r.bottom && r.top <= 10000 && r.contains(requests[id].x, requests[id].y);
  }
  // TODO: thresholdより良くならないと分かったらnullopt。それ以外は正確な「差分」。
  // 仮変更をpendingへ保存し、採用時だけapply_moveで確定する。
  optional<Score> evaluate_move_with_threshold(const State& state, const Move& move,
                                               double threshold) {
    auto evaluation_guard = evaluation_profile.measure();
    pending.clear();
    const int id = move.index, n = static_cast<int>(requests.size());
    if (move.kind == 0) {
      auto next = state.regions[id];
      if (move.side == 0) next.left = move.coordinate;
      if (move.side == 1) next.right = move.coordinate;
      if (move.side == 2) next.bottom = move.coordinate;
      if (move.side == 3) next.top = move.coordinate;
      double delta = quality(id, next) - state.quality[id];
      // 他領域は縮むだけなので、ここから差分は増えない。これは厳密な枝刈り。
      if (delta <= threshold) return nullopt;
      pending.push_back({id, next, quality(id, next)});
      for (int j = 0; j < n; ++j) if (j != id && next.overlaps(state.regions[j])) {
        auto other = state.regions[j];
        if (move.side == 0) other.right = next.left;
        if (move.side == 1) other.left = next.right;
        if (move.side == 2) other.top = next.bottom;
        if (move.side == 3) other.bottom = next.top;
        if (!legal(j, other)) return nullopt;
        const double q = quality(j, other);
        delta += q - state.quality[j];
        if (delta <= threshold) return nullopt;
        pending.push_back({j, other, q});
      }
      return delta;
    }
    auto rebuild_guard = rebuild_profile.measure();
    obstacles.clear();
    for (int j = 0; j < n; ++j) {
      if (j != id && (move.kind == 1 || j != move.other)) obstacles.push_back(state.regions[j]);
    }
    if (move.kind == 1) {
      const Rect next = largest_empty_rectangle(Rect{0,0,10000,10000},
                                                requests[id].x, requests[id].y, obstacles);
      const double q = quality(id, next);
      pending.push_back({id, next, q});
      return q - state.quality[id];
    }
    // TODO: 近い2領域を外し、点を分離する境界で空間を分け、各側で再配置する。
    // 3本の境界を試す近似探索。全ての2領域配置を厳密に最適化するわけではない。
    int first = id, second = move.other;
    int axis = move.side % 2;
    if ((axis == 0 ? requests[first].x == requests[second].x :
                     requests[first].y == requests[second].y)) axis ^= 1;
    const auto coordinate = [&](int i) { return axis == 0 ? requests[i].x : requests[i].y; };
    if (coordinate(first) > coordinate(second)) swap(first, second);
    const int low = coordinate(first) + 1, high = coordinate(second);
    const int random_cut = low + static_cast<int>(move.salt % static_cast<uint64_t>(high - low + 1));
    const array<int,3> cuts{{low, high, random_cut}};
    double best_delta = -1e100;
    Rect best_a{}, best_b{};
    for (int cut : cuts) {
      Rect left{0,0,10000,10000}, right = left;
      if (axis == 0) { left.right = cut; right.left = cut; }
      else { left.top = cut; right.bottom = cut; }
      const auto a = largest_empty_rectangle(left, requests[first].x, requests[first].y, obstacles);
      const auto b = largest_empty_rectangle(right, requests[second].x, requests[second].y, obstacles);
      const double delta = quality(first, a) + quality(second, b) -
                           state.quality[first] - state.quality[second];
      if (delta > best_delta) { best_delta = delta; best_a = a; best_b = b; }
    }
    pending.push_back({first, best_a, quality(first, best_a)});
    pending.push_back({second, best_b, quality(second, best_b)});
    return best_delta;
  }
  Score evaluate_move(const State& state, const Move& move) {
    // 途中打ち切りOFFでは全差分を計算。不合法手は必ず棄却される-infを返す。
    return evaluate_move_with_threshold(state, move, -numeric_limits<double>::infinity())
        .value_or(-numeric_limits<double>::infinity());
  }
  // TODO: 採用された仮変更だけをStateへ反映する。不採用時は呼ばれない。
  void apply_move(State& state, Move&) {
    for (const auto& c : pending) { state.regions[c.index] = c.region; state.quality[c.index] = c.quality; }
  }
  void validate(const State& state) const {
    for (int i = 0; i < static_cast<int>(requests.size()); ++i) {
      if (!legal(i, state.regions[i]) || abs(state.quality[i] - quality(i,state.regions[i])) > 1e-12)
        throw runtime_error("invalid region/cache");
      for (int j = 0; j < i; ++j) if (state.regions[i].overlaps(state.regions[j]))
        throw runtime_error("overlapping regions");
    }
  }
};

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);
  const auto started = chrono::steady_clock::now();
  const auto elapsed = [&]() { return chrono::duration<double,milli>(chrono::steady_clock::now()-started).count(); };
  int n;
  if (!(cin >> n) || n < 1 || n > 200) throw runtime_error("invalid request count");
  vector<Request> requests(static_cast<size_t>(n));
  uint64_t seed = 1;
  for (auto& r : requests) {
    cin >> r.x >> r.y >> r.desired_area;
    seed = seed * 1000003 + static_cast<uint64_t>(r.x + 10000 * r.y) + static_cast<uint64_t>(r.desired_area);
  }
  if (!cin) throw runtime_error("incomplete input");
  RegionProblem problem(requests);
  // TODO: 初期解にも全体時間を使う。固定反復の検査では初期構築も短くする。
  auto state = problem.make_state(make_initial_regions(requests,
      AHC001_ITERATIONS > 0 ? 1.0 : min(180.0, AHC001_TIME_LIMIT_MS * 0.1), seed));
  uint64_t iterations = 0, pruned = 0;
  for (int phase = 0; phase < 2; ++phase) {
    const double remaining = AHC001_TIME_LIMIT_MS - elapsed();
    if (remaining < 10 && AHC001_ITERATIONS == 0) break;
    const double budget = AHC001_ITERATIONS > 0 ? 1e9 : remaining * (phase == 0 ? 0.84 : 0.98);
    // TODO: 代理評価の和に対する温度。後半はほぼ山登りで仕上げる。
    TimeBasedAnnealingRunner<RegionProblem> sa(problem, state, problem.score(state),
        budget, phase == 0 ? 0.015 : 1e-8, phase == 0 ? 1e-6 : 1e-8,
        seed + static_cast<uint64_t>(phase), 4);
    // TODO: 前計算とスコア途中打ち切りは独立。どちらもON/OFFできる。
    sa.annealing().set_threshold_precomputation(AHC001_PRECOMPUTE_THRESHOLD != 0,
        AHC001_THRESHOLD_TABLE_SIZE > 0 ? AHC001_THRESHOLD_TABLE_SIZE : 4096);
    if (AHC001_ITERATIONS > 0) {
      for (int i = 0; i < AHC001_ITERATIONS; ++i)
        sa.step_with_threshold(AHC001_SCORE_EARLY_STOP != 0);
    } else sa.run_with_threshold(AHC001_SCORE_EARLY_STOP != 0);
    state = sa.best_state();
    iterations += sa.iterations();
    pruned += sa.threshold_pruned_moves();
  }
  problem.validate(state);
  // TODO: 担当領域の中で、公式得点が最大になる整数サイズへ縮めて出力する。
  for (int i = 0; i < n; ++i) {
    const auto& region = state.regions[i];
    int w = 1, h = 1;
    exact_best_area(region.width(), region.height(), requests[i].desired_area, w, h);
    const int x = clamp(requests[i].x - w / 2, region.left, region.right - w);
    const int y = clamp(requests[i].y - h / 2, region.bottom, region.top - h);
    cout << x << ' ' << y << ' ' << x+w << ' ' << y+h << '\n';
  }
  cerr << "iterations=" << iterations << " pruned=" << pruned << " elapsed_ms=" << elapsed() << '\n';
  problem.evaluation_profile.report(cerr);
  problem.rebuild_profile.report(cerr);
  return 0;
}
