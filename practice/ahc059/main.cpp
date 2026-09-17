#include <bits/stdc++.h>
using namespace std;

// 提出時はこのincludeをhpp全文に置換する。practice/ahc059には展開版を置く。
// BEGIN LIBRARY: large-neighborhood-search.hpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <utility>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/large-neighborhood-search.hpp
// LNS / RRTの着想: https://atcoder.jp/contests/ahc059/editorial/15052
// 解説の考え方から独自実装。第三者の提出コードは使用していない。

enum class LnsAcceptance { HillClimbing, RecordToRecord, SimulatedAnnealing };
// 直近のtrueを返したstep()の結果（採用とは限らない）。近傍への報酬などに使う。
// Rejectedには修復失敗・閾値打ち切り・非有限scoreも含む。
enum class LnsOutcome { Rejected, Accepted, ImprovedCurrent, ImprovedBest };

struct LnsOptions {
  double time_limit_ms = 1900.0;
  std::uint64_t seed = 0;
  bool maximize = true;  // 距離・コストを小さくするならfalse。
  LnsAcceptance acceptance = LnsAcceptance::HillClimbing;
  // RRT: 「現在値」ではなく「過去最良値」から許す悪化幅。線形に変化。
  double start_margin = 2.0;
  double end_margin = 2.0;
  // SA: 指数冷却。Scoreと同じ単位で指定する。
  double start_temperature = 10.0;
  double end_temperature = 0.1;
  bool early_cutoff = true;
  int clock_interval = 1;
  // 有限値なら進捗は反復数から計算する（再現テスト用）。時計の上限も守る。
  std::uint64_t iteration_limit = std::numeric_limits<std::uint64_t>::max();
};

// 部分破壊・再構築の制御だけを担当する。これ1ファイルで使える。
// Problemには次の2関数を書く（詳しいTODOはtemplate/searchにある）。
//   void destroy(const State& current, State& candidate,
//                std::mt19937_64& rng, double progress);
//     currentを変更せず、candidateを「一部壊れた状態」に上書きする。
//   std::optional<Score> repair(State& candidate, std::mt19937_64& rng,
//                               double progress, long double threshold);
//     修復成功なら「完成した候補の絶対スコア」を返す。改善量ではない！
//     不可能 / 閾値を超えられないと確定した時だけnulloptを返す。
//     最大化なら score >= threshold、最小化なら score <= threshold が採用条件。
//     cutoff OFF時はそれぞれ -inf / +inf が渡される。
//
// Stateは通常の値型（コピー・swap可能、デフォルト構築不要）。Scoreは数値型。
// repairがnulloptでも元のcurrentは壊れない。candidateのvector容量は再利用する。
// destroyでcandidate=currentとすれば簡単。重い場合は必要な要素だけ詰め直す。
// ProblemはRunnerより長生きさせる。Problem側の作業bufferを共有する同時実行は不可。
// 時計は近傍の間で見る。1回のdestroy/repairが長すぎる場合は問題側でも制限する。
template <class Problem>
class LargeNeighborhoodSearch {
 public:
  using State = typename Problem::State;
  using Score = typename Problem::Score;

  LargeNeighborhoodSearch(Problem& problem, State initial, Score initial_score,
                          LnsOptions options = {})
      : problem_(problem), options_(options), current_(std::move(initial)),
        candidate_(current_), best_(current_), current_score_(initial_score),
        best_score_(initial_score), move_engine_(options.seed),
        acceptance_engine_(options.seed ^ 0xd1b54a32d192ed03ULL) {
    static_assert(std::is_arithmetic<Score>::value, "Score must be numeric");
    static_assert(!std::is_integral<Score>::value ||
                      std::numeric_limits<long double>::digits >=
                          std::numeric_limits<Score>::digits,
                  "long double must represent Score integers exactly");
    if (!(options_.time_limit_ms > 0) || !std::isfinite(options_.time_limit_ms) ||
        options_.clock_interval <= 0 ||
        !finite_nonnegative(options_.start_margin) ||
        !finite_nonnegative(options_.end_margin) ||
        !(options_.start_temperature > 0) ||
        !std::isfinite(options_.start_temperature) ||
        !(options_.end_temperature > 0) ||
        !std::isfinite(options_.end_temperature) ||
        !std::isfinite(static_cast<long double>(initial_score))) {
      throw std::invalid_argument("invalid LNS options or initial score");
    }
    switch (options_.acceptance) {
      case LnsAcceptance::HillClimbing:
      case LnsAcceptance::RecordToRecord:
      case LnsAcceptance::SimulatedAnnealing: break;
      default: throw std::invalid_argument("invalid LNS acceptance");
    }
    log_start_ = std::log(options_.start_temperature);
    log_end_ = std::log(options_.end_temperature);
    started_ = Clock::now();
  }

  // true: 近傍を1回試した（採用とは限らない）。false: 予算終了。
  bool step() {
    if (stopped_ || iterations_ >= options_.iteration_limit) return false;
    if (until_clock_ == 0) {
      const double elapsed = elapsed_ms();
      if (elapsed >= options_.time_limit_ms) {
        stopped_ = true;
        return false;
      }
      progress_ = std::clamp(elapsed / options_.time_limit_ms, 0.0, 1.0);
      until_clock_ = options_.clock_interval;
    }
    --until_clock_;
    if (options_.iteration_limit != std::numeric_limits<std::uint64_t>::max()) {
      progress_ = static_cast<double>(iterations_) /
                  static_cast<double>(options_.iteration_limit);
    }

    last_outcome_ = LnsOutcome::Rejected;
    const long double threshold = acceptance_threshold();
    const long double evaluation_threshold = options_.early_cutoff ? threshold :
        (options_.maximize ? -std::numeric_limits<long double>::infinity() :
                             std::numeric_limits<long double>::infinity());
    problem_.destroy(static_cast<const State&>(current_), candidate_,
                     move_engine_, progress_);
    const std::optional<Score> score =
        problem_.repair(candidate_, move_engine_, progress_, evaluation_threshold);
    ++iterations_;
    if (!score || !std::isfinite(static_cast<long double>(*score))) {
      ++rejected_repairs_;
      return true;
    }
    const long double value = static_cast<long double>(*score);
    if (options_.maximize ? value < threshold : value > threshold) return true;
    last_outcome_ = better(*score, current_score_) ? LnsOutcome::ImprovedCurrent : LnsOutcome::Accepted;
    using std::swap;
    swap(current_, candidate_);
    current_score_ = *score;
    ++accepted_;
    if (better(*score, best_score_)) {
      best_ = current_;
      best_score_ = *score;
      ++improved_;
      last_outcome_ = LnsOutcome::ImprovedBest;
    }
    return true;
  }

  void run() { while (step()) {} }
  const State& best_state() const { return best_; }
  const State& current_state() const { return current_; }
  Score best_score() const { return best_score_; }
  Score current_score() const { return current_score_; }
  std::uint64_t iterations() const { return iterations_; }
  std::uint64_t accepted() const { return accepted_; }
  std::uint64_t improved() const { return improved_; }
  std::uint64_t rejected_repairs() const { return rejected_repairs_; }
  LnsOutcome last_outcome() const { return last_outcome_; }
  double progress() const { return progress_; }
  double elapsed_ms() const {
    return std::chrono::duration<double, std::milli>(Clock::now() - started_).count();
  }
  // 時間・反復予算・温度進捗はリセットしない。
  void restart_from_best() { current_ = best_; current_score_ = best_score_; }

 private:
  using Clock = std::chrono::steady_clock;
  static bool finite_nonnegative(double value) {
    return value >= 0 && std::isfinite(value);
  }
  bool better(Score first, Score second) const {
    return options_.maximize ? first > second : first < second;
  }
  long double acceptance_threshold() {
    if (options_.acceptance == LnsAcceptance::HillClimbing) {
      return static_cast<long double>(current_score_);
    }
    long double base = static_cast<long double>(best_score_);
    long double margin = (1.0L - progress_) * options_.start_margin +
                         progress_ * options_.end_margin;
    if (options_.acceptance == LnsAcceptance::SimulatedAnnealing) {
      base = static_cast<long double>(current_score_);
      // 0 < u <= 1。log(0)は起きない。近傍生成用とは別の乱数列。
      const double u = static_cast<double>((acceptance_engine_() >> 11) + 1) *
                       (1.0 / 9007199254740992.0);
      const double temperature = std::exp((1.0 - progress_) * log_start_ +
                                          progress_ * log_end_);
      margin = -static_cast<long double>(temperature) * std::log(u);
    }
    // 整数Scoreの悪化量も整数。先にfloorすれば、uint64_t最大値付近でも
    // 「0.5まで許す」が絶対閾値の丸めで「1まで許す」に変わらない。
    if constexpr (std::is_integral<Score>::value) margin = std::floor(margin);
    return options_.maximize ? base - margin : base + margin;
  }
  Problem& problem_;
  LnsOptions options_;
  State current_, candidate_, best_;
  Score current_score_, best_score_;
  std::mt19937_64 move_engine_, acceptance_engine_;
  Clock::time_point started_;
  std::uint64_t iterations_ = 0, accepted_ = 0, improved_ = 0, rejected_repairs_ = 0;
  int until_clock_ = 0;
  bool stopped_ = false;
  LnsOutcome last_outcome_ = LnsOutcome::Rejected;
  double progress_ = 0, log_start_ = 0, log_end_ = 0;
};
// END LIBRARY: large-neighborhood-search.hpp
// BEGIN LIBRARY: adaptive-operator-selector.hpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/adaptive-operator-selector.hpp
// ALNSの着想（過去の成果に応じて近傍の選択頻度を変える）:
// https://doi.org/10.1287/trsc.1050.0135
// 概念を参考にした独自実装。論文の実験設定や第三者コードの再現ではない。

struct AdaptiveOperatorOptions {
  bool adaptive = true; // falseなら常に等確率。比較実験用。
  int update_interval = 128; // この回数のrecordごとに重みを更新する。
  double learning_rate = 0.2; // 0: 学習なし、1: 直近区間の平均報酬で置換。
  double exploration = 0.1; // 選択確率のうち一様分布を混ぜる割合。(0,1]
};

// 少数個の近傍向け。選択O(近傍数)、通常の記録O(1)、区間更新O(近傍数)。
// select/record中はメモリ確保しない。時計も盤面hashも一致判定も不要。
// SA/LNS等から独立。select(rng)の戻り値0..size()-1で壊し方を選び、
// 試行後にrecord(id, reward)へ「0以上1以下の成果」を渡す。
// 失敗・枝刈りも報酬0で必ず記録する。成功だけ記録すると成功率を学べない。
// 報酬は問題依存。採用数≠得点改善なので、設定は実問題scoreで検証する。
class AdaptiveOperatorSelector {
 public:
  explicit AdaptiveOperatorSelector(int count, AdaptiveOperatorOptions options = {})
      : options_(options) {
    if (count <= 0 || options.update_interval <= 0 ||
        !std::isfinite(options.learning_rate) || options.learning_rate < 0 || options.learning_rate > 1 ||
        !std::isfinite(options.exploration) || options.exploration <= 0 || options.exploration > 1) {
      throw std::invalid_argument("invalid adaptive operator options");
    }
    weights_.assign(count, 1.0);
    rewards_.assign(count, 0.0);
    used_.assign(count, 0);
    probabilities_.resize(count);
    cumulative_.resize(count);
    rebuild();
  }
  int size() const { return static_cast<int>(weights_.size()); }
  template <class Random> int select(Random& rng) const {
    const double value = std::generate_canonical<double, 53>(rng); // [0,1)
    for (int id = 0; id + 1 < size(); ++id) if (value < cumulative_[id]) return id;
    return size() - 1; // 丸め誤差でも範囲外を返さない。
  }
  void record(int id, double reward) {
    if (id < 0 || id >= size() || !std::isfinite(reward) || reward < 0 || reward > 1) {
      throw std::invalid_argument("operator id/reward out of range");
    }
    if (!options_.adaptive) return;
    rewards_[id] += reward;
    ++used_[id];
    if (++pending_ == options_.update_interval) update();
  }
  double probability(int id) const { return probabilities_.at(id); }
  // 未完の区間を反映したい時のみ手動で呼ぶ。通常はrecordが自動実行する。
  void update() {
    if (!pending_) return;
    for (int id = 0; id < size(); ++id) {
      if (used_[id]) {
        const double mean = rewards_[id] / used_[id];
        weights_[id] = (1 - options_.learning_rate) * weights_[id] + options_.learning_rate * mean;
      } // 未試行の近傍は「失敗」と見なさず、以前の重みを維持する。
      used_[id] = 0;
      rewards_[id] = 0;
    }
    pending_ = 0;
    rebuild();
  }
 private:
  void rebuild() {
    double sum = 0;
    for (double weight : weights_) sum += weight;
    double cumulative = 0;
    for (int id = 0; id < size(); ++id) {
      const double learned = sum > 0 ? weights_[id] / sum : 1.0 / size();
      probabilities_[id] = options_.adaptive ?
          options_.exploration / size() + (1 - options_.exploration) * learned : 1.0 / size();
      cumulative_[id] = cumulative += probabilities_[id];
    }
    cumulative_.back() = 1.0;
  }
  AdaptiveOperatorOptions options_;
  std::vector<double> weights_, rewards_, probabilities_, cumulative_;
  std::vector<int> used_;
  int pending_ = 0;
};
// END LIBRARY: adaptive-operator-selector.hpp
// Pre-contest public solver source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/examples/search/ahc059_lns.cpp
// 問題: https://atcoder.jp/contests/ahc059/tasks/ahc059_a
// 着想: https://atcoder.jp/contests/ahc059/editorial/15052
// 部分破壊・再挿入という考え方から独自実装。第三者の提出コードは参照していない。

#ifndef AHC059_LNS_TIME_MS
#define AHC059_LNS_TIME_MS 1850.0
#endif
#ifndef AHC059_LNS_MODE
#define AHC059_LNS_MODE 2 // 0=山登り、1=RRT、2=SA
#endif
#ifndef AHC059_LNS_MARGIN
#define AHC059_LNS_MARGIN 2.0
#endif
#ifndef AHC059_LNS_CUTOFF
#define AHC059_LNS_CUTOFF 1
#endif
#ifndef AHC059_LNS_PRECOMPUTE
#define AHC059_LNS_PRECOMPUTE 1
#endif
#ifndef AHC059_LNS_SPAN
#define AHC059_LNS_SPAN 15
#endif
#ifndef AHC059_ALNS_POLICY
#define AHC059_ALNS_POLICY 0 // 0=従来の区間15のみ、1=5種類を等確率、2=成果から適応
#endif

struct CardPairProblem {
  // TODO(問題依存): Stateは「カードを取る順番」。要素は元のマス番号。
  // 同じ番号の2枚が交差しない順番だけを保持する。X(置く)は使わない。
  struct State {
    vector<int> order;
    int cost = 0; // (0,0)出発、最後のカードで終了。帰りの距離は含めない。
  };
  using Score = int; // 距離をそのまま返す。Options.maximize=false。
  int n = 0, pairs = 0;
  vector<int> label;
  vector<array<int, 2>> cells;
  vector<int> distances;
  vector<int> removed; // 破壊・修復間だけ使う作業領域。最良解には不要。
  uint64_t seed = 0x123456789abcdefULL;
  bool precompute = AHC059_LNS_PRECOMPUTE;
  int operator_id = -1; // -1=従来版、0..3=区間4/8/15/30、4=離れた4ペア

  void read_input(istream& input = cin) {
    input >> n;
    if (n < 2 || n > 20 || n % 2) throw runtime_error("invalid N");
    pairs = n * n / 2;
    label.resize(n * n);
    cells.resize(pairs);
    vector<int> count(pairs);
    for (int cell = 0; cell < n * n; ++cell) {
      int id;
      if (!(input >> id) || id < 0 || id >= pairs || count[id] == 2) {
        throw runtime_error("invalid card input");
      }
      label[cell] = id;
      cells[id][count[id]++] = cell;
      seed = (seed ^ static_cast<uint64_t>(id + 1)) * 0x9e3779b97f4a7c15ULL;
    }
    if (precompute) {
      distances.resize(n * n * n * n);
      for (int a = 0; a < n * n; ++a) {
        for (int b = 0; b < n * n; ++b) {
          distances[a * n * n + b] = abs(a / n - b / n) + abs(a % n - b % n);
        }
      }
    }
    removed.reserve(pairs);
  }
  int distance(int a, int b) const {
    return precompute ? distances[a * n * n + b] :
        abs(a / n - b / n) + abs(a % n - b % n);
  }
  int route_cost(const vector<int>& order) const {
    int cost = 0, previous = 0;
    for (int cell : order) { cost += distance(previous, cell); previous = cell; }
    return cost;
  }
  struct Insertion { int delta, first_gap, second_gap, first_cell, second_cell; };

  // TODO(問題依存): 1ペアを合法に入れる最小増分をO(現在のカード枚数)で求める。
  // 挿入する2箇所では「山札の中身」が一致する必要がある。
  // 有効な括弧列なので、山札の一番上のペア番号が同じなら祖先も同じ。
  // 各山札状態ごとに、先に置くカードの挿入増分の最小値だけを保持する。
  // 同じ隙間に2枚とも入れる場合は、辺が共有されるので別計算する。
  Insertion best_insertion(const vector<int>& order, int id) const {
    const int infinity = 1000000;
    const int a = cells[id][0], b = cells[id][1];
    array<int, 201> min_a, min_b, pos_a{}, pos_b{}, parent{};
    min_a.fill(infinity); min_b.fill(infinity);
    Insertion best{infinity, 0, 0, a, b};
    int context = 0; // 空の山札=0、トップのペア番号+1=それ以外。
    const int size = static_cast<int>(order.size());
    auto consider = [&](int delta, int i, int j, int x, int y) {
      if (delta < best.delta) best = {delta, i, j, x, y};
    };
    for (int gap = 0; gap <= size; ++gap) {
      const int before = gap == 0 ? 0 : order[gap - 1];
      const int after = gap == size ? -1 : order[gap];
      const int old_edge = after < 0 ? 0 : distance(before, after);
      const int a_after = after < 0 ? 0 : distance(a, after);
      const int b_after = after < 0 ? 0 : distance(b, after);
      const int a_before = distance(before, a), b_before = distance(before, b);
      const int da = a_before + a_after - old_edge;
      const int db = b_before + b_after - old_edge;
      if (min_a[context] != infinity) {
        consider(min_a[context] + db, pos_a[context], gap, a, b);
      }
      if (min_b[context] != infinity) {
        consider(min_b[context] + da, pos_b[context], gap, b, a);
      }
      consider(a_before + distance(a, b) + b_after - old_edge, gap, gap, a, b);
      consider(b_before + distance(a, b) + a_after - old_edge, gap, gap, b, a);
      if (da < min_a[context]) { min_a[context] = da; pos_a[context] = gap; }
      if (db < min_b[context]) { min_b[context] = db; pos_b[context] = gap; }
      if (gap < size) {
        const int next_context = label[order[gap]] + 1;
        if (next_context == context) context = parent[context];
        else { parent[next_context] = context; context = next_context; }
      }
    }
    return best;
  }
  void insert_pair(State& state, int id) const {
    const Insertion choice = best_insertion(state.order, id);
    // 後ろから挿入すれば元のgap位置がずれない。同じgapでも順番はfirst,second。
    state.order.insert(state.order.begin() + choice.second_gap, choice.second_cell);
    state.order.insert(state.order.begin() + choice.first_gap, choice.first_cell);
    state.cost += choice.delta;
  }
  State make_initial_state() const {
    State state;
    state.order.reserve(n * n);
    vector<int> ids(pairs);
    iota(ids.begin(), ids.end(), 0);
    mt19937_64 engine(seed);
    shuffle(ids.begin(), ids.end(), engine);
    for (int id : ids) insert_pair(state, id);
    return state;
  }
  // TODO(問題依存): 区間内に現れるペアを2枚とも除く。残りは必ず合法。
  void destroy(const State& current, State& candidate,
               mt19937_64& engine, double /* progress */) {
    const int size = static_cast<int>(current.order.size());
    array<bool, 200> erase{};
    removed.clear();
    if (operator_id == 4) {
      // TODO(問題依存): 離れたペアを選ぶ別の壊し方。重複なしで最大4組。
      while (static_cast<int>(removed.size()) < min(4, pairs)) {
        const int id = static_cast<int>(engine() % static_cast<uint64_t>(pairs));
        if (!erase[id]) { erase[id] = true; removed.push_back(id); }
      }
    } else {
      constexpr int spans[] = {4, 8, 15, 30};
      const int span = min(size, operator_id < 0 ? AHC059_LNS_SPAN : spans[operator_id]);
      const int left = static_cast<int>(engine() % static_cast<uint64_t>(size - span + 1));
      for (int i = left; i < left + span; ++i) {
        const int id = label[current.order[i]];
        if (!erase[id]) { erase[id] = true; removed.push_back(id); }
      }
    }
    candidate.order.clear(); // capacityは捨てない。current全体もコピーしない。
    for (int cell : current.order) if (!erase[label[cell]]) candidate.order.push_back(cell);
    candidate.cost = route_cost(candidate.order);
    // 打ち切りのON/OFFで次の乱数列が変わらないよう、乱数は修復前に消費する。
    shuffle(removed.begin(), removed.end(), engine);
  }
  // TODO(問題依存): 修復成功時だけ、完成候補の絶対距離を返す。
  optional<Score> repair(State& candidate, mt19937_64&, double,
                         long double threshold) const {
    for (int id : removed) {
      // Manhattan距離の三角不等式により、挿入で距離は減らない。
      // だから途中の距離が上限を超えたら、この修復は採用され得ない。
      if (candidate.cost > threshold) return nullopt;
      insert_pair(candidate, id);
    }
    return candidate.cost;
  }
  bool is_valid(const State& state) const {
    vector<bool> used(n * n);
    vector<int> stack;
    for (int cell : state.order) {
      if (cell < 0 || cell >= n * n || used[cell]) return false;
      used[cell] = true;
      const int id = label[cell];
      if (!stack.empty() && stack.back() == id) stack.pop_back();
      else stack.push_back(id);
    }
    return stack.empty() && route_cost(state.order) == state.cost;
  }
  void print_answer(const State& answer) const {
    int row = 0, col = 0;
    for (int cell : answer.order) {
      const int target_row = cell / n, target_col = cell % n;
      while (row < target_row) { cout << "D\n"; ++row; }
      while (row > target_row) { cout << "U\n"; --row; }
      while (col < target_col) { cout << "R\n"; ++col; }
      while (col > target_col) { cout << "L\n"; --col; }
      cout << "Z\n";
    }
  }
};

#ifndef AHC059_LNS_NO_MAIN
int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);
  const auto started = chrono::steady_clock::now();
  CardPairProblem problem;
  problem.read_input();
  auto initial = problem.make_initial_state();
  const int initial_cost = initial.cost;
  LnsOptions options;
  options.maximize = false;
  options.time_limit_ms = max(1.0, AHC059_LNS_TIME_MS -
      chrono::duration<double, milli>(chrono::steady_clock::now() - started).count());
  options.seed = problem.seed;
  options.acceptance = static_cast<LnsAcceptance>(AHC059_LNS_MODE);
  options.start_margin = options.end_margin = AHC059_LNS_MARGIN;
  options.start_temperature = 8.0;
  options.end_temperature = 0.25;
  options.early_cutoff = AHC059_LNS_CUTOFF;
#ifdef AHC059_LNS_ITERATIONS
  options.iteration_limit = AHC059_LNS_ITERATIONS;
#endif
  LargeNeighborhoodSearch<CardPairProblem> search(problem, std::move(initial), initial_cost, options);
  if constexpr (AHC059_ALNS_POLICY == 0) {
    search.run();
  } else {
    AdaptiveOperatorOptions selection;
    selection.adaptive = AHC059_ALNS_POLICY == 2;
    AdaptiveOperatorSelector selector(5, selection);
    // 選択用乱数は近傍・採用判定用と分ける。壊し方の内部変更と干渉させない。
    mt19937_64 selection_rng(problem.seed ^ 0x8cb92baa3f3d8dd7ULL);
    array<uint64_t, 5> tried{};
    while (true) {
      problem.operator_id = selector.select(selection_rng);
      if (!search.step()) break; // 予算終了時は試行していないので報酬も記録しない。
      ++tried[problem.operator_id];
      double reward = 0;
      switch (search.last_outcome()) {
        case LnsOutcome::ImprovedBest: reward = 1.0; break;
        case LnsOutcome::ImprovedCurrent: reward = 0.5; break;
        case LnsOutcome::Accepted: reward = 0.1; break;
        case LnsOutcome::Rejected: break;
      }
      selector.record(problem.operator_id, reward);
    }
    for (int id = 0; id < 5; ++id) {
      cerr << "operator=" << id << " tried=" << tried[id]
           << " probability=" << selector.probability(id) << '\n';
    }
  }
  assert(problem.is_valid(search.best_state()));
  problem.print_answer(search.best_state());
  cerr << "iterations=" << search.iterations() << " accepted=" << search.accepted()
       << " pruned=" << search.rejected_repairs() << " initial_cost=" << initial_cost
       << " cost=" << search.best_score() << '\n';
}
#endif
