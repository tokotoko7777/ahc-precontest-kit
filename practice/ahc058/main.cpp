#include <bits/stdc++.h>
// BEGIN LIBRARY: deterministic-rollout.hpp
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/deterministic-rollout.hpp

// 候補をそれぞれ終端まで仮実行し、評価が最良の1手を返す薄いRunner。
// 乱数で未来を作る問題には common-scenario-average.hpp を使い、
// 未来が現在状態とActionから全て決まる問題にはこちらを使う。
//
// 【使う人がmain.cpp側へ書く場所】
// 空関数を配置済みの雛形: template/search/deterministic-rollout.cpp
//
//   struct Problem {
//     using State = ...;   // TODO: 現在の実状態。
//     using Action = ...;  // TODO: 今比較する1手。
//     using Score = ...;   // TODO: 仮実行の評価値。int以外でもよい。
//
//     // TODO: 今比較したい合法Actionを全て返す。
//     // vectorでなくarrayやFixedVectorを返してもよい。
//     auto generate_actions(const State&) const;
//
//     // TODO: Actionを最初に選んだ場合を、終端または指定深さまで
//     // 仮実行して評価値を返す。元のStateは変更しない。
//     Score evaluate_action(const State&, const Action&) const;
//   };
//
// Runnerは候補列と評価値の一時メモリを再利用する。
// State更新、Action履歴保存、出力は問題依存なので呼び出し側が行う。
// 同点ならgenerate_actionsで先に返されたActionを選ぶ。
// ↓↓↓ ここから下はライブラリ本体。通常は編集しない。↓↓↓
template <class Problem>
struct DeterministicRolloutRunner {
  using State = typename Problem::State;
  using Action = typename Problem::Action;
  using Score = typename Problem::Score;

  explicit DeterministicRolloutRunner(Problem& problem,
                                      bool maximize = true)
      : problem_(problem), maximize_(maximize) {}

  Action choose_action(const State& state) {
    actions_.clear();
    auto&& generated_actions = problem_.generate_actions(state);
    for (const auto& action : generated_actions) actions_.push_back(action);
    if (actions_.empty()) {
      throw std::runtime_error("generate_actions returned no action");
    }

    scores_.clear();
    for (const Action& action : actions_) {
      scores_.push_back(problem_.evaluate_action(state, action));
    }

    best_index_ = 0;
    for (std::size_t index = 1; index < scores_.size(); ++index) {
      const bool better = maximize_ ? scores_[best_index_] < scores_[index]
                                    : scores_[index] < scores_[best_index_];
      if (better) best_index_ = index;
    }
    return actions_[best_index_];
  }

  void reserve(int action_count) {
    if (action_count < 0) {
      throw std::invalid_argument("action_count must be non-negative");
    }
    actions_.reserve(static_cast<std::size_t>(action_count));
    scores_.reserve(static_cast<std::size_t>(action_count));
  }

  const std::vector<Action>& last_actions() const { return actions_; }
  const std::vector<Score>& last_scores() const { return scores_; }
  std::size_t last_best_index() const { return best_index_; }

 private:
  Problem& problem_;
  bool maximize_;
  std::vector<Action> actions_;
  std::vector<Score> scores_;
  std::size_t best_index_ = 0;
};
// END LIBRARY: deterministic-rollout.hpp
// BEGIN LIBRARY: prefix-replay.hpp
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/prefix-replay.hpp

// 行動列の変更前を使い回し、変わった位置以降だけ再実行する。
// 焼きなまし、順序最適化、スケジュールの仮評価向け。
//
// TODO: Stateへ途中のシミュレーション状態を、Actionへ1行動を書く。
// TODO: step(State&, const Action&)に1行動の状態更新を書く。
//   状態を最後まで進めるための情報は全てStateへ置く。
//   外部入力は固定にし、stepの副作用や隠れた乱数状態に依存しないこと。
// TODO: Actionのoperator==を書く。同じ遷移を表す行動だけを等しいとする。
//   intなどなら追加実装は不要。
//
// PrefixReplay<MyState, MyAction> replay(initial, 16);
// replay.evaluate(initial_actions, step);
// replay.commit();
// const MyState& end = replay.evaluate(candidate_actions, step);
// if (better(end)) replay.commit(); // 採用時だけ確定。破棄はdiscard()または次のevaluate。
//
// evaluateは現在列・現在cacheを書き換えない。commitまで現在解を復元可能。
// 異なる長さ、末尾への追加、削除、空列も扱う。最初の相違点は自動検出。
// checkpoint_intervalを大きくするとcacheコピーが減り、再実行する手数が少し増える。
// Action列の比較・コピーはO(列長)。重いstepの再実行とStateコピーを減らす部品で、
// evaluate全体を必ずO(変更数)にするものではない。
// stepを呼ぶ範囲は、最初の相違点以前の直近checkpointから新しい列の末尾まで。
// 戻り値の参照は次のevaluate/commit/discardまで有効と考える。
// 空関数入りの使用例: template/search/prefix-replay-annealing.cpp
// ↓↓↓ ライブラリ本体。通常は編集しない。↓↓↓
template <class State, class Action>
class PrefixReplay {
 public:
  explicit PrefixReplay(State initial, int checkpoint_interval = 16)
      : current_end_(initial), trial_end_(initial) {
    if (checkpoint_interval <= 0) {
      throw std::invalid_argument("checkpoint_interval must be positive");
    }
    interval_ = static_cast<std::size_t>(checkpoint_interval);
    checkpoints_.push_back(std::move(initial));
  }

  void reserve(std::size_t action_count) {
    actions_.reserve(action_count);
    trial_actions_.reserve(action_count);
    const std::size_t count = action_count / interval_ + 1;
    checkpoints_.reserve(count);
    trial_checkpoints_.reserve(count);
  }

  template <class Step>
  const State& evaluate(const std::vector<Action>& candidate, Step&& step) {
    trial_ready_ = false;
    std::size_t common = 0;
    while (common < std::min(actions_.size(), candidate.size()) &&
           actions_[common] == candidate[common]) ++common;
    first_checkpoint_ = common / interval_;
    const std::size_t begin = first_checkpoint_ * interval_;
    trial_actions_ = candidate;
    trial_checkpoints_.clear();
    trial_end_ = checkpoints_[first_checkpoint_];
    last_replayed_actions_ = 0;
    for (std::size_t index = begin; index < candidate.size(); ++index) {
      step(trial_end_, candidate[index]);
      ++last_replayed_actions_;
      if ((index + 1) % interval_ == 0) trial_checkpoints_.push_back(trial_end_);
    }
    // stepが例外を投げたときはcommit不可。現在解は変更されていない。
    trial_ready_ = true;
    return trial_end_;
  }

  void commit() {
    if (!trial_ready_) throw std::logic_error("no completed trial to commit");
    // unchanged prefixはそのまま残す。大きいStateは新しいsuffixだけコピー。
    checkpoints_.erase(checkpoints_.begin() +
                           static_cast<std::ptrdiff_t>(first_checkpoint_ + 1),
                       checkpoints_.end());
    checkpoints_.insert(checkpoints_.end(), trial_checkpoints_.begin(),
                        trial_checkpoints_.end());
    actions_.swap(trial_actions_);
    current_end_ = trial_end_;
    trial_ready_ = false;
  }

  void discard() { trial_ready_ = false; }
  const std::vector<Action>& actions() const { return actions_; }
  const State& current_end() const { return current_end_; }
  std::size_t last_replayed_actions() const { return last_replayed_actions_; }
  std::size_t checkpoint_count() const { return checkpoints_.size(); }

 private:
  std::size_t interval_ = 1;
  std::vector<Action> actions_, trial_actions_;
  std::vector<State> checkpoints_, trial_checkpoints_;
  State current_end_, trial_end_;
  std::size_t first_checkpoint_ = 0, last_replayed_actions_ = 0;
  bool trial_ready_ = false;
};
// END LIBRARY: prefix-replay.hpp
// BEGIN LIBRARY: time-based-simulated-annealing.hpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/time-based-simulated-annealing.hpp

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
  bool step_with_threshold() {
    if (annealing_.is_over()) return false;
    ++iterations_;
    std::optional<Move> move = problem_.propose_move(
        static_cast<const State&>(current_state_),
        move_engine_,
        annealing_.cached_progress());
    if (!move.has_value()) return true;

    ++valid_moves_;
    const double threshold = annealing_.draw_acceptance_threshold();
    std::optional<Score> improvement =
        problem_.evaluate_move_with_threshold(
            static_cast<const State&>(current_state_), *move, threshold);
    if (!improvement.has_value()) {
      ++threshold_pruned_moves_;
      return true;
    }
    if (!annealing_.accept_with_threshold(*improvement, threshold)) return true;

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
// Pre-contest public solver source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/examples/search/ahc058_prefix_sa.cpp
// Official problem: https://atcoder.jp/contests/ahc058/tasks/ahc058_a
using namespace std;
using i128 = __int128_t;

#ifndef ROLLOUT_DEPTH
#define ROLLOUT_DEPTH 3
#endif
#ifndef ROLLOUT_CANDIDATES
#define ROLLOUT_CANDIDATES 12
#endif
static_assert(ROLLOUT_DEPTH == 2 || ROLLOUT_DEPTH == 3);
static_assert(ROLLOUT_CANDIDATES >= 0);

// ============================================================================
// TODO(AHC058): ここから問題依存部分。RunnerはこのProblemだけを呼ぶ。
// 編集する順番: State/Action → generate_actions → advance → evaluate_action。
// ============================================================================
struct ProductionProblem {
    static constexpr int IDS = 10, LEVELS = 4;
    static constexpr i128 VALUE_LIMIT = i128{1} << 120;
    using Counts = array<array<i128, IDS>, LEVELS>;
    using Powers = array<array<int, IDS>, LEVELS>;
    struct State {
        // TODO: 現在の機械数、強化回数、りんご数、残りターン数を置く。
        // 固定長配列なので先読みでコピーしてもヒープ確保しない。
        Counts count{};
        Powers power{};
        i128 apples = 1;
        int turns = 500;
    };
    using Action = int;  // TODO: -1=待機、それ以外=level*10+id。
    using Score = i128;  // TODO: 最終りんご数の見積り。浮動小数へ丸めず比較。

    array<long long, IDS> production{};
    array<array<long long, IDS>, LEVELS> cost{};

    static i128 add(i128 a, i128 b) {
        return a >= VALUE_LIMIT - b ? VALUE_LIMIT : a + b;
    }
    static i128 multiply(i128 a, i128 b) {
        if (a == 0 || b == 0) return 0;
        return a >= VALUE_LIMIT / b ? VALUE_LIMIT : a * b;
    }
    static long long combination(int n, int k) {
        if (k < 0 || k > n) return 0;
        k = min(k, n - k);
        long long result = 1;
        for (int i = 1; i <= k; ++i) result = result * (n - k + i) / i;
        return result;
    }

    // TODO: 以後強化しない場合、このIDが残りturnsで作るりんご数を返す。
    // 毎ターン再生せず、機械の線形な増加を二項係数でまとめる。
    i128 future_production(int id, const State& state) const {
        i128 sum = multiply(state.count[0][id], state.turns);
        i128 product = 1;
        for (int level = 1; level < LEVELS; ++level) {
            product = multiply(product, state.power[level][id]);
            i128 term = multiply(state.count[level][id], product);
            term = multiply(term, combination(state.turns, level + 1));
            sum = add(sum, term);
        }
        return multiply(multiply(sum, state.power[0][id]), production[id]);
    }

    long long upgrade_cost(const State& state, Action action) const {
        return cost[action / IDS][action % IDS] *
               (state.power[action / IDS][action % IDS] + 1LL);
    }

    // TODO: 今実行可能な最初の1手を返す。待機も必ず入れる。
    // 同点では列挙順を優先するので、旧版と同じ「待機→番号順」。
    vector<Action> generate_actions(const State& state) const {
        vector<Action> actions;
        actions.reserve(1 + IDS * LEVELS);
        actions.push_back(-1);
        for (int code = 0; code < IDS * LEVELS; ++code) {
            if (state.apples >= upgrade_cost(state, code)) actions.push_back(code);
        }
        return actions;
    }

    // TODO: 合法なactionを反映し、公式順序で1ターン進める。
    // 本番の更新と仮実行で必ず同じ処理を使う。Score用の飽和とは分ける。
    void advance(State& state, Action action) const {
        assert(state.turns > 0);
        if (action != -1) {
            assert(action >= 0 && action < IDS * LEVELS);
            const long long payment = upgrade_cost(state, action);
            assert(state.apples >= payment);
            state.apples -= payment;
            ++state.power[action / IDS][action % IDS];
        }
        for (int id = 0; id < IDS; ++id) {
            state.apples += state.count[0][id] * state.power[0][id] * production[id];
        }
        for (int level = 1; level < LEVELS; ++level) {
            for (int id = 0; id < IDS; ++id) {
                state.count[level - 1][id] += state.count[level][id] * state.power[level][id];
            }
        }
        --state.turns;
    }

    // TODO: 今1回だけ強化するか待機し、以後待機した場合の最大最終値を返す。
    Score value_with_one_upgrade(State& state) const {
        i128 value = state.apples;
        for (int id = 0; id < IDS; ++id) value = add(value, future_production(id, state));
        if (state.turns == 0) return value;
        i128 best_gain = 0;
        for (int code = 0; code < IDS * LEVELS; ++code) {
            const long long payment = upgrade_cost(state, code);
            if (state.apples < payment) continue;
            const int id = code % IDS, level = code / IDS;
            const i128 before = future_production(id, state);
            ++state.power[level][id];
            const i128 after = future_production(id, state);
            --state.power[level][id];
            best_gain = max(best_gain, after - before - payment);
        }
        return add(value, best_gain);
    }

    // TODO: 2手目だけ絞り込む。上位12個に各Level代表と待機を足す。
    // 長期投資の種類を1つの評価の上位だけで全滅させないため。
    vector<Action> second_actions(State& state) const {
        struct Ranked { i128 gain; int code; };
        vector<Ranked> ranked;
        ranked.reserve(IDS * LEVELS);
        array<int, LEVELS> best_code;
        array<i128, LEVELS> best_gain;
        best_code.fill(-1);
        best_gain.fill(-VALUE_LIMIT);
        for (int code = 0; code < IDS * LEVELS; ++code) {
            const long long payment = upgrade_cost(state, code);
            if (state.apples < payment) continue;
            const int id = code % IDS, level = code / IDS;
            const i128 before = future_production(id, state);
            ++state.power[level][id];
            const i128 after = future_production(id, state);
            --state.power[level][id];
            const i128 gain = after - before - payment;
            ranked.push_back({gain, code});
            if (gain > best_gain[level]) {
                best_gain[level] = gain;
                best_code[level] = code;
            }
        }
        sort(ranked.begin(), ranked.end(), [](const Ranked& a, const Ranked& b) {
            return a.gain > b.gain;
        });
        vector<Action> actions{-1};
        const int count = min(ROLLOUT_CANDIDATES, static_cast<int>(ranked.size()));
        for (int i = 0; i < count; ++i) actions.push_back(ranked[i].code);
        for (int code : best_code) {
            if (code != -1 && find(actions.begin(), actions.end(), code) == actions.end()) {
                actions.push_back(code);
            }
        }
        return actions;
    }

    // TODO: actionを最初に選んだときの評価を返す。元のstateは変えない。
    // 既定3手: 最初の1手→絞った2手目→式で選ぶ3手目→以後待機。
    Score evaluate_action(const State& state, Action action) const {
        State next = state;
        advance(next, action);
        if (next.turns == 0) return next.apples;
#if ROLLOUT_DEPTH >= 3
        i128 best = -1;
        for (Action second : second_actions(next)) {
            State after = next;
            advance(after, second);
            best = max(best, value_with_one_upgrade(after));
        }
        return best;
#else
        return value_with_one_upgrade(next);
#endif
    }

    // TODO: 入力を読む。公式の固定サイズを確認してから固定長配列へ格納する。
    State read_input() {
        int n, levels, turns;
        long long apples;
        if (!(cin >> n >> levels >> turns >> apples)) throw runtime_error("missing input");
        if (n != IDS || levels != LEVELS || turns != 500 || apples != 1) {
            throw runtime_error("expected AHC058 input: 10 4 500 1");
        }
        for (auto& x : production) cin >> x;
        for (auto& row : cost) for (auto& x : row) cin >> x;
        if (!cin) throw runtime_error("incomplete input");
        State state;
        state.apples = apples;
        state.turns = turns;
        for (auto& row : state.count) row.fill(1);
        return state;
    }
};


// ============================================================================
// TODO(AHC058-SA): 購入順序を改善する問題依存部分。
// Stateは短い行動列だけ。大きいprefix cacheはProblem側に1個だけ持つ。
// ============================================================================
#ifndef AHC058_TIME_LIMIT_MS
#define AHC058_TIME_LIMIT_MS 1850
#endif
#ifndef AHC058_SA_ITERATIONS
#define AHC058_SA_ITERATIONS 0
#endif
#ifndef AHC058_RANDOM_SEED
#define AHC058_RANDOM_SEED 58
#endif

struct PurchaseSequenceProblem {
    using SimulationState = ProductionProblem::State;
    using Score = long long; // TODO: 最終りんご数を公式のlogスコアへ変換する。
    struct State {
        vector<int> actions; // TODO: 待機を除いた「購入する順序」。先頭0は保護する。
        Score score = 0;
    };
    struct Move { vector<int> actions; }; // TODO: 変更後の購入順序。

    const ProductionProblem& game;
    SimulationState initial;
    PrefixReplay<SimulationState, int> replay;
    Score trial_score = 0;

    PurchaseSequenceProblem(const ProductionProblem& game_value,
                            const SimulationState& initial_value)
        : game(game_value), initial(initial_value), replay(initial_value, 8) {
        replay.reserve(500);
    }

    // TODO: 購入しない間の生産量をC(t,1)..C(t,4)の係数で表す。
    // 同じ係数で待ち時間の二分探索を行うので、各判定は4項だけになる。
    array<i128, 4> income_coefficients(const SimulationState& state) const {
        array<i128, 4> coefficients{};
        for (int id = 0; id < ProductionProblem::IDS; ++id) {
            i128 factor = game.production[id];
            for (int level = 0; level < ProductionProblem::LEVELS; ++level) {
                factor *= state.power[level][id];
                coefficients[level] += state.count[level][id] * factor;
            }
        }
        return coefficients;
    }

    static i128 income(const array<i128, 4>& coefficients, int turns) {
        i128 result = 0;
        for (int level = 0; level < 4; ++level) {
            result += coefficients[level] *
                      ProductionProblem::combination(turns, level + 1);
        }
        return result;
    }

    // TODO: 行動しない区間を一括更新する。更新順は下位levelから。
    // 上位機械の個数はまだ書き換えていないので、その区間の開始時点の値を使える。
    void wait_turns(SimulationState& state, int turns,
                    const array<i128, 4>& coefficients) const {
        assert(0 <= turns && turns <= state.turns);
        state.apples += income(coefficients, turns);
        for (int id = 0; id < ProductionProblem::IDS; ++id) {
            for (int lower = 0; lower < ProductionProblem::LEVELS - 1; ++lower) {
                i128 factor = 1;
                for (int upper = lower + 1; upper < ProductionProblem::LEVELS; ++upper) {
                    factor *= state.power[upper][id];
                    state.count[lower][id] += state.count[upper][id] * factor *
                        ProductionProblem::combination(turns, upper - lower);
                }
            }
        }
        state.turns -= turns;
    }

    // TODO: 次の購入を最速で実行する。資金不足なら必要なだけ待つ。
    // 間に合わなければ残りを待機し、後続の購入も実行しない。
    // 「買えない操作を出力する」ことはない。入力制約内では128bitで十分。
    void advance_purchase(SimulationState& state, int action) const {
        if (state.turns == 0) return;
        assert(0 <= action && action < 40);
        const long long payment = game.upgrade_cost(state, action);
        if (state.apples < payment) {
            const auto coefficients = income_coefficients(state);
            const int last_wait = state.turns - 1;
            if (state.apples + income(coefficients, last_wait) < payment) {
                wait_turns(state, state.turns, coefficients);
                return;
            }
            int low = 0, high = last_wait;
            while (high - low > 1) {
                const int middle = (low + high) / 2;
                if (state.apples + income(coefficients, middle) >= payment) high = middle;
                else low = middle;
            }
            wait_turns(state, high, coefficients);
        }
        game.advance(state, action);
    }

    i128 final_apples(const SimulationState& state) const {
        return state.apples + income(income_coefficients(state), state.turns);
    }

    Score score(const SimulationState& state) const {
        return llround(100000.0 * log2(static_cast<double>(final_apples(state))));
    }

    State make_state(vector<int> actions) {
        State state{std::move(actions), 0};
        const auto step = [this](SimulationState& simulation, int action) {
            advance_purchase(simulation, action);
        };
        state.score = score(replay.evaluate(state.actions, step));
        replay.commit();
        return state;
    }

    // TODO: 近傍を作る。先頭の安い生産機だけは残し、交換/移動/変更/挿入/削除する。
    // ときどき同じIDの4段階をまとめて挿入し、投資先変更も試す。
    optional<Move> propose_move(const State& state, mt19937_64& engine,
                                double /* progress */) const {
        Move move{state.actions};
        auto& actions = move.actions;
        const int size = static_cast<int>(actions.size());
        if (size == 0) return nullopt;
        const auto random_index = [&](int n) { return static_cast<int>(engine() % static_cast<uint64_t>(n)); };
        const auto random_action = [&]() {
            if ((engine() & 1U) != 0) return random_index(40);
            const int id = actions[random_index(size)] % 10;
            return random_index(4) * 10 + id;
        };
        const int kind = random_index(100);
        if (kind < 40 && size > 2) {
            const int from = 1 + random_index(size - 1);
            int to = 1 + random_index(size - 1);
            if ((engine() & 1U) != 0) {
                to = clamp(from + random_index(13) - 6, 1, size - 1);
            }
            if (kind < 20) swap(actions[from], actions[to]);
            else {
                const int action = actions[from];
                actions.erase(actions.begin() + from);
                actions.insert(actions.begin() + to, action);
            }
        } else if (kind < 65 && size > 1) {
            actions[1 + random_index(size - 1)] = random_action();
        } else if (kind < 80 && size < 500) {
            const int action = random_action();
            actions.insert(actions.begin() + 1 + random_index(size), action);
        } else if (kind < 95 && size > 1) {
            actions.erase(actions.begin() + 1 + random_index(size - 1));
        } else if (size <= 496) {
            const int id = random_index(10);
            const int at = 1 + random_index(min(size, 80));
            const array<int, 4> block{{id, 10 + id, 20 + id, 30 + id}};
            actions.insert(actions.begin() + at, block.begin(), block.end());
        }
        if (actions == state.actions) return nullopt;
        return move;
    }

    // TODO: 仮実行で得たスコアの「差分」を返す。採用前のcacheは壊さない。
    Score evaluate_move(const State& state, const Move& move) {
        const auto step = [this](SimulationState& simulation, int action) {
            advance_purchase(simulation, action);
        };
        // restart_from_best後にも使えるよう、cacheを現在の行動列へ同期する。
        if (replay.actions() != state.actions) {
            replay.evaluate(state.actions, step);
            replay.commit();
        }
        trial_score = score(replay.evaluate(move.actions, step));
        return trial_score - state.score;
    }

    // TODO: 採用されたときだけ候補とcacheを確定する。
    void apply_move(State& state, Move& move) {
        replay.commit();
        state.actions = std::move(move.actions);
        state.score = trial_score;
    }

    // TODO: 購入順序を、合法な500ターンの出力へ復号する。
    vector<int> decode(const vector<int>& purchases, i128& apples) const {
        auto state = initial;
        vector<int> actions;
        actions.reserve(500);
        for (int action : purchases) {
            if (state.turns == 0) break;
            const long long payment = game.upgrade_cost(state, action);
            while (state.turns > 0 && state.apples < payment) {
                actions.push_back(-1);
                game.advance(state, -1);
            }
            if (state.turns == 0) break;
            actions.push_back(action);
            game.advance(state, action);
        }
        while (state.turns > 0) {
            actions.push_back(-1);
            game.advance(state, -1);
        }
        apples = state.apples;
        return actions;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    const auto started = chrono::steady_clock::now();
    const auto elapsed_ms = [&]() {
        return chrono::duration<double, milli>(chrono::steady_clock::now() - started).count();
    };
    ProductionProblem game;
    const auto initial = game.read_input();
    auto simulation = initial;
    DeterministicRolloutRunner<ProductionProblem> rollout(game);
    rollout.reserve(41);
    vector<int> baseline, purchases;
    baseline.reserve(500);
    purchases.reserve(500);
    // 既存の3手先読みで初期解を構築。初期解作成も総時間に含める。
    while (simulation.turns > 0) {
        const int action = rollout.choose_action(simulation);
        baseline.push_back(action);
        if (action != -1) purchases.push_back(action);
        game.advance(simulation, action);
    }
    const i128 baseline_apples = simulation.apples;
    PurchaseSequenceProblem problem(game, initial);
    auto state = problem.make_state(std::move(purchases));
    const double remaining_ms = AHC058_TIME_LIMIT_MS - elapsed_ms();
    vector<int> best_purchases = state.actions;
    uint64_t iterations = 0;
    if (remaining_ms > 10 || AHC058_SA_ITERATIONS > 0) {
        // TODO: 公式スコアの差に対する温度。例: 2500点→5点。
        TimeBasedAnnealingRunner<PurchaseSequenceProblem> annealing(
            problem, state, state.score,
            AHC058_SA_ITERATIONS > 0 ? 1e9 : remaining_ms,
            2500, 5, AHC058_RANDOM_SEED, 8);
        if (AHC058_SA_ITERATIONS > 0) {
            // 固定反復は回帰テスト専用。通常提出は時間制限で止める。
            for (int i = 0; i < AHC058_SA_ITERATIONS; ++i) annealing.step();
        } else annealing.run();
        best_purchases = annealing.best_state().actions;
        iterations = annealing.iterations();
    }
    i128 candidate_apples = 0;
    auto answer = problem.decode(best_purchases, candidate_apples);
    // 元の先読み出力を保持し、厳密なりんご数でも悪化させない。
    if (candidate_apples < baseline_apples) answer = baseline;
    for (int action : answer) {
        if (action == -1) cout << -1 << '\n';
        else cout << action / 10 << ' ' << action % 10 << '\n';
    }
    cerr << "iterations=" << iterations << " elapsed_ms=" << elapsed_ms() << '\n';
    return 0;
}
