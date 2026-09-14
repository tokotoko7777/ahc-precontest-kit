// BEGIN ahc-precontest-kit: library/action-beam-search.hpp
// Source: https://github.com/tokotoko7777/ahc-precontest-kit/blob/ef1ad633dbb4053ce2b91acefe0da7e35a6acfd3/library/action-beam-search.hpp
// SHA-256: ab4749354e16e1d9182d7324983279d09ce4586f1123b26a894ffb844932d75e
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/action-beam-search.hpp

// 全候補の State を作らず、軽い Action だけを先に上位N個へ絞るビームサーチ。
// State のコピーは各世代で最大 beam_width 回だけ行う。
//
// 次のような時に向いている。
// - State が大きく、全候補ぶんコピーすると遅い
// - action から次状態の順位を差分計算できる
// - tree-beam-search.hpp の revert を書くのは難しい
//
// 問題ごとのコードを1か所にまとめる使い方。
// 下のコメントは、その項目に「何を入れ、何を返すか」を示している。
// 空関数を配置済みの雛形: template/search/action-beam.cpp
// struct Problem {
//   // TODO: 【問題ごと】探索途中の解1個を表すStateを書く。
//   // State = 探索途中の解を1個だけ表す型。
//   // 盤面、現在ターン、使用回数、途中得点、答えの操作列など、
//   // Actionを1回適用して次へ進むために必要な「変化する情報」を入れる。
//   // 入力データのように全Stateで共通の情報は、コピーを避けるためProblem側に置く。
//   using State = MyState;
//
//   // TODO: 【問題ごと】次の1手だけを表す軽いActionを書く。
//   // Action = Stateを1手進めるための軽い情報。
//   // 例: 選ぶ頂点番号、(置く場所, 向き)、近傍操作の種類。
//   // 次の盤面全体を入れる必要はない。全候補ぶん保存されるため小さいほど速い。
//   using Action = MyMove;
//
//   // TODO: 【問題ごと】候補順位の型を選ぶ。
//   // Score = ビーム内で候補の良さを比較する数値型。
//   // 既定では大きい値ほど良い。小さい値を良くする時はRunner構築時にfalseを渡す。
//   using Score = long long;
//
//   // TODO: 【問題ごと】stateから試せるActionを全て返す。
//   // stateから次に試せるActionを全て返す。空なら、そのStateは行き止まり。
//   // vector/arrayなどを値で返しても、Problemが持つコンテナをconst参照で返してもよい。
//   vector<Action> generate_actions(const State& state) {
//     return make_legal_moves(state);
//   }
//
//   // TODO: 【問題ごと】action適用後の順位値そのものを差分計算する。
//   // actionを適用した「後」の子Stateを並べるためのScoreを返す。
//   // 差分だけでなく、子Stateの順位値そのものを返すことに注意。
//   // 全候補に呼ばれるため、stateを変更せず、できればO(1)の差分計算にする。
//   Score evaluate_action(const State& state, const Action& action) {
//     return state.rank_score + calculate_delta(state, action);
//   }
//
//   // TODO: 【問題ごと】採用されたactionをコピー済みStateへ反映する。
//   // 採用されたActionを、親からコピー済みのstateへ本当に反映する。
//   // 盤面だけでなく、ターン、得点、hash、使用回数、操作履歴もここで更新する。
//   // evaluate_actionと同じ子状態・同じ順位になるように書く。
//   void apply_action(State& state, Action& action) {
//     apply_move(state, action);
//   }
// };
// Problem problem;
// // 第3引数はinitialの順位値、第4引数はビーム幅。
// ActionBeamRunner<Problem> beam(problem, initial, initial.rank_score, 200);
// beam.run(turns);
// MyState answer = beam.best();
// ↑↑↑ TODOが付いた箇所だけ問題ごとに書く。ライブラリ本体は通常編集しない。↑↑↑
//
// 下のActionBeamSearchを直接使う場合、expand(parent)はActionのコンテナ、
// evaluate_action(parent, action)はaction適用後の順位値を返す。
// apply(child, action)はコピー済みchildへactionを反映し、
// 選ばれた最大beam_width件にしか呼ばれない。
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

  // 重い順位計算を、現在の上位beam_width件の境界で途中終了できる版。
  // evaluate_action_with_threshold(parent, action, threshold) は
  // optional<Score>を返す。threshold==nullptrなら境界はまだ未確定なので、
  // 必ず正確なScoreを返す。非nullなら、候補が境界を厳密に超えないと証明できた
  // 時だけnulloptを返してよい。最大化/最小化は構築時の指定に従う。
  //
  // nulloptは近似枝刈りではない。判定できない時は最後まで計算してScoreを返せば、
  // 通常のstepと同じ結果になる。同点は先に生成された候補を優先するため、後発候補は
  // 境界と同点でも枝刈りしてよい。keyによる重複除去とは併用しない。
  template <class Expand, class EvaluateActionWithThreshold, class Apply>
  bool step_with_threshold(
      Expand&& expand,
      EvaluateActionWithThreshold&& evaluate_action_with_threshold,
      Apply&& apply) {
    return step_with_threshold_impl(
        expand, evaluate_action_with_threshold, apply);
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
  std::size_t last_threshold_pruned_count() const {
    return last_threshold_pruned_count_;
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
  std::size_t last_threshold_pruned_count_ = 0;

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
    last_threshold_pruned_count_ = 0;
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

  template <class Expand, class EvaluateActionWithThreshold, class Apply>
  bool step_with_threshold_impl(
      Expand& expand,
      EvaluateActionWithThreshold& evaluate_action_with_threshold,
      Apply& apply) {
    begin_step();
    const std::size_t width = static_cast<std::size_t>(beam_width_);
    std::size_t order = 0;
    for (std::size_t parent = 0; parent < beam_.size(); ++parent) {
      auto&& actions = expand(static_cast<const State&>(beam_[parent]));
      for (auto&& expanded_action : actions) {
        Action action = std::move(expanded_action);
        const Score* threshold = nullptr;
        if (batched_selection_ && cutoff_ready_) {
          threshold = &candidates_[width - 1].score;
        }

        ++last_generated_count_;
        auto score = evaluate_action_with_threshold(
            static_cast<const State&>(beam_[parent]),
            static_cast<const Action&>(action), threshold);
        const std::size_t candidate_order = order++;
        if (!score.has_value()) {
          ++last_threshold_pruned_count_;
          continue;
        }
        add_unkeyed_candidate(Candidate{
            parent, std::move(action), std::move(*score), candidate_order});

        // 最初のN件がそろった時点で境界を作る。以後の重い評価はこの境界を
        // 利用できる。古い境界は真の境界以下(最小化なら以上)なので安全。
        if (batched_selection_ && !cutoff_ready_ &&
            candidates_.size() >= width) {
          keep_best_candidates(width);
          cutoff_ready_ = true;
        }
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

// 問題依存コードをProblemへ集めるための薄いラッパー。
// ビーム選抜の実装を変更せず、Problemの次の3関数だけを呼ぶ。
//
// 必須:
//   generate_actions(const State&)
//     -> そのStateから試すActionのコンテナ。空なら行き止まり。
//   evaluate_action(const State&, const Action&)
//     -> Action適用後の子Stateの順位値。差分値ではなくScoreそのもの。
//   apply_action(State&, Action&)
//     -> 親からコピー済みのStateを、Action適用後の子Stateへ変更する。
//
// 任意:
//   evaluate_action_with_threshold(const State&, const Action&, const Score*)
//     -> 重い評価を境界で安全に中断するstep_with_threshold用。optional<Score>。
//   make_key(const State&, const Action&)
//     -> Action適用後の同一局面を表す値。step_with_key用。
//   make_bucket(const State&, const Action&)
//     -> 似た候補を同じ組にする粗い特徴。step_with_bucket_limit用。
//
// 入力、出力、State、Action、評価、状態更新はProblem側に置く。
// Runner側はターンループ、候補選抜、Stateコピー、幅、統計を担当する。
template <class Problem>
struct ActionBeamRunner {
  using State = typename Problem::State;
  using Action = typename Problem::Action;
  using Score = typename Problem::Score;

  ActionBeamRunner(Problem& problem,
                   State initial_state,
                   Score initial_score,
                   int beam_width,
                   bool maximize = true)
      : problem_(problem),
        beam_(std::move(initial_state),
              std::move(initial_score),
              beam_width,
              maximize) {}

  bool step() {
    return beam_.step(
        [&](const State& state) -> decltype(auto) {
          return problem_.generate_actions(state);
        },
        [&](const State& state, const Action& action) {
          return problem_.evaluate_action(state, action);
        },
        [&](State& state, Action& action) {
          problem_.apply_action(state, action);
        });
  }

  // Problem::evaluate_action_with_thresholdはoptional<Score>を返す。
  // thresholdが非nullなら現在の採用境界。超えられないと証明できた時だけ
  // nulloptを返す。分からない時は正確なScoreを返せばよい。
  bool step_with_threshold() {
    return beam_.step_with_threshold(
        [&](const State& state) -> decltype(auto) {
          return problem_.generate_actions(state);
        },
        [&](const State& state,
            const Action& action,
            const Score* threshold) {
          return problem_.evaluate_action_with_threshold(
              state, action, threshold);
        },
        [&](State& state, Action& action) {
          problem_.apply_action(state, action);
        });
  }

  // observer(parent_rank, parent, action, rank_score)を全候補へ呼ぶ。
  template <class OnGenerated>
  bool step_and_observe(OnGenerated&& observer) {
    return beam_.step_and_observe(
        [&](const State& state) -> decltype(auto) {
          return problem_.generate_actions(state);
        },
        [&](const State& state, const Action& action) {
          return problem_.evaluate_action(state, action);
        },
        [&](State& state, Action& action) {
          problem_.apply_action(state, action);
        },
        std::forward<OnGenerated>(observer));
  }

  // Problem::make_keyが同じ候補を1件にまとめる。
  bool step_with_key() {
    return beam_.step_with_key(
        [&](const State& state) -> decltype(auto) {
          return problem_.generate_actions(state);
        },
        [&](const State& state, const Action& action) {
          return problem_.evaluate_action(state, action);
        },
        [&](const State& state, const Action& action) {
          return problem_.make_key(state, action);
        },
        [&](State& state, Action& action) {
          problem_.apply_action(state, action);
        });
  }

  // Problem::make_bucketごとに最大max_per_bucket件を残す。
  bool step_with_bucket_limit(int max_per_bucket) {
    return beam_.step_with_bucket_limit(
        [&](const State& state) -> decltype(auto) {
          return problem_.generate_actions(state);
        },
        [&](const State& state, const Action& action) {
          return problem_.evaluate_action(state, action);
        },
        [&](const State& state, const Action& action) {
          return problem_.make_bucket(state, action);
        },
        max_per_bucket,
        [&](State& state, Action& action) {
          problem_.apply_action(state, action);
        });
  }

  // 最大turns回進め、実際に進んだ回数を返す。
  int run(int turns) {
    if (turns < 0) {
      throw std::invalid_argument("turns must be non-negative");
    }
    int advanced = 0;
    while (advanced < turns && step()) ++advanced;
    return advanced;
  }

  int run_with_threshold(int turns) {
    if (turns < 0) {
      throw std::invalid_argument("turns must be non-negative");
    }
    int advanced = 0;
    while (advanced < turns && step_with_threshold()) ++advanced;
    return advanced;
  }

  int run_with_key(int turns) {
    if (turns < 0) {
      throw std::invalid_argument("turns must be non-negative");
    }
    int advanced = 0;
    while (advanced < turns && step_with_key()) ++advanced;
    return advanced;
  }

  int run_with_bucket_limit(int turns, int max_per_bucket) {
    if (turns < 0) {
      throw std::invalid_argument("turns must be non-negative");
    }
    int advanced = 0;
    while (advanced < turns &&
           step_with_bucket_limit(max_per_bucket)) {
      ++advanced;
    }
    return advanced;
  }

  const std::vector<State>& states() const { return beam_.states(); }
  const std::vector<Score>& scores() const { return beam_.scores(); }
  const State& best() const { return beam_.best(); }
  State& best() { return beam_.best(); }
  const Score& best_score() const { return beam_.best_score(); }
  std::size_t size() const { return beam_.size(); }
  int depth() const { return beam_.depth(); }
  int width() const { return beam_.width(); }

  void set_width(int beam_width) { beam_.set_width(beam_width); }
  void set_batched_selection(bool enabled) {
    beam_.set_batched_selection(enabled);
  }
  bool batched_selection() const { return beam_.batched_selection(); }
  void reserve_candidates(std::size_t count) {
    beam_.reserve_candidates(count);
  }
  void reset(State initial_state, Score initial_score) {
    beam_.reset(std::move(initial_state), std::move(initial_score));
  }
  void release_memory() { beam_.release_memory(); }

  std::size_t last_generated_count() const {
    return beam_.last_generated_count();
  }
  std::size_t last_unique_count() const {
    return beam_.last_unique_count();
  }
  std::size_t last_kept_count() const {
    return beam_.last_kept_count();
  }
  std::size_t last_buffered_peak_count() const {
    return beam_.last_buffered_peak_count();
  }
  std::size_t last_threshold_pruned_count() const {
    return beam_.last_threshold_pruned_count();
  }

 private:
  Problem& problem_;
  ActionBeamSearch<State, Action, Score> beam_;
};
// END ahc-precontest-kit: library/action-beam-search.hpp

// BEGIN ahc-precontest-kit: library/simulated-annealing.hpp
// Source: https://github.com/tokotoko7777/ahc-precontest-kit/blob/ef1ad633dbb4053ce2b91acefe0da7e35a6acfd3/library/simulated-annealing.hpp
// SHA-256: 12f7511c39c98196f8b4e43392350810b673e415c94150ac9f3a35302c9ebe46
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/simulated-annealing.hpp

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
  mutable double log_start_temperature = 0.0;
  mutable double log_temperature_ratio = 0.0;
  double cached_progress_value = 0.0;
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

  bool accept_worsening(double exponent) {
    // 従来と同じく、悪化手では必ず乱数をちょうど1個消費する。
    const double random_value = random_01();
    // exp(-37) は random_01() の最小の正値 2^-53 より小さい。
    // random_value==0 の時だけunderflowを含めて従来式で確認する。
    if (exponent <= -37.0 && random_value != 0.0) return false;
    return random_value < std::exp(exponent);
  }

 public:
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

  double temperature(double progress) const {
    if (std::isnan(progress)) {
      throw std::invalid_argument("progress must not be NaN");
    }
    progress = std::clamp(progress, 0.0, 1.0);
    synchronize_temperature_settings();
    return temperature_from_prepared_settings(progress);
  }

  // 同じ progress を繰り返し渡しても温度は最初の1回しか計算しない。
  void set_progress(double progress) {
    if (std::isnan(progress)) {
      throw std::invalid_argument("progress must not be NaN");
    }
    progress = std::clamp(progress, 0.0, 1.0);
    synchronize_temperature_settings();
    if (progress == cached_progress_value) return;
    cached_progress_value = progress;
    refresh_cached_temperature();
  }

  double cached_progress() const { return cached_progress_value; }

  double current_temperature() const {
    synchronize_temperature_settings();
    return cached_temperature_value;
  }

  // 現在の温度で、この得点差を採用する確率。温度調整の確認用。
  template <class Score>
  double acceptance_probability(Score improvement) const {
    const double value = static_cast<double>(improvement);
    if (std::isnan(value)) return 0.0;
    if (value >= 0.0) return 1.0;
    synchronize_temperature_settings();
    return std::exp(acceptance_exponent(value));
  }

  template <class Score>
  double acceptance_probability(Score improvement, double progress) const {
    const double value = static_cast<double>(improvement);
    const double selected_temperature = temperature(progress);
    if (std::isnan(value)) return 0.0;
    if (value >= 0.0) return 1.0;
    return std::exp(value / selected_temperature);
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
  // uniform_real_distribution を熱いループで毎回作らない。
  double random_01() {
    constexpr double inverse = 1.0 / 9007199254740992.0;  // 2^53
    return static_cast<double>(engine() >> 11) * inverse;
  }

  // この試行が採用されるために必要な最小improvementを先に乱数で決める。
  // 戻り値は必ず0以下で、通常のacceptと同じ条件は
  //   improvement > draw_acceptance_threshold()
  // になる。重い差分計算へこの値を渡すと、「ここから計算しても閾値を
  // 超えない」と分かった時点で安全に打ち切れる。
  // このAPIは良化手を含む全試行で乱数を1個消費するため、accept()と乱数列は
  // 一致しないが、各手の採用確率は同じ。
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

  template <class Score>
  bool accept(Score improvement, double progress) {
    set_progress(progress);
    return accept(improvement);
  }
};
// END ahc-precontest-kit: library/simulated-annealing.hpp

#include <bits/stdc++.h>
using namespace std;

// 提出時は、この2行を各hppの全文へ置き換える。

// Pre-contest public bundled solver source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/practice/ahc071/main.cpp
// Editable source:
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/examples/search/ahc071_action_beam.cpp
// Official problem: https://atcoder.jp/contests/ahc071/tasks/ahc071_a

// AHC071「Wall Making」用の実例。
// 上の段に置いた各レンガは、中心1マスだけが下の段に支えられていればよい。
// したがって上から下へ作る時、次の段へ渡す情報は「直上段の中心bitset」だけ。
//
// 問題ごとに考える部分は TODO(AHC071) と書いた。ビームの上位N件選択、
// 同一状態の重複除去、採用候補だけのStateコピーはActionBeamRunnerが担当する。
using Mask = uint64_t;
constexpr int MAX_W = 60;
constexpr int MAX_ROW_CANDIDATES = 32;
constexpr float INF = 1e30f;

#ifndef AHC071_TIME_LIMIT
#define AHC071_TIME_LIMIT 1.80
#endif
#ifndef AHC071_BEAM_WIDTH
#define AHC071_BEAM_WIDTH 64
#endif
#ifndef AHC071_ROW_CANDIDATES
#define AHC071_ROW_CANDIDATES 8
#endif

struct Brick {
  int x;
  int y;
  int width;
};

struct Row {
  // starts[t]のx bitが1なら、(x, y)から幅2*t+1のレンガを置く。
  array<Mask, 5> starts{};
  Mask centers = 0;
  int cost = 0;

  Mask covered() const {
    Mask result = 0;
    for (int t = 0; t < 5; ++t) {
      for (int dx = 0; dx <= 2 * t; ++dx) result |= starts[t] << dx;
    }
    return result;
  }
};

struct Solver {
  // TODO(AHC071): 入力と、全Stateで共通の事前計算をここへ置く。
  int W = 0;
  int H = 0;
  int K = 0;
  array<int, 5> costs{};
  vector<Mask> holes;
  vector<array<float, MAX_W>> potential;
  vector<unordered_map<Mask, vector<array<float, MAX_W>>>> local_cache;
  array<float, MAX_W> zero_weight{};
  Mask full = 0;

  mt19937 random_engine{712367821};
  chrono::steady_clock::time_point started;
  double time_limit = AHC071_TIME_LIMIT;
  int beam_width = AHC071_BEAM_WIDTH;
  int row_candidate_count = AHC071_ROW_CANDIDATES;
  double initial_ratio = 0.25;
  int min_rebuild_height = 3;
  int max_rebuild_height = 12;
  bool use_lns = true;

  double elapsed() const {
    return chrono::duration<double>(
               chrono::steady_clock::now() - started)
        .count();
  }

  void read_input() {
    cin >> W >> H >> K;
    for (int& cost : costs) cin >> cost;
    full = (Mask(1) << W) - 1;
    holes.assign(H, 0);
    for (int i = 0; i < K; ++i) {
      int x, y;
      cin >> x >> y;
      holes[y] |= Mask(1) << x;
    }
  }

  // TODO(AHC071): 1行の最適化。
  // requiredを全て覆い、各レンガの中心がallowedに含まれる配置の最小値を返す。
  // weight[center]は「その中心を次の段でも支える将来費用」の近似値。
  float row_dp(Mask required,
               const array<float, MAX_W>& weight,
               Mask allowed,
               Row* answer = nullptr) const {
    float dp[MAX_W + 1];
    int next_required[MAX_W + 1];
    signed char take_type[MAX_W];
    dp[W] = 0;
    next_required[W] = W;
    for (int p = W - 1; p >= 0; --p) {
      const int first = next_required[p] =
          ((required >> p) & 1) ? p : next_required[p + 1];
      dp[p] = first == p ? INF : dp[p + 1];
      take_type[p] = -1;
      if (first == W) {
        dp[p] = 0;
        continue;
      }
      for (int type = 0; type < 5; ++type) {
        const int length = 2 * type + 1;
        if (p + length > W || p + length <= first) continue;
        if (!((allowed >> (p + type)) & 1)) continue;
        const float value =
            static_cast<float>(costs[type]) +
            weight[p + type] + dp[p + length];
        if (value < dp[p]) {
          dp[p] = value;
          take_type[p] = static_cast<signed char>(type);
        }
      }
    }

    if (answer != nullptr && dp[0] < INF / 2) {
      *answer = Row{};
      for (int p = 0; next_required[p] < W;) {
        const int type = take_type[p];
        if (type < 0) {
          ++p;
          continue;
        }
        answer->starts[type] |= Mask(1) << p;
        answer->centers |= Mask(1) << (p + type);
        answer->cost += costs[type];
        p += 2 * type + 1;
      }
    }
    return dp[0];
  }

  // TODO(AHC071): ある中心を追加した時、下の行以降で増える費用を前計算する。
  void build_potential() {
    potential.resize(H);
    for (int y = 0; y < H; ++y) {
      array<float, MAX_W> weight{};
      if (y > 0) {
        for (int x = 0; x < W; ++x) {
          weight[x] = 0.90f * potential[y - 1][x];
        }
      }
      const float base = row_dp(holes[y], weight, full);
      for (int x = 0; x < W; ++x) {
        const float added =
            row_dp(holes[y] | (Mask(1) << x), weight, full);
        potential[y][x] = max(0.0f, added - base);
      }
    }
  }

  struct RowDpEntry {
    float value = 0;
    Mask centers = 0;
    int cost = 0;
    unsigned char start = 0;
    unsigned char type = 0;
    unsigned char tail_rank = 0;
  };

  // TODO(AHC071): 1行について最良だけでなく上位limit通りを列挙する。
  // ビーム幅を広げても各親から同じ1通りしか出さないと多様性が増えない。
  vector<Row> row_candidates(
      Mask required,
      const array<float, MAX_W>& weight,
      int limit) const {
    assert(1 <= limit && limit <= MAX_ROW_CANDIDATES);
    if (required == 0) return {Row{}};

    RowDpEntry dp[MAX_W + 1][MAX_ROW_CANDIDATES];
    int count[MAX_W + 1]{};
    int next_required[MAX_W + 1];
    count[W] = 1;
    next_required[W] = W;
    const auto worse = [](const RowDpEntry& left, const RowDpEntry& right) {
      if (left.value != right.value) return left.value > right.value;
      if (left.cost != right.cost) return left.cost > right.cost;
      return left.centers > right.centers;
    };

    for (int p = W - 1; p >= 0; --p) {
      const int first = next_required[p] =
          ((required >> p) & 1) ? p : next_required[p + 1];
      if (first == W) {
        count[p] = 1;
        continue;
      }

      array<RowDpEntry, 6> heap;
      int heap_size = 0;
      const auto make_entry = [&](int type, int rank) {
        const bool skip = type == 5;
        const int end = p + (skip ? 1 : 2 * type + 1);
        const RowDpEntry& tail = dp[end][rank];
        return RowDpEntry{
            (skip ? 0.0f
                  : static_cast<float>(costs[type]) + weight[p + type]) +
                tail.value,
            tail.centers |
                (skip ? Mask(0) : Mask(1) << (p + type)),
            (skip ? 0 : costs[type]) + tail.cost,
            static_cast<unsigned char>(p),
            static_cast<unsigned char>(type),
            static_cast<unsigned char>(rank)};
      };

      if (first != p && count[p + 1]) {
        heap[heap_size++] = make_entry(5, 0);
      }
      for (int type = 0; type < 5; ++type) {
        const int end = p + 2 * type + 1;
        if (first < end && end <= W && count[end] &&
            weight[p + type] < INF / 4) {
          heap[heap_size++] = make_entry(type, 0);
        }
      }
      make_heap(heap.begin(), heap.begin() + heap_size, worse);
      while (heap_size && count[p] < limit) {
        pop_heap(heap.begin(), heap.begin() + heap_size, worse);
        const RowDpEntry entry = heap[--heap_size];
        bool duplicate = false;
        for (int i = 0; i < count[p]; ++i) {
          duplicate |= dp[p][i].centers == entry.centers;
        }
        if (!duplicate) dp[p][count[p]++] = entry;

        const int end =
            p + (entry.type == 5 ? 1 : 2 * entry.type + 1);
        if (entry.tail_rank + 1 < count[end]) {
          heap[heap_size++] = make_entry(entry.type, entry.tail_rank + 1);
          push_heap(heap.begin(), heap.begin() + heap_size, worse);
        }
      }
    }

    vector<Row> result;
    result.reserve(count[0]);
    for (int rank = 0; rank < count[0]; ++rank) {
      Row row;
      row.cost = dp[0][rank].cost;
      row.centers = dp[0][rank].centers;
      int p = 0;
      int current_rank = rank;
      while (next_required[p] < W) {
        const RowDpEntry& entry = dp[p][current_rank];
        if (entry.type == 5) {
          ++p;
        } else {
          row.starts[entry.type] |= Mask(1) << entry.start;
          p = entry.start + 2 * entry.type + 1;
        }
        current_rank = entry.tail_rank;
      }
      result.push_back(row);
    }
    return result;
  }

  vector<Row> greedy_solution() const {
    vector<Row> rows(H);
    Mask support_from_above = 0;
    for (int y = H - 1; y >= 0; --y) {
      const auto& weight = y ? potential[y - 1] : zero_weight;
      row_dp(holes[y] | support_from_above, weight, full, &rows[y]);
      support_from_above = rows[y].centers;
    }
    return rows;
  }

  // TODO(AHC071): 他の行を固定し、1行だけ厳密に安くする局所改善。
  void polish(vector<Row>& rows) const {
    for (int pass = 0; pass < 6; ++pass) {
      bool changed = false;
      for (int i = 0; i < H; ++i) {
        const int y = pass % 2 ? i : H - 1 - i;
        const Mask required =
            holes[y] | (y + 1 < H ? rows[y + 1].centers : 0);
        const Mask allowed = y ? rows[y - 1].covered() : full;
        Row replacement;
        const float value =
            row_dp(required, zero_weight, allowed, &replacement);
        if (value < INF / 2 && replacement.cost < rows[y].cost) {
          rows[y] = replacement;
          changed = true;
        }
      }
      if (!changed) break;
    }
  }

  static int total_cost(const vector<Row>& rows) {
    int result = 0;
    for (const Row& row : rows) result += row.cost;
    return result;
  }

  // Scoreは小さいほど良い。estimatedだけが同点なら実費、中心bitsetで比較する。
  struct BeamRank {
    float estimated = 0;
    int cost = 0;
    Mask centers = 0;

    friend bool operator<(const BeamRank& left, const BeamRank& right) {
      if (left.estimated != right.estimated) {
        return left.estimated < right.estimated;
      }
      if (left.cost != right.cost) return left.cost < right.cost;
      return left.centers < right.centers;
    }
  };

  // ここが「人が問題に合わせて書く部分」。ライブラリ本体は編集しない。
  struct RowBeamProblem {
    struct State {
      // TODO(AHC071): 次の行を作るための最小状態。
      Mask support_from_above = 0;
      int cost = 0;
      vector<Row> rows_top_down;
    };

    struct Action {
      // TODO(AHC071): 1手=今作る1行。next_costは差分評価結果のcache。
      Row row;
      int next_cost = 0;
    };

    using Score = BeamRank;

    Solver& solver;
    int lo;
    int hi;
    int row_limit;
    int completion_cost_bound;
    float scale;
    Mask lower_support;
    const vector<array<float, MAX_W>>& guide;
    const vector<Row>* incumbent_rows;
    double deadline;
    vector<array<float, MAX_W>> row_weight;
    vector<array<float, MAX_W>> below_weight;

    RowBeamProblem(Solver& solver_value,
                   int lo_value,
                   int hi_value,
                   int row_limit_value,
                   int completion_cost_bound_value,
                   float scale_value,
                   float noise,
                   Mask lower_support_value,
                   const vector<array<float, MAX_W>>& guide_value,
                   const vector<Row>* incumbent_rows_value,
                   double deadline_value)
        : solver(solver_value),
          lo(lo_value),
          hi(hi_value),
          row_limit(row_limit_value),
          completion_cost_bound(completion_cost_bound_value),
          scale(scale_value),
          lower_support(lower_support_value),
          guide(guide_value),
          incumbent_rows(incumbent_rows_value),
          deadline(deadline_value),
          row_weight(solver.H),
          below_weight(solver.H) {
      // 同じ世代の全親へ同じ摂動を使う。同一局面の評価も同じになる。
      for (int y = lo; y <= hi; ++y) {
        for (int x = 0; x < solver.W; ++x) {
          if (y > lo) {
            row_weight[y][x] = scale * guide[y - 1][x];
            if (noise != 0) {
              row_weight[y][x] +=
                  noise * (float(solver.random_engine() % 10001) / 10000 -
                           0.5f);
            }
          }
          if (y > lo + 1) {
            below_weight[y][x] = 0.90f * guide[y - 2][x];
          }
        }
      }
    }

    vector<Action> generate_actions(const State& state) const {
      // TODO(AHC071): 現在のStateから合法な「次の1行」を列挙する。
      if (solver.elapsed() >= deadline) return {};
      const int y = hi - static_cast<int>(state.rows_top_down.size());
      const Mask required = solver.holes[y] | state.support_from_above;
      vector<Row> options;
      if (y == lo) {
        Row row;
        if (solver.row_dp(required,
                          solver.zero_weight,
                          lower_support,
                          &row) < INF / 4) {
          options.push_back(row);
        }
      } else {
        options = solver.row_candidates(required, row_weight[y], row_limit);
        // 区間再構築では元の行も候補へ入れ、合法なら戻れる道を増やす。
        if (incumbent_rows != nullptr) {
          const Row& old = (*incumbent_rows)[y];
          if ((old.covered() & required) == required) options.push_back(old);
        }
      }

      vector<Action> actions;
      actions.reserve(options.size());
      for (Row& row : options) {
        const int next_cost = state.cost + row.cost;
        // TODO(AHC071): 完成済み解を閾値にしたbranch-and-bound。
        // 未構築行の費用は非負なので、この時点で上限を超えた枝は改善不能。
        if (next_cost > completion_cost_bound) continue;
        actions.push_back(Action{std::move(row), next_cost});
      }
      return actions;
    }

    Score evaluate_action(const State& state, const Action& action) const {
      // TODO(AHC071): Action適用後の「順位値そのもの」を返す。
      const int y = hi - static_cast<int>(state.rows_top_down.size());
      float future = 0;
      if (y > lo) {
        const Mask allowed = y == lo + 1 ? lower_support : solver.full;
        future = solver.row_dp(solver.holes[y - 1] | action.row.centers,
                               below_weight[y],
                               allowed);
      }
      return Score{static_cast<float>(action.next_cost) + scale * future,
                   action.next_cost,
                   action.row.centers};
    }

    optional<Score> evaluate_action_with_threshold(
        const State& state,
        const Action& action,
        const Score* threshold) const {
      // TODO(AHC071): key重複除去が不要な問題ではrun_with_thresholdを使える。
      // 将来費用は非負なので、現在費用だけで境界を超えたらrow_dpを省略可能。
      if (threshold != nullptr &&
          static_cast<float>(action.next_cost) > threshold->estimated) {
        return nullopt;
      }
      return evaluate_action(state, action);
    }

    Mask make_key(const State& state, const Action& action) const {
      // TODO(AHC071): 次の段が見る情報が同じ候補は、最安の1件だけ残す。
      const int y = hi - static_cast<int>(state.rows_top_down.size());
      return y > lo ? action.row.centers | solver.holes[y - 1] : 0;
    }

    void apply_action(State& state, Action& action) const {
      // TODO(AHC071): 選ばれた上位N件だけ、Stateを本当に更新する。
      state.support_from_above = action.row.centers;
      state.cost = action.next_cost;
      state.rows_top_down.push_back(std::move(action.row));
    }
  };

  // [lo, hi]を上から下へActionBeamRunnerで構築する。
  optional<vector<Row>> search_rows(
      int lo,
      int hi,
      int width,
      int row_limit,
      int cost_bound,
      float scale,
      float noise,
      Mask upper_support,
      Mask lower_support,
      const vector<array<float, MAX_W>>& guide,
      const vector<Row>* incumbent_rows,
      double deadline) {
    RowBeamProblem problem(*this,
                           lo,
                           hi,
                           row_limit,
                           cost_bound,
                           scale,
                           noise,
                           lower_support,
                           guide,
                           incumbent_rows,
                           deadline);
    typename RowBeamProblem::State initial;
    initial.support_from_above = upper_support;
    ActionBeamRunner<RowBeamProblem> beam(
        problem, initial, BeamRank{}, width, false);  // false = 小さいほど良い
    beam.reserve_candidates(static_cast<size_t>(width) * row_limit);

    // AHC071では同じ「次段の必須bitset」をまとめる効果が大きいためkey版。
    // 閾値評価版を使う問題はrun_with_thresholdへ変更する。
    const int turns = hi - lo + 1;
    if (beam.run_with_key(turns) != turns) return nullopt;

    const auto& best = beam.best();
    vector<Row> result(turns);
    for (int i = 0; i < turns; ++i) {
      result[hi - lo - i] = best.rows_top_down[i];
    }
    return result;
  }

  // 区間下端の支持条件を含む将来費用をcacheする。
  const vector<array<float, MAX_W>>* prepare_local_potential(
      int lo,
      int hi,
      Mask lower,
      double deadline) {
    if (static_cast<int>(local_cache.size()) != H) local_cache.resize(H);
    auto& cache = local_cache[lo];
    if (cache.size() >= 64 && !cache.count(lower)) cache.clear();
    auto [iterator, inserted] = cache.try_emplace(lower);
    auto& local = iterator->second;
    if (inserted) local.resize(lo);
    while (static_cast<int>(local.size()) < hi) {
      if (elapsed() >= deadline) return nullptr;
      const int y = static_cast<int>(local.size());
      array<float, MAX_W> weight{};
      if (y > lo) {
        for (int x = 0; x < W; ++x) weight[x] = 0.90f * local[y - 1][x];
      }
      const Mask allowed = y == lo ? lower : full;
      const float base = row_dp(holes[y], weight, allowed);
      array<float, MAX_W> level{};
      for (int x = 0; x < W; ++x) {
        const float value =
            row_dp(holes[y] | (Mask(1) << x), weight, allowed);
        level[x] = value >= INF / 4 ? INF : max(0.0f, value - base);
      }
      local.push_back(level);
    }
    return &local;
  }

  bool rebuild(vector<Row>& rows,
               int lo,
               int hi,
               int width,
               float scale,
               float noise,
               double deadline) {
    const Mask lower = lo ? rows[lo - 1].covered() : full;
    const auto* guide = prepare_local_potential(lo, hi, lower, deadline);
    if (guide == nullptr) return false;

    int old_cost = 0;
    for (int y = lo; y <= hi; ++y) old_cost += rows[y].cost;
    auto replacement = search_rows(lo,
                                   hi,
                                   width,
                                   row_candidate_count,
                                   old_cost,
                                   scale,
                                   noise,
                                   hi + 1 < H ? rows[hi + 1].centers : 0,
                                   lower,
                                   *guide,
                                   &rows,
                                   deadline);
    if (!replacement.has_value()) return false;
    const int new_cost = total_cost(*replacement);
    if (new_cost > old_cost) return false;
    for (int y = lo; y <= hi; ++y) rows[y] = (*replacement)[y - lo];
    return true;
  }

  vector<Row> solve() {
    started = chrono::steady_clock::now();
    build_potential();
    vector<Row> best = greedy_solution();
    polish(best);
    int best_cost = total_cost(best);

    const double deadline = max(0.0, time_limit - 0.025);
    const double initial_deadline =
        use_lns ? deadline * initial_ratio : deadline;
    const float scales[] = {1.0f, 1.5f, 0.7f, 2.0f, 1.2f, 0.85f};
    int completed_beams = 0;
    for (int run = 0; elapsed() < initial_deadline; ++run) {
      auto candidate = search_rows(0,
                                   H - 1,
                                   beam_width,
                                   row_candidate_count,
                                   best_cost,
                                   scales[run % 6],
                                   run < 3 ? 0.0f : 1.0f,
                                   0,
                                   full,
                                   potential,
                                   nullptr,
                                   initial_deadline);
      if (!candidate.has_value()) break;
      ++completed_beams;
      polish(*candidate);
      const int cost = total_cost(*candidate);
      if (cost < best_cost) {
        best = std::move(*candidate);
        best_cost = cost;
      }
    }

    const int initial_cost = best_cost;
    int rebuild_count = 0;
    if (use_lns) {
      vector<Row> current = best;
      int current_cost = best_cost;
      SimulatedAnnealing annealing(2.0, 0.1, 314159265);
      while (elapsed() < deadline) {
        vector<Row> trial = current;
        const int max_height = min(H, max_rebuild_height);
        const int min_height = min(max_height, min_rebuild_height);
        const int height = min_height + static_cast<int>(
            random_engine() %
            static_cast<unsigned int>(max_height - min_height + 1));
        const int lo = static_cast<int>(
            random_engine() % static_cast<unsigned int>(H - height + 1));
        const float scale =
            0.7f + static_cast<float>(random_engine() % 1001) / 1000;

        if (random_engine() % 4 == 0) {
          // 1行だけ別配置にして、区間ビームとは違う谷へ移る。
          const int y = static_cast<int>(
              random_engine() % static_cast<unsigned int>(H));
          array<float, MAX_W> random_weight{};
          for (int x = 0; x < W; ++x) {
            random_weight[x] =
                8.0f * (float(random_engine() % 10001) / 10000 - 0.5f);
          }
          const Mask required =
              holes[y] | (y + 1 < H ? trial[y + 1].centers : 0);
          const Mask allowed = y ? trial[y - 1].covered() : full;
          row_dp(required, random_weight, allowed, &trial[y]);
        } else {
          if (!rebuild(trial,
                       lo,
                       lo + height - 1,
                       24,
                       scale,
                       2.0f,
                       deadline)) {
            continue;
          }
          ++rebuild_count;
          polish(trial);
        }

        const int trial_cost = total_cost(trial);
        const double progress = clamp(
            (elapsed() - initial_deadline) /
                max(0.001, deadline - initial_deadline),
            0.0,
            1.0);
        annealing.set_progress(progress);
        const int improvement = current_cost - trial_cost;  // 費用最小化
        if (annealing.accept(improvement)) {
          current = std::move(trial);
          current_cost = trial_cost;
          if (current_cost < best_cost) {
            best = current;
            best_cost = current_cost;
          }
        }
      }
      polish(best);
      best_cost = total_cost(best);
    }

    cerr << "cost: " << best_cost
         << " beams: " << completed_beams
         << " initial: " << initial_cost
         << " rebuilds: " << rebuild_count
         << " elapsed: " << elapsed() * 1000 << "ms\n";
    return best;
  }

  void print_answer(const vector<Row>& rows) const {
    vector<Brick> answer;
    for (int y = 0; y < H; ++y) {
      for (int type = 0; type < 5; ++type) {
        for (Mask starts = rows[y].starts[type]; starts;
             starts &= starts - 1) {
          answer.push_back(
              Brick{__builtin_ctzll(starts), y, 2 * type + 1});
        }
      }
    }
    cout << answer.size() << '\n';
    for (const Brick& brick : answer) {
      cout << brick.x << ' ' << brick.y << ' ' << brick.width << '\n';
    }
  }
};

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  Solver solver;
  solver.read_input();
  if (solver.W <= 0 || solver.W > MAX_W || solver.H <= 0 ||
      solver.row_candidate_count < 1 ||
      solver.row_candidate_count > MAX_ROW_CANDIDATES) {
    return 1;
  }
  const vector<Row> answer = solver.solve();
  solver.print_answer(answer);
}
