#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// 全候補の State を作らず、軽い Action だけを先に上位N個へ絞るビームサーチ。
// State のコピーは各世代で最大 beam_width 回だけ行う。
//
// 次のような時に向いている。
// - State が大きく、全候補ぶんコピーすると遅い
// - action から次状態の順位を差分計算できる
// - tree-beam-search.hpp の revert を書くのは難しい
//
// 使い方:
// ActionBeamSearch<State, Move, long long> beam(initial, 0, 200);
// for (int turn = 0; turn < turns; ++turn) {
//   if (!beam.step(
//           [](const State& state) { return state.actions(); },
//           [](const State& state, const Move& move) {
//             return state.score + move.score_delta;  // Stateを作らず順位計算
//           },
//           [](State& state, Move& move) { state.apply(move); })) {
//     break;
//   }
// }
// State answer = beam.best();
//
// expand(parent) は Action のコンテナを返す。
// evaluate_action(parent, action) は、そのaction適用後の順位値を返す。
// apply(child, action) は、コピー済みchildへactionを反映する。
// applyは選ばれた最大beam_width件にしか呼ばれない。
//
// 候補は2 * beam_width件たまるたび上位beam_width件へ縮める。
// 一度境界が分かった後は、境界以下の候補を保存しない。この選抜は近似ではなく、
// 全候補を最後にsortした場合と同じ上位N件を、同点なら生成順で返す。
// 浮動小数点ScoreのNaNは、最大化・最小化のどちらでも最下位にする。
template <class State, class Action, class Score>
struct ActionBeamSearch {
  ActionBeamSearch(State initial_state,
                   Score initial_score,
                   int beam_width,
                   bool maximize = true)
      : beam_width_(beam_width), maximize_(maximize) {
    if (beam_width <= 0) {
      throw std::invalid_argument("beam_width must be positive");
    }
    beam_.push_back(std::move(initial_state));
    scores_.push_back(std::move(initial_score));
    reserve_candidates(default_candidate_reserve());
  }

  // 1世代進める。Stateを作るapplyは、選抜後の候補だけに呼ばれる。
  template <class Expand, class EvaluateAction, class Apply>
  bool step(Expand&& expand,
            EvaluateAction&& evaluate_action,
            Apply&& apply) {
    NoObserver observer;
    return step_impl<false>(expand, evaluate_action, apply, observer);
  }

  // on_generated(parent_rank, parent, action, rank_score)を全候補へ
  // 選抜前に1回呼ぶ。軽量性を保つためchild Stateは作らない。
  // 早期terminalを保存する時はparentの履歴へactionを1個足して復元する。
  template <class Expand,
            class EvaluateAction,
            class Apply,
            class OnGenerated>
  bool step_and_observe(Expand&& expand,
                        EvaluateAction&& evaluate_action,
                        Apply&& apply,
                        OnGenerated&& on_generated) {
    return step_impl<true>(
        expand, evaluate_action, apply, on_generated);
  }

  // make_key(parent, action) が同じ候補は、最良の1件だけを残してから幅を絞る。
  // keyは、action適用後の状態を作らず計算できる値にする。
  // 重複除去とバッファcutoffは相性が複雑なので、この経路は全候補のActionを
  // 一度保存してから厳密に重複除去する。
  template <class Expand, class EvaluateAction, class MakeKey, class Apply>
  bool step_with_key(Expand&& expand,
                     EvaluateAction&& evaluate_action,
                     MakeKey&& make_key,
                     Apply&& apply) {
    using Key = std::decay_t<decltype(make_key(
        std::declval<const State&>(), std::declval<const Action&>()))>;
    return step_with_key_impl(
        std::forward<Expand>(expand),
        std::forward<EvaluateAction>(evaluate_action),
        std::forward<MakeKey>(make_key),
        std::hash<Key>{}, std::equal_to<Key>{},
        std::forward<Apply>(apply));
  }

  // 特徴bucketごとに最大max_per_bucket件を残してから、全体の幅を絞る。
  // 似た候補だけでビームが埋まる時の多様性確保に使う。
  // make_bucket(parent, action) は、盤面の一部や粗いhashなどを返す。
  template <class Expand, class EvaluateAction, class MakeBucket, class Apply>
  bool step_with_bucket_limit(Expand&& expand,
                              EvaluateAction&& evaluate_action,
                              MakeBucket&& make_bucket,
                              int max_per_bucket,
                              Apply&& apply) {
    using Bucket = std::decay_t<decltype(make_bucket(
        std::declval<const State&>(), std::declval<const Action&>()))>;
    return step_with_bucket_limit_impl(
        std::forward<Expand>(expand),
        std::forward<EvaluateAction>(evaluate_action),
        std::forward<MakeBucket>(make_bucket), max_per_bucket,
        std::hash<Bucket>{}, std::equal_to<Bucket>{},
        std::forward<Apply>(apply));
  }

  // std::hash<Bucket>がない型では、hashと等値比較も直接渡す。
  template <class Expand,
            class EvaluateAction,
            class MakeBucket,
            class Hash,
            class KeyEqual,
            class Apply>
  bool step_with_bucket_limit(Expand&& expand,
                              EvaluateAction&& evaluate_action,
                              MakeBucket&& make_bucket,
                              int max_per_bucket,
                              Hash&& hash,
                              KeyEqual&& key_equal,
                              Apply&& apply) {
    return step_with_bucket_limit_impl(
        std::forward<Expand>(expand),
        std::forward<EvaluateAction>(evaluate_action),
        std::forward<MakeBucket>(make_bucket), max_per_bucket,
        std::forward<Hash>(hash), std::forward<KeyEqual>(key_equal),
        std::forward<Apply>(apply));
  }

  // std::hash<Key>がない型では、hashと等値比較を直接渡す。
  template <class Expand,
            class EvaluateAction,
            class MakeKey,
            class Hash,
            class KeyEqual,
            class Apply>
  bool step_with_key(Expand&& expand,
                     EvaluateAction&& evaluate_action,
                     MakeKey&& make_key,
                     Hash&& hash,
                     KeyEqual&& key_equal,
                     Apply&& apply) {
    return step_with_key_impl(
        std::forward<Expand>(expand),
        std::forward<EvaluateAction>(evaluate_action),
        std::forward<MakeKey>(make_key),
        std::forward<Hash>(hash), std::forward<KeyEqual>(key_equal),
        std::forward<Apply>(apply));
  }

  const std::vector<State>& states() const { return beam_; }

  const std::vector<Score>& scores() const { return scores_; }

  const State& best() const {
    assert(!beam_.empty());
    return beam_.front();
  }

  // 探索途中に書き換えると、保存済みscoreと不整合になることがある。
  // 通常は探索終了後だけ使う。
  State& best() {
    assert(!beam_.empty());
    return beam_.front();
  }

  const Score& best_score() const {
    assert(!scores_.empty());
    return scores_.front();
  }

  std::size_t size() const { return beam_.size(); }

  int depth() const { return depth_; }

  int width() const { return beam_width_; }

  // 直近stepの件数。key/bucket制限なしではgenerated == unique。
  // bucket制限時のuniqueは、bucketごとの上限を適用した後の候補数。
  // buffered_peakはAction候補bufferが同時に保持した最大件数。
  std::size_t last_generated_count() const { return last_generated_count_; }
  std::size_t last_unique_count() const { return last_unique_count_; }
  std::size_t last_kept_count() const { return last_kept_count_; }
  std::size_t last_buffered_peak_count() const {
    return last_buffered_peak_count_;
  }

  // 既定は2*width件ごとに縮める。候補が生成順にずっと改善する場合は
  // 中間選抜が増えるため、falseにして最後のnth_element 1回と実測比較できる。
  // どちらを選んでも最終結果は同じ。
  void set_batched_selection(bool enabled) {
    batched_selection_ = enabled;
  }

  bool batched_selection() const { return batched_selection_; }

  // 1層の候補数が分かる時に使う。通常のstepは2*width以上を同時に
  // 保持しない。step_with_keyでは重複除去前の候補数が目安。
  void reserve_candidates(std::size_t count) {
    candidates_.reserve(count);
    scratch_candidates_.reserve(count);
    candidate_ids_.reserve(count);
  }

  // 幅を小さくした時は現在の層もその場で切り詰める。
  void set_width(int beam_width) {
    if (beam_width <= 0) {
      throw std::invalid_argument("beam_width must be positive");
    }
    beam_width_ = beam_width;
    while (beam_.size() > static_cast<std::size_t>(beam_width_)) {
      beam_.pop_back();
      scores_.pop_back();
    }
  }

  // 新しい初期状態から使い直す。確保済みbufferは再利用する。
  void reset(State initial_state, Score initial_score) {
    beam_.clear();
    scores_.clear();
    beam_.push_back(std::move(initial_state));
    scores_.push_back(std::move(initial_score));
    next_beam_.clear();
    next_scores_.clear();
    candidates_.clear();
    scratch_candidates_.clear();
    candidate_ids_.clear();
    depth_ = 0;
    clear_last_counts();
  }

  // 現在のbeamだけ残し、作業bufferを解放する。
  void release_memory() {
    std::vector<State>().swap(next_beam_);
    std::vector<Score>().swap(next_scores_);
    std::vector<Candidate>().swap(candidates_);
    std::vector<Candidate>().swap(scratch_candidates_);
    std::vector<std::size_t>().swap(candidate_ids_);
    beam_.shrink_to_fit();
    scores_.shrink_to_fit();
  }

 private:
  struct NoObserver {};

  struct Candidate {
    std::size_t parent;
    Action action;
    Score score;
    std::size_t order;
  };

  int beam_width_;
  bool maximize_;
  int depth_ = 0;
  std::vector<State> beam_;
  std::vector<Score> scores_;
  std::vector<State> next_beam_;
  std::vector<Score> next_scores_;
  std::vector<Candidate> candidates_;
  std::vector<Candidate> scratch_candidates_;
  std::vector<std::size_t> candidate_ids_;
  bool batched_selection_ = true;
  bool cutoff_ready_ = false;
  std::size_t last_generated_count_ = 0;
  std::size_t last_unique_count_ = 0;
  std::size_t last_kept_count_ = 0;
  std::size_t last_buffered_peak_count_ = 0;

  std::size_t default_candidate_reserve() const {
    const std::size_t width = static_cast<std::size_t>(beam_width_);
    if (width > std::numeric_limits<std::size_t>::max() / 2) return width;
    return width * 2;
  }

  void clear_last_counts() {
    last_generated_count_ = 0;
    last_unique_count_ = 0;
    last_kept_count_ = 0;
    last_buffered_peak_count_ = 0;
  }

  void begin_step() {
    candidates_.clear();
    scratch_candidates_.clear();
    candidate_ids_.clear();
    cutoff_ready_ = false;
    clear_last_counts();
  }

  bool score_is_nan(const Score& value) const {
    if constexpr (std::is_floating_point_v<Score>) {
      return std::isnan(value);
    } else {
      static_cast<void>(value);
      return false;
    }
  }

  bool score_is_better(const Score& a, const Score& b) const {
    const bool a_nan = score_is_nan(a);
    const bool b_nan = score_is_nan(b);
    if (a_nan != b_nan) return !a_nan;
    if (a_nan) return false;
    return maximize_ ? b < a : a < b;
  }

  bool candidate_is_better(const Candidate& a, const Candidate& b) const {
    if (score_is_better(a.score, b.score)) return true;
    if (score_is_better(b.score, a.score)) return false;
    return a.order < b.order;
  }

  std::size_t batch_limit() const {
    const std::size_t width = static_cast<std::size_t>(beam_width_);
    if (width > std::numeric_limits<std::size_t>::max() / 2) {
      return std::numeric_limits<std::size_t>::max();
    }
    return width * 2;
  }

  void add_unkeyed_candidate(Candidate candidate) {
    const std::size_t width = static_cast<std::size_t>(beam_width_);
    // candidates_[width - 1] は直近cutoff時点の最下位。新しい候補を
    // 追加しても真の境界は良くなるだけなので、この古い境界で落とすのは安全。
    if (batched_selection_ && cutoff_ready_ &&
        !candidate_is_better(candidate, candidates_[width - 1])) {
      return;
    }

    candidates_.push_back(std::move(candidate));
    last_buffered_peak_count_ =
        std::max(last_buffered_peak_count_, candidates_.size());
    if (batched_selection_ && candidates_.size() >= batch_limit()) {
      keep_best_candidates(width);
      cutoff_ready_ = true;
    }
  }

  template <bool Observe,
            class Expand,
            class EvaluateAction,
            class Apply,
            class OnGenerated>
  bool step_impl(Expand& expand,
                 EvaluateAction& evaluate_action,
                 Apply& apply,
                 OnGenerated& on_generated) {
    begin_step();
    std::size_t order = 0;
    for (std::size_t parent = 0; parent < beam_.size(); ++parent) {
      auto&& actions = expand(static_cast<const State&>(beam_[parent]));
      for (auto&& expanded_action : actions) {
        Action action = std::move(expanded_action);
        Score score = evaluate_action(
            static_cast<const State&>(beam_[parent]),
            static_cast<const Action&>(action));
        if constexpr (Observe) {
          on_generated(
              parent,
              static_cast<const State&>(beam_[parent]),
              static_cast<const Action&>(action),
              static_cast<const Score&>(score));
        }
        ++last_generated_count_;
        add_unkeyed_candidate(
            Candidate{parent, std::move(action), std::move(score), order++});
      }
    }
    last_unique_count_ = last_generated_count_;
    return finish_step(apply);
  }

  // candidates_を良い順の上位kept件へ縮める。
  // 小さいIDを選ぶため、選抜中にActionやScoreを何度もswapしない。
  void keep_best_candidates(std::size_t kept) {
    kept = std::min(kept, candidates_.size());
    candidate_ids_.resize(candidates_.size());
    std::iota(candidate_ids_.begin(), candidate_ids_.end(), std::size_t{0});
    const auto better_id = [&](std::size_t a, std::size_t b) {
      return candidate_is_better(candidates_[a], candidates_[b]);
    };
    if (kept < candidate_ids_.size()) {
      std::nth_element(candidate_ids_.begin(),
                       candidate_ids_.begin() + kept,
                       candidate_ids_.end(), better_id);
      candidate_ids_.resize(kept);
    }
    std::sort(candidate_ids_.begin(), candidate_ids_.end(), better_id);

    scratch_candidates_.clear();
    scratch_candidates_.reserve(std::max(scratch_candidates_.capacity(), kept));
    for (std::size_t id : candidate_ids_) {
      scratch_candidates_.push_back(std::move(candidates_[id]));
    }
    candidates_.swap(scratch_candidates_);
    scratch_candidates_.clear();
    candidate_ids_.clear();
  }

  // candidate_ids_で指定した候補だけを、現在のID順で残す。
  void keep_candidates_by_id() {
    scratch_candidates_.clear();
    scratch_candidates_.reserve(candidate_ids_.size());
    for (std::size_t id : candidate_ids_) {
      scratch_candidates_.push_back(std::move(candidates_[id]));
    }
    candidates_.swap(scratch_candidates_);
    scratch_candidates_.clear();
    candidate_ids_.clear();
  }

  void select_candidate_ids() {
    const auto better_id = [&](std::size_t a, std::size_t b) {
      return candidate_is_better(candidates_[a], candidates_[b]);
    };
    const std::size_t kept = std::min(
        static_cast<std::size_t>(beam_width_), candidate_ids_.size());
    if (kept < candidate_ids_.size()) {
      std::nth_element(candidate_ids_.begin(),
                       candidate_ids_.begin() + kept,
                       candidate_ids_.end(), better_id);
      candidate_ids_.resize(kept);
    }
    std::sort(candidate_ids_.begin(), candidate_ids_.end(), better_id);
    keep_candidates_by_id();
  }

  template <class Apply>
  bool finish_step(Apply& apply) {
    if (candidates_.empty()) return false;
    keep_best_candidates(static_cast<std::size_t>(beam_width_));
    last_kept_count_ = candidates_.size();

    next_beam_.clear();
    next_scores_.clear();
    next_beam_.reserve(candidates_.size());
    next_scores_.reserve(candidates_.size());
    for (Candidate& candidate : candidates_) {
      State child(beam_[candidate.parent]);
      apply(child, candidate.action);
      next_beam_.push_back(std::move(child));
      next_scores_.push_back(std::move(candidate.score));
    }

    beam_.swap(next_beam_);
    scores_.swap(next_scores_);
    next_beam_.clear();
    next_scores_.clear();
    candidates_.clear();
    ++depth_;
    return true;
  }

  template <class Expand,
            class EvaluateAction,
            class MakeKey,
            class Hash,
            class KeyEqual,
            class Apply>
  bool step_with_key_impl(Expand&& expand,
                          EvaluateAction&& evaluate_action,
                          MakeKey&& make_key,
                          Hash&& hash,
                          KeyEqual&& key_equal,
                          Apply&& apply) {
    using Key = std::decay_t<decltype(make_key(
        std::declval<const State&>(), std::declval<const Action&>()))>;
    using HashType = std::decay_t<Hash>;
    using KeyEqualType = std::decay_t<KeyEqual>;

    begin_step();
    std::vector<Key> keys;
    keys.reserve(candidates_.capacity());
    std::size_t order = 0;
    for (std::size_t parent = 0; parent < beam_.size(); ++parent) {
      auto&& actions = expand(static_cast<const State&>(beam_[parent]));
      for (auto&& expanded_action : actions) {
        Action action = std::move(expanded_action);
        Score score = evaluate_action(
            static_cast<const State&>(beam_[parent]),
            static_cast<const Action&>(action));
        keys.push_back(make_key(
            static_cast<const State&>(beam_[parent]),
            static_cast<const Action&>(action)));
        candidates_.push_back(
            Candidate{parent, std::move(action), std::move(score), order++});
        ++last_generated_count_;
        last_buffered_peak_count_ =
            std::max(last_buffered_peak_count_, candidates_.size());
      }
    }
    if (candidates_.empty()) return false;

    std::unordered_map<Key, std::size_t, HashType, KeyEqualType> best_by_key(
        0, std::forward<Hash>(hash), std::forward<KeyEqual>(key_equal));
    best_by_key.reserve(candidates_.size());
    for (std::size_t i = 0; i < candidates_.size(); ++i) {
      const auto found = best_by_key.find(keys[i]);
      if (found == best_by_key.end()) {
        best_by_key.emplace(std::move(keys[i]), i);
      } else if (candidate_is_better(
                     candidates_[i], candidates_[found->second])) {
        found->second = i;
      }
    }

    candidate_ids_.clear();
    candidate_ids_.reserve(best_by_key.size());
    for (const auto& entry : best_by_key) candidate_ids_.push_back(entry.second);
    last_unique_count_ = candidate_ids_.size();
    select_candidate_ids();
    return finish_step(apply);
  }

  template <class Expand,
            class EvaluateAction,
            class MakeBucket,
            class Hash,
            class KeyEqual,
            class Apply>
  bool step_with_bucket_limit_impl(Expand&& expand,
                                   EvaluateAction&& evaluate_action,
                                   MakeBucket&& make_bucket,
                                   int max_per_bucket,
                                   Hash&& hash,
                                   KeyEqual&& key_equal,
                                   Apply&& apply) {
    if (max_per_bucket <= 0) {
      throw std::invalid_argument("max_per_bucket must be positive");
    }
    using Bucket = std::decay_t<decltype(make_bucket(
        std::declval<const State&>(), std::declval<const Action&>()))>;
    using HashType = std::decay_t<Hash>;
    using KeyEqualType = std::decay_t<KeyEqual>;

    begin_step();
    std::vector<Bucket> bucket_keys;
    bucket_keys.reserve(candidates_.capacity());
    std::size_t order = 0;
    for (std::size_t parent = 0; parent < beam_.size(); ++parent) {
      auto&& actions = expand(static_cast<const State&>(beam_[parent]));
      for (auto&& expanded_action : actions) {
        Action action = std::move(expanded_action);
        Score score = evaluate_action(
            static_cast<const State&>(beam_[parent]),
            static_cast<const Action&>(action));
        bucket_keys.push_back(make_bucket(
            static_cast<const State&>(beam_[parent]),
            static_cast<const Action&>(action)));
        candidates_.push_back(
            Candidate{parent, std::move(action), std::move(score), order++});
        ++last_generated_count_;
        last_buffered_peak_count_ =
            std::max(last_buffered_peak_count_, candidates_.size());
      }
    }
    if (candidates_.empty()) return false;

    std::unordered_map<Bucket,
                       std::vector<std::size_t>,
                       HashType,
                       KeyEqualType> by_bucket(
        0, std::forward<Hash>(hash), std::forward<KeyEqual>(key_equal));
    by_bucket.reserve(candidates_.size());
    const std::size_t bucket_limit = std::min(
        static_cast<std::size_t>(max_per_bucket),
        static_cast<std::size_t>(beam_width_));
    for (std::size_t i = 0; i < candidates_.size(); ++i) {
      auto found = by_bucket.find(bucket_keys[i]);
      if (found == by_bucket.end()) {
        found = by_bucket.emplace(
            std::move(bucket_keys[i]), std::vector<std::size_t>{}).first;
        found->second.reserve(bucket_limit);
      }

      std::vector<std::size_t>& entries = found->second;
      if (entries.size() < bucket_limit) {
        entries.push_back(i);
        continue;
      }

      std::size_t worst = 0;
      for (std::size_t j = 1; j < entries.size(); ++j) {
        if (candidate_is_better(
                candidates_[entries[worst]], candidates_[entries[j]])) {
          worst = j;
        }
      }
      if (candidate_is_better(candidates_[i], candidates_[entries[worst]])) {
        entries[worst] = i;
      }
    }

    candidate_ids_.clear();
    for (const auto& bucket : by_bucket) {
      for (std::size_t id : bucket.second) candidate_ids_.push_back(id);
    }
    last_unique_count_ = candidate_ids_.size();
    select_candidate_ids();
    return finish_step(apply);
  }
};
