// GCC提出用の最適化。診断時は -DAHC_DISABLE_GCC_OPTIMIZE で無効化。
#if defined(__GNUC__) && !defined(__clang__) && !defined(AHC_DISABLE_GCC_OPTIMIZE)
#pragma GCC optimize("O3")
#endif

#include <bits/stdc++.h>
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
  // TODO: evaluate_moveへlowerを渡し、最後はaccept()で確認する。
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
// 空関数を配置済みの雛形: template/search/local-search/basic.cpp
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
//   TODO: 【問題ごと】評価関数はこの1個だけ。近傍による改善量を差分計算する。
//   optional<Score> evaluate_move(
//       const State&, const Move&, double acceptance_threshold)
//     -> 正なら良化、負なら悪化となる正確な改善量を返す。
//     -> 【任意】最終改善量の上限 <= 閾値と証明できたらnulloptで途中終了。
//     -> 打ち切りが難しい問題では閾値を無視して最後まで計算してよい。
//     -> 打ち切りOFF時の閾値は-inf。不合法手は設定に関係なくnulloptでよい。
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
  // 閾値は-infで全評価。従来のaccept()の採否・乱数消費を維持する。
  bool step() {
    if (annealing_.is_over()) return false;
    ++iterations_;
    std::optional<Move> move = problem_.propose_move(
        static_cast<const State&>(current_state_),
        move_engine_,
        annealing_.cached_progress());
    if (!move.has_value()) return true;

    ++valid_moves_;
    const std::optional<Score> improvement = problem_.evaluate_move(
        static_cast<const State&>(current_state_), *move,
        -std::numeric_limits<double>::infinity());
    if (!improvement || !annealing_.accept(*improvement)) return true;

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

  // 差分評価が重く、途中で採用不能と証明できる問題向け。
  // Problem::evaluate_moveは、improvementがthresholdを
  // 超えないと証明できた時だけnulloptを返す。分からない場合は最後まで
  // 計算し、正確なimprovementを返せば通常の焼きなましと同じ分布になる。
  // 区間表を有効にした場合は実際の閾値以下の下限を渡し、最終採否を別途確認する。
  bool step_with_threshold() {
    return step_threshold_impl<true>();
  }

  // 呼ぶ評価関数は常にevaluate_moveの1個だけ。
  // true: 安全な受理閾値を渡す。false: -infを渡して全評価。
  // 閾値の抽選・最終採否は同じ。評価側は閾値を無視してもよい。
  bool step_with_threshold(bool enable_score_early_stop) {
    return enable_score_early_stop ? step_threshold_impl<true>()
                                   : step_threshold_impl<false>();
  }

  // 山登り。Problemは焼きなましと同じで、改善量>0の手だけ採用する。
  // 同点・悪化・非有限値は棄却。evaluate_moveへ渡す閾値は0。
  // 温度による採否や受理乱数は使わない。時計・近傍・最良解管理は共通。
  bool step_hill_climbing() {
    if (annealing_.is_over()) return false;
    ++iterations_;
    std::optional<Move> move = problem_.propose_move(
        static_cast<const State&>(current_state_), move_engine_,
        annealing_.cached_progress());
    if (!move) return true;
    ++valid_moves_;
    const std::optional<Score> improvement = problem_.evaluate_move(
        static_cast<const State&>(current_state_), *move, 0.0);
    if (!improvement) { ++threshold_pruned_moves_; return true; }
    if (!std::isfinite(static_cast<long double>(*improvement)) ||
        !(*improvement > Score{0})) return true;
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
    const double evaluation_threshold = EnableScoreEarlyStop
        ? threshold.lower : -std::numeric_limits<double>::infinity();
    const std::optional<Score> improvement = problem_.evaluate_move(
        static_cast<const State&>(current_state_), *move, evaluation_threshold);
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

  std::uint64_t run_hill_climbing() {
    while (step_hill_climbing()) {}
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
  // 閾値方式でevaluate_moveがnulloptを返した件数。不合法手の棄却も含む。
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
// BEGIN LIBRARY: aho-corasick.hpp
#include <array>
#include <cassert>
#include <queue>
#include <stdexcept>
#include <vector>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/aho-corasick.hpp

// 複数パターンをまとめて探す。文字を0..Alphabet-1の整数へ変換して使う。
// 例: AhoCorasick<3> ac; ac.add({0,1}, 7); ac.build();
// int state=0; for (int c : {0,1,0}) {
//   state=ac.advance(state,c); for(int id:ac.matches(state)) { /* id=7が一致 */ }
// }
// addはbuild前だけ。空パターンは不可。同じパターン・IDの重複登録は許可し、
// matchesにも重複を残す。全登録完了後buildを1回呼ぶ（再呼び出しは無害）。
// 検索O(入力長+一致数)、build O(節点数*Alphabet+展開した出力ID数)。
// suffixの出力IDを各節点へ複製するため、多数の包含語ではメモリに注意。
template <int Alphabet = 26>
struct AhoCorasick {
  static_assert(Alphabet > 0, "positive alphabet required");
  struct Node {
    std::array<int, Alphabet> next;
    int failure = 0;
    std::vector<int> output;
    Node() { next.fill(-1); }
  };
  std::vector<Node> nodes{1};
  bool built = false;

  void add(const std::vector<int>& word, int id) {
    if (built || word.empty()) throw std::invalid_argument("add nonempty patterns before build");
    for (int c : word) {
      if (c < 0 || c >= Alphabet) throw std::out_of_range("symbol outside alphabet");
    }
    int state = 0;
    for (int c : word) {
      if (nodes[state].next[c] < 0) {
        const int child = static_cast<int>(nodes.size());
        nodes[state].next[c] = child;
        nodes.emplace_back();
      }
      state = nodes[state].next[c];
    }
    nodes[state].output.push_back(id);
  }

  void build() {
    if (built) return;
    std::queue<int> queue;
    for (int c = 0; c < Alphabet; ++c) {
      int& child = nodes[0].next[c];
      if (child < 0) child = 0;
      else queue.push(child);
    }
    while (!queue.empty()) {
      const int state = queue.front();
      queue.pop();
      const int failure = nodes[state].failure;
      const auto& inherited = nodes[failure].output;
      nodes[state].output.insert(nodes[state].output.end(), inherited.begin(), inherited.end());
      for (int c = 0; c < Alphabet; ++c) {
        int& child = nodes[state].next[c];
        if (child < 0) child = nodes[failure].next[c];
        else {
          nodes[child].failure = nodes[failure].next[c];
          queue.push(child);
        }
      }
    }
    built = true;
  }

  int advance(int state, int symbol) const {
    assert(built && state >= 0 && state < static_cast<int>(nodes.size()));
    assert(symbol >= 0 && symbol < Alphabet);
    return nodes[state].next[symbol];
  }
  const std::vector<int>& matches(int state) const {
    assert(built && state >= 0 && state < static_cast<int>(nodes.size()));
    return nodes[state].output;
  }
};
// END LIBRARY: aho-corasick.hpp
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/examples/search/ahc004_genome_sa.cpp
// Problem / scoring: https://atcoder.jp/contests/ahc004/tasks/ahc004_a
using namespace std;

// library/timer.hpp
struct Timer {
  chrono::steady_clock::time_point start;

  Timer() : start(chrono::steady_clock::now()) {}

  double elapsed_ms() const {
    const auto now = chrono::steady_clock::now();
    return chrono::duration<double, milli>(now - start).count();
  }
};

// library/random.hpp
struct Random {
  mt19937_64 engine;

  explicit Random(uint64_t seed = 0) : engine(seed) {}

  uint64_t next_u64() { return engine(); }

  template <class Int>
  Int next_int(Int left, Int right) {
    assert(left < right);
    uniform_int_distribution<Int> distribution(left, right - 1);
    return distribution(engine);
  }

  double next_double() {
    return uniform_real_distribution<double>(0.0, 1.0)(engine);
  }
};

struct Target {
  string text;
  int frequency = 0;
  int covered_weight = 0;
  int overlap_strength = 0;
  bool maximal = true;
};

// library/sequence-overlap.hpp
template <class Sequence>
int suffix_prefix_overlap(const Sequence& first, const Sequence& second) {
  const int limit = static_cast<int>(min(first.size(), second.size()));
  for (int length = limit; length >= 1; --length) {
    bool same = true;
    for (int index = 0; index < length; ++index) {
      if (first[first.size() - length + index] != second[index]) {
        same = false;
        break;
      }
    }
    if (same) return length;
  }
  return 0;
}


// ===== 問題ごとに書く部分（local-search/basic.cppと同じインターフェース） =====
struct GenomeProblem {
  using Score = double;
  using Board = vector<string>;
  // TODO: 現在解と、差分評価で参照するcacheを書く。
  // line_hitsには同じ語の複数出現も残す。countが0になる時だけ失点する。
  struct State {
    Board board;
    vector<vector<int>> line_hits;
    vector<int> count;
    vector<uint64_t> line_code, line_empty;
    int covered = 0, empty = 0;
  };
  // TODO: 1近傍で変更する場所と変更後の値を書く（評価時にはStateを触らない）。
  struct Move { vector<pair<int, char>> changes; };
  int n, m;
  vector<Target> targets;
  AhoCorasick<8> matcher;
  int max_length = 0;
  vector<vector<int>> line_cells;
  // 採用判定前の一時領域。最良Stateにコピーする必要はないのでProblemに置く。
  Board scratch;
  vector<int> delta, touched;
  vector<unsigned char> touched_flag;
  vector<int> changed_lines;
  vector<vector<int>> next_hits;
  int next_covered = 0, next_empty = 0;

  GenomeProblem(int n_, int m_, const vector<Target>& targets_)
      : n(n_), m(m_), targets(targets_), line_cells(2*n),
        delta(targets.size()), touched_flag(targets.size()), next_hits(2*n) {
    for (int id = 0; id < (int)targets.size(); ++id) {
      vector<int> symbols;
      for (char c : targets[id].text) symbols.push_back(c - 'A');
      matcher.add(symbols, id);
      max_length = max(max_length, (int)symbols.size());
    }
    matcher.build();
    for (int line = 0; line < 2*n; ++line) {
      for (int p = 0; p < n + max_length - 1; ++p) {
        line_cells[line].push_back(line < n ? line*n+p%n : (p%n)*n+line-n);
      }
      next_hits[line].reserve(n * max_length);
    }
  }

  // 20文字+最大長-1文字だけ読む。開始位置0..19にある一致だけを数える。
  void scan_line(const Board& board, int line, vector<int>& hits) const {
    hits.clear();
    int state = 0;
    for (int p = 0; p < (int)line_cells[line].size(); ++p) {
      const int cell = line_cells[line][p];
      const char c = board[cell/n][cell%n];
      if (c == '.') { state = 0; continue; }
      state = matcher.advance(state, c - 'A');
      for (int id : matcher.matches(state)) {
        if (p + 1 - (int)targets[id].text.size() < n) hits.push_back(id);
      }
    }
  }

  void encode_line(State& state, int line) const {
    uint64_t code = 0, empty = 0;
    for (int p = 0; p < n; ++p) {
      int cell = line_cells[line][p];
      char c = state.board[cell/n][cell%n];
      if (c == '.') empty |= 1ULL << (3*p);
      else code |= uint64_t(c-'A') << (3*p);
    }
    state.line_code[line] = code;
    state.line_empty[line] = empty;
  }

  // 3bit/文字。XOR後の各3bitを1bitへ畳み、popcountで一致数を数える。
  // 空白は別bit列に持つので、'A'と取り違えない。
  int placement_quality(const State& state, int line, int start,
                        uint64_t word, int length) const {
    auto rotate = [&](uint64_t bits) {
      return (bits >> (3*start)) | (bits << (3*(n-start)));
    };
    const uint64_t low_bits = ((1ULL << (3*length))-1)/7;
    const uint64_t empty = rotate(state.line_empty[line]) & low_bits;
    const uint64_t diff = rotate(state.line_code[line]) ^ word;
    const uint64_t mismatch = (diff | (diff>>1) | (diff>>2) | empty) & low_bits;
    const int same = length - __builtin_popcountll(mismatch);
    return 1250*same + 330*__builtin_popcountll(empty) - 250*length;
  }

  State make_state(Board board) const {
    State state;
    state.board = std::move(board);
    state.line_hits.resize(2*n);
    state.line_code.resize(2*n);
    state.line_empty.resize(2*n);
    state.count.assign(targets.size(), 0);
    for (int line = 0; line < 2*n; ++line) {
      encode_line(state, line);
      scan_line(state.board, line, state.line_hits[line]);
      for (int id : state.line_hits[line]) ++state.count[id];
    }
    for (int id = 0; id < (int)targets.size(); ++id)
      if (state.count[id]) state.covered += targets[id].frequency;
    for (const string& row : state.board) state.empty += count(row.begin(), row.end(), '.');
    return state;
  }

  Score score(int covered, int empty) const {
    // 全語を含む時だけ空白bonus。公式点の丸め前をm/1e8倍した尺度。
    return covered < m ? covered : double(m) * (2*n*n) / (2*n*n-empty);
  }
  Score score(const State& state) const { return score(state.covered, state.empty); }

  // TODO: 問題に合う近傍を1つ生成する。nulloptなら今回の試行はスキップ。
  // ここでは未収録語を重なりのよい場所へ上書きする。
  optional<Move> propose_move(const State& state, mt19937_64& rng, double) {
    auto random_int = [&](int upper) {
      return uniform_int_distribution<int>(0, upper-1)(rng);
    };
    if (state.covered == m) {
      const int cell = random_int(n*n);
      if (state.board[cell/n][cell%n] == '.') return nullopt;
      return Move{{{cell, '.'}}};
    }
    int chosen = -1;
    for (int trial = 0; trial < 32; ++trial) {
      int id = random_int((int)targets.size());
      if (!state.count[id]) { chosen = id; break; }
    }
    if (chosen < 0) {
      const int first = random_int((int)targets.size());
      for (int k = 0; k < (int)targets.size(); ++k) {
        int id = (first+k) % targets.size();
        if (!state.count[id]) { chosen = id; break; }
      }
    }
    if (chosen < 0) return nullopt;
    const string& text = targets[chosen].text;
    uint64_t word = 0;
    for (int k = 0; k < (int)text.size(); ++k) word |= uint64_t(text[k]-'A') << (3*k);
    int best_quality = INT_MIN, best_line = 0, best_start = 0, ties = 0;
    for (int trial = 0; trial < 2*n*n; ++trial) {
      const int line = trial/n, start = trial%n;
      const int quality = placement_quality(state, line, start, word, (int)text.size());
      // 同点を1/2で置換すると走査末尾に偏る。k個目は1/kで置換する。
      if (quality > best_quality) {
        best_quality = quality; best_line = line; best_start = start; ties = 1;
      } else if (quality == best_quality && random_int(++ties) == 0) {
        best_line = line; best_start = start;
      }
    }
    Move move;
    move.changes.reserve(text.size());
    for (int k = 0; k < (int)text.size(); ++k) {
      int cell = line_cells[best_line][best_start+k];
      if (state.board[cell/n][cell%n] != text[k]) move.changes.emplace_back(cell, text[k]);
    }
    if (move.changes.empty()) return nullopt;
    return move;
  }

  // TODO: 「変更後score - 変更前score」を返す。負の値もそのまま返してよい。
  // thresholdを超えないと証明できる場合だけnulloptで打ち切れる。
  // この版では上限判定を入れず、正確な差分を最後まで求める。
  optional<Score> evaluate_move(const State& state, const Move& move, double threshold) {
    (void)threshold;
    for (int id : touched) { delta[id] = 0; touched_flag[id] = 0; }
    touched.clear();
    scratch = state.board;
    next_empty = state.empty;
    uint64_t line_mask = 0;
    for (auto [cell, c] : move.changes) {
      next_empty += (c == '.') - (scratch[cell/n][cell%n] == '.');
      scratch[cell/n][cell%n] = c;
      line_mask |= (1ULL << (cell/n)) | (1ULL << (n+cell%n));
    }
    changed_lines.clear();
    auto add = [&](int id, int change) {
      if (!touched_flag[id]) { touched_flag[id] = 1; touched.push_back(id); }
      delta[id] += change;
    };
    while (line_mask) {
      int line = __builtin_ctzll(line_mask);
      line_mask &= line_mask - 1;
      changed_lines.push_back(line);
      for (int id : state.line_hits[line]) add(id, -1);
      scan_line(scratch, line, next_hits[line]);
      for (int id : next_hits[line]) add(id, 1);
    }
    next_covered = state.covered;
    for (int id : touched) {
      assert(state.count[id] + delta[id] >= 0);
      next_covered += targets[id].frequency *
          ((state.count[id] + delta[id] > 0) - (state.count[id] > 0));
    }
    return score(next_covered, next_empty) - score(state);
  }

  // TODO: 採用した手だけ反映する。直前のevaluate_moveのcacheを使う。
  // 不採用なら呼ばれないので、rollbackは不要。
  void apply_move(State& state, Move&) {
    state.board.swap(scratch);
    for (int line : changed_lines) {
      state.line_hits[line].swap(next_hits[line]);
      encode_line(state, line);
    }
    for (int id : touched) state.count[id] += delta[id];
    state.covered = next_covered;
    state.empty = next_empty;
  }
};
// ===== ここまでが問題依存。時計・温度・採否・最良解保存はRunnerへ任せる =====

#ifndef AHC004_TEST
int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  int n, m;
  cin >> n >> m;
  vector<string> input_strings(m);
  map<string, int> frequency;
  uint64_t input_hash = 1469598103934665603ULL;
  for (string& text : input_strings) {
    cin >> text;
    ++frequency[text];
    for (char letter : text) {
      input_hash ^= static_cast<unsigned char>(letter);
      input_hash *= 1099511628211ULL;
    }
  }
  Timer timer;
  constexpr double GREEDY_END_MS = 150.0;
  constexpr double SEARCH_END_MS = 2780.0;

  vector<Target> targets;
  targets.reserve(frequency.size());
  for (const auto& [text, count] : frequency) {
    targets.push_back({text, count});
  }

  const int target_count = static_cast<int>(targets.size());
  for (int i = 0; i < target_count; ++i) {
    for (int j = 0; j < target_count; ++j) {
      if (i == j) continue;
      if (targets[i].text.size() < targets[j].text.size() &&
          targets[j].text.find(targets[i].text) != string::npos) {
        targets[i].maximal = false;
      }
    }
  }

  vector<int> maximal_indices;
  for (int i = 0; i < target_count; ++i) {
    if (!targets[i].maximal) continue;
    maximal_indices.push_back(i);
    for (int j = 0; j < target_count; ++j) {
      if (targets[i].text.find(targets[j].text) != string::npos) {
        targets[i].covered_weight += targets[j].frequency;
      }
    }
  }

  for (int i : maximal_indices) {
    array<int, 3> largest{};
    for (int j : maximal_indices) {
      if (i == j) continue;
      const int overlap = max(
          suffix_prefix_overlap(targets[i].text, targets[j].text),
          suffix_prefix_overlap(targets[j].text, targets[i].text));
      if (overlap > largest[0]) {
        largest[0] = overlap;
        sort(largest.begin(), largest.end());
      }
    }
    targets[i].overlap_strength = largest[0] + largest[1] + largest[2];
  }

  using Board = vector<string>;

  const auto is_present = [&](const Board& board, const string& text) {
    const int length = static_cast<int>(text.size());
    for (int direction = 0; direction < 2; ++direction) {
      for (int line = 0; line < n; ++line) {
        for (int start = 0; start < n; ++start) {
          bool same = true;
          for (int offset = 0; offset < length; ++offset) {
            const int row = direction == 0 ? line : (start + offset) % n;
            const int column = direction == 0 ? (start + offset) % n : line;
            if (board[row][column] != text[offset]) {
              same = false;
              break;
            }
          }
          if (same) return true;
        }
      }
    }
    return false;
  };

  const auto count_score = [&](const Board& board) {
    int covered = 0;
    for (const Target& target : targets) {
      if (is_present(board, target.text)) covered += target.frequency;
    }
    int empty = 0;
    for (const string& row : board) {
      empty += count(row.begin(), row.end(), '.');
    }
    return pair<int, int>{covered, empty};
  };

  Random random(input_hash);
  Board best_board(n, string(n, '.'));
  pair<int, int> best_score{-1, -1};
  int attempt = 0;

  do {
    Board board(n, string(n, '.'));
    vector<int> order = maximal_indices;
    vector<uint64_t> priority(target_count);
    for (int index : order) {
      const uint64_t base =
          1000000ULL * targets[index].text.size() +
          5000ULL * targets[index].covered_weight +
          3000ULL * targets[index].overlap_strength;
      const uint64_t noise =
          attempt == 0 ? 0 : random.next_u64() % 2500000ULL;
      priority[index] = base + noise;
    }
    sort(order.begin(), order.end(), [&](int first, int second) {
      return priority[first] > priority[second];
    });

    vector<int> remaining;
    remaining.reserve(target_count);
    for (int i = 0; i < target_count; ++i) {
      if (!targets[i].maximal) remaining.push_back(i);
    }
    sort(remaining.begin(), remaining.end(), [&](int first, int second) {
      if (targets[first].frequency != targets[second].frequency) {
        return targets[first].frequency > targets[second].frequency;
      }
      return targets[first].text.size() > targets[second].text.size();
    });
    order.insert(order.end(), remaining.begin(), remaining.end());

    for (int target_index : order) {
      const string& text = targets[target_index].text;
      if (is_present(board, text)) continue;

      long long best_quality = numeric_limits<long long>::min();
      int best_direction = -1;
      int best_line = -1;
      int best_start = -1;
      int equal_candidates = 0;

      for (int direction = 0; direction < 2; ++direction) {
        for (int line = 0; line < n; ++line) {
          int line_filled = 0;
          for (int position = 0; position < n; ++position) {
            const int row = direction == 0 ? line : position;
            const int column = direction == 0 ? position : line;
            line_filled += board[row][column] != '.';
          }

          for (int start = 0; start < n; ++start) {
            bool compatible = true;
            int matching = 0;
            int new_cells = 0;
            for (int offset = 0; offset < static_cast<int>(text.size());
                 ++offset) {
              const int row = direction == 0 ? line : (start + offset) % n;
              const int column =
                  direction == 0 ? (start + offset) % n : line;
              const char current = board[row][column];
              if (current == '.') {
                ++new_cells;
              } else if (current == text[offset]) {
                ++matching;
              } else {
                compatible = false;
                break;
              }
            }
            if (!compatible) continue;

            long long quality = 1000000LL * matching - 10000LL * new_cells;
            if (matching == 0) {
              quality -= 100LL * line_filled;
            } else {
              quality += 10LL * line_filled;
            }

            if (quality > best_quality) {
              best_quality = quality;
              best_direction = direction;
              best_line = line;
              best_start = start;
              equal_candidates = 1;
            } else if (quality == best_quality) {
              ++equal_candidates;
              if (random.next_int(0, equal_candidates) == 0) {
                best_direction = direction;
                best_line = line;
                best_start = start;
              }
            }
          }
        }
      }

      if (best_direction == -1) continue;
      for (int offset = 0; offset < static_cast<int>(text.size()); ++offset) {
        const int row = best_direction == 0
                            ? best_line
                            : (best_start + offset) % n;
        const int column = best_direction == 0
                               ? (best_start + offset) % n
                               : best_line;
        board[row][column] = text[offset];
      }
    }

    const pair<int, int> score = count_score(board);
    if (score > best_score) {
      best_score = score;
      best_board = move(board);
    }
    ++attempt;
  } while (timer.elapsed_ms() < GREEDY_END_MS);

  GenomeProblem problem(n, m, targets);
  auto initial = problem.make_state(best_board);
  const double initial_score = problem.score(initial);
  const double remaining_ms = SEARCH_END_MS - timer.elapsed_ms();
  if (remaining_ms > 0) {
    TimeBasedAnnealingRunner<GenomeProblem> runner(
        problem, std::move(initial), initial_score, remaining_ms, 6.0, 0.03,
        input_hash, 32);
    runner.run();
    best_board = runner.best_state().board;
    cerr << "iterations=" << runner.iterations()
         << " covered=" << runner.best_state().covered << "/" << m << '\n';
  }
  for (const string& row : best_board) cout << row << '\n';
}
#endif
