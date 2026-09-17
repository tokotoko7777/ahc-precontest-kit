#include <bits/stdc++.h>
using namespace std;

// 提出時は次の1行を、このhppの全文へ置き換える。
// BEGIN LIBRARY: action-beam-search.hpp
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
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
    // 新しい候補のorderは必ず後。同点でも残れないのでScore比較1回でよい。
    if (batched_selection_ && cutoff_ready_ &&
        !score_is_better(candidate.score, candidates_[width - 1].score)) {
      return;
    }

    candidates_.push_back(std::move(candidate));
    last_buffered_peak_count_ =
        std::max(last_buffered_peak_count_, candidates_.size());
    if (batched_selection_ && candidates_.size() >= batch_limit()) {
      keep_best_candidates(width, false);
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
          keep_best_candidates(width, false);
          cutoff_ready_ = true;
        }
      }
    }
    last_unique_count_ = last_generated_count_;
    return finish_step(apply);
  }

  // candidates_を上位kept件へ縮める。中間選抜では全件をsortしない。
  // sorted=falseでもkept-1には最下位を置くので、次のcutoff判定に使える。
  // 大きい候補は小さいIDを選び、選抜中にActionやScoreを何度もswapしない。
  void keep_best_candidates(std::size_t kept, bool sorted = true) {
    kept = std::min(kept, candidates_.size());
    if (kept == 0) { candidates_.clear(); return; }
    // 小さく単純な候補は直接partitionする。ID経由の間接参照・別bufferへの
    // 移動を省く。大きい/非trivial/代入不能なActionは下のID選抜を維持する。
    if constexpr (sizeof(Candidate) <= 32 &&
                  std::is_trivially_copyable_v<Candidate> &&
                  std::is_move_constructible_v<Candidate> &&
                  std::is_move_assignable_v<Candidate>) {
      const auto better = [&](const Candidate& a, const Candidate& b) {
        return candidate_is_better(a, b);
      };
      if (kept < candidates_.size()) {
        select_small_candidates(sorted ? kept : kept - 1, better);
      } else if (!sorted) {
        std::iter_swap(candidates_.end() - 1,
                      std::max_element(candidates_.begin(), candidates_.end(), better));
      }
      candidates_.erase(candidates_.begin() + kept, candidates_.end());
      if (sorted) std::sort(candidates_.begin(), candidates_.end(), better);
      return;
    }
    candidate_ids_.resize(candidates_.size());
    std::iota(candidate_ids_.begin(), candidate_ids_.end(), std::size_t{0});
    const auto better_id = [&](std::size_t a, std::size_t b) {
      return candidate_is_better(candidates_[a], candidates_[b]);
    };
    if (kept < candidate_ids_.size()) {
      std::nth_element(candidate_ids_.begin(),
                       candidate_ids_.begin() + (sorted ? kept : kept - 1),
                       candidate_ids_.end(), better_id);
      candidate_ids_.resize(kept);
    }
    if (sorted) {
      std::sort(candidate_ids_.begin(), candidate_ids_.end(), better_id);
    } else if (kept == candidates_.size()) {
      // 最初のN件で境界を作る場合はpartition不要。最下位だけ末尾へ移す。
      std::iter_swap(candidate_ids_.end() - 1,
                    std::max_element(candidate_ids_.begin(), candidate_ids_.end(), better_id));
    }

    scratch_candidates_.clear();
    scratch_candidates_.reserve(std::max(scratch_candidates_.capacity(), kept));
    for (std::size_t id : candidate_ids_) {
      scratch_candidates_.push_back(std::move(candidates_[id]));
    }
    candidates_.swap(scratch_candidates_);
    scratch_candidates_.clear();
    candidate_ids_.clear();
  }

  // 整数Scoreなら順位を変えず、分布の1 byteで境界bucketを先に絞る。
  // 同じbucket内だけ比較選抜する。同点は従来どおり生成順で決定する。
  // 少数候補・bool・浮動小数・独自Scoreはstd::nth_elementへ戻す。
  template <class Better>
  void select_small_candidates(std::size_t nth, Better better) {
    auto first = candidates_.begin();
    auto last = candidates_.end();
    if constexpr (std::is_integral_v<Score> &&
                  !std::is_same_v<Score, bool> && sizeof(Score) <= 8) {
      if (candidates_.size() >= 1024) {
        using Unsigned = std::make_unsigned_t<Score>;
        const auto ordered_key = [&](const Candidate& candidate) {
          Unsigned key = static_cast<Unsigned>(candidate.score);
          if constexpr (std::is_signed_v<Score>) {
            key ^= Unsigned(1) << (std::numeric_limits<Unsigned>::digits - 1);
          }
          return maximize_ ? static_cast<Unsigned>(~key) : key;
        };
        Unsigned low = ordered_key(*first), high = low;
        for (const Candidate& candidate : candidates_) {
          const Unsigned key = ordered_key(candidate);
          low = std::min(low, key);
          high = std::max(high, key);
        }
        std::uintmax_t different = static_cast<Unsigned>(low ^ high);
        if (different != 0) {
          unsigned shift = 0;
          while (different > 255) { different >>= 1; ++shift; }
          const auto bucket = [&](const Candidate& candidate) {
            return static_cast<unsigned>((ordered_key(candidate) >> shift) & Unsigned(255));
          };
          std::array<std::size_t, 256> counts{};
          for (const Candidate& candidate : candidates_) ++counts[bucket(candidate)];
          std::size_t prefix = 0;
          unsigned boundary = 0;
          while (prefix + counts[boundary] <= nth) prefix += counts[boundary++];
          first = std::partition(first, last, [&](const Candidate& candidate) {
            return bucket(candidate) < boundary;
          });
          last = std::partition(first, last, [&](const Candidate& candidate) {
            return bucket(candidate) == boundary;
          });
        }
      }
    }
    std::nth_element(first, candidates_.begin() + nth, last, better);
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
  bool finish_step(Apply& apply, bool already_selected = false) {
    if (candidates_.empty()) return false;
    if (!already_selected) keep_best_candidates(static_cast<std::size_t>(beam_width_));
    last_kept_count_ = candidates_.size();

    next_beam_.clear();
    next_scores_.clear();
    next_beam_.reserve(candidates_.size());
    next_scores_.reserve(candidates_.size());
    for (Candidate& candidate : candidates_) {
      // 最終配置先へ直接コピーして反映。大きなStateの一時object→vector移動を省く。
      next_beam_.emplace_back(beam_[candidate.parent]);
      apply(next_beam_.back(), candidate.action);
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
    return finish_step(apply, true);
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
    return finish_step(apply, true);
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
// END LIBRARY: action-beam-search.hpp

// Pre-contest public solver source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/examples/search/ahc032_action_beam.cpp
// Official problem: https://atcoder.jp/contests/ahc032/tasks/ahc032_a

// ============================================================================
// ここから問題ごとに書く部分。定数、State、Action、Problem、出力を含む。
// ============================================================================
constexpr int BOARD_SIZE = 9;
constexpr int STAMP_SIZE = 3;
constexpr int STAMP_COUNT = 20;
constexpr int OPERATION_LIMIT = 81;
constexpr int PLACEMENTS_PER_AXIS = BOARD_SIZE - STAMP_SIZE + 1;
constexpr int PLACEMENT_COUNT =
    PLACEMENTS_PER_AXIS * PLACEMENTS_PER_AXIS;
constexpr uint32_t MODULO = 998244353U;
constexpr long long RANK_SCALE = 4900;

#ifndef AHC032_BEAM_WIDTH
#define AHC032_BEAM_WIDTH 9000
#endif
#ifndef AHC032_END_COMBOS
#define AHC032_END_COMBOS 0
#endif
static_assert(AHC032_END_COMBOS >= 0);
// 5～7枚合成＋最後の7手予約は実験用。開発10ケースで既存配分より悪化した
// ため既定では無効。-DAHC032_END_COMBOS=4096で比較できる。

struct ModStampProblem {
  struct Placement {
    uint8_t row = 0;
    uint8_t column = 0;
    uint8_t max_actions = 0;
    uint8_t cumulative_limit = 0;
  };

  // TODO(AHC032): 探索途中の盤面と、確定済み得点をStateへ置く。
  struct State {
    array<uint32_t, BOARD_SIZE * BOARD_SIZE> board{};
    array<uint16_t, PLACEMENT_COUNT> choices{};
    int position = 0;
    int operations = 0;
    long long finalized_score = 0;
  };

  // TODO(AHC032): Actionは「同じ場所で押すスタンプ多重集合」の番号だけ。
  // 3x3盤面をActionへ持たせず、全候補ぶんの保存量を2 byteに抑える。
  using Action = uint16_t;
  using Score = long long;

  struct Combo {
    array<uint32_t, STAMP_SIZE * STAMP_SIZE> add{};
    array<uint8_t, 7> stamp_ids{};
    uint8_t count = 0;
  };

  array<uint32_t, BOARD_SIZE * BOARD_SIZE> initial_board{};
  array<array<uint32_t, STAMP_SIZE * STAMP_SIZE>, STAMP_COUNT> stamps{};
  array<Placement, PLACEMENT_COUNT> placements{};
  vector<Combo> combinations;
  array<vector<Action>, 8> allowed_actions;

  void read_input() {
    int n, m, k;
    cin >> n >> m >> k;
    if (n != BOARD_SIZE || m != STAMP_COUNT || k != OPERATION_LIMIT) {
      throw runtime_error("this example expects the official AHC032 sizes");
    }
    for (uint32_t& value : initial_board) cin >> value;
    for (auto& stamp : stamps) {
      for (uint32_t& value : stamp) cin >> value;
    }
    prepare();
  }

  // 入力配列を直接用意するテスト・ベンチマークも、同じ前計算を使う。
  void prepare() {
    combinations.clear();
    for (auto& actions : allowed_actions) actions.clear();
    build_placements();
    build_combinations();
  }

  State make_initial_state() const {
    State state;
    state.board = initial_board;
    return state;
  }

  Score initial_score() const {
    return 6LL * MODULO * PLACEMENT_COUNT * OPERATION_LIMIT;
  }

  // TODO(AHC032): 現在位置で合法なAction一覧を返す。
  // Action一覧はProblem側で共有し、親Stateごとのvector確保を避ける。
  const vector<Action>& generate_actions(const State& state) const {
    const Placement& placement = placements[state.position];
    const int remaining_budget =
        static_cast<int>(placement.cumulative_limit) - state.operations;
    const int maximum = max(
        0, min(static_cast<int>(placement.max_actions), remaining_budget));
    return allowed_actions[maximum];
  }

  // TODO(AHC032): Action適用後の順位値そのものを、盤面を作らず計算する。
  // 走査済みの上辺・左辺は以後の3x3スタンプで変わらないので、その値を
  // finalized_scoreへ移す。残り手数の項は、序盤で81手を使い切るのを防ぐ。
  Score evaluate_action(const State& state, const Action& action) const {
    const Placement& placement = placements[state.position];
    const int row = placement.row;
    const int column = placement.column;
    const int finalized_rows =
        row == PLACEMENTS_PER_AXIS - 1 ? STAMP_SIZE : 1;
    const int finalized_columns =
        column == PLACEMENTS_PER_AXIS - 1 ? STAMP_SIZE : 1;
    const Combo& combo = combinations[action];

    Score finalized = state.finalized_score;
    for (int di = 0; di < finalized_rows; ++di) {
      for (int dj = 0; dj < finalized_columns; ++dj) {
        const int board_index = (row + di) * BOARD_SIZE + column + dj;
        const int stamp_index = di * STAMP_SIZE + dj;
        finalized += add_mod(
            state.board[board_index], combo.add[stamp_index]);
      }
    }
    const int next_position = state.position + 1;
    const int next_operations = state.operations + combo.count;
    return finalized * RANK_SCALE +
           6LL * MODULO * (PLACEMENT_COUNT - next_position) *
               (OPERATION_LIMIT - next_operations);
  }

  // TODO(AHC032): 上位N件に残ったActionだけを子Stateへ反映する。
  void apply_action(State& state, Action& action) const {
    const Placement& placement = placements[state.position];
    const int row = placement.row;
    const int column = placement.column;
    const Combo& combo = combinations[action];
    for (int di = 0; di < STAMP_SIZE; ++di) {
      for (int dj = 0; dj < STAMP_SIZE; ++dj) {
        const int board_index = (row + di) * BOARD_SIZE + column + dj;
        const int stamp_index = di * STAMP_SIZE + dj;
        state.board[board_index] = add_mod(
            state.board[board_index], combo.add[stamp_index]);
      }
    }

    const int finalized_rows =
        row == PLACEMENTS_PER_AXIS - 1 ? STAMP_SIZE : 1;
    const int finalized_columns =
        column == PLACEMENTS_PER_AXIS - 1 ? STAMP_SIZE : 1;
    for (int di = 0; di < finalized_rows; ++di) {
      for (int dj = 0; dj < finalized_columns; ++dj) {
        state.finalized_score +=
            state.board[(row + di) * BOARD_SIZE + column + dj];
      }
    }

    state.choices[state.position] = action;
    state.operations += combo.count;
    ++state.position;
  }

  void validate(const State& answer) const {
    if (answer.position != PLACEMENT_COUNT ||
        answer.operations > OPERATION_LIMIT) {
      throw runtime_error("beam did not produce a legal complete answer");
    }
    auto board = initial_board;
    int operations = 0;
    for (int position = 0; position < PLACEMENT_COUNT; ++position) {
      const Placement& placement = placements[position];
      const Combo& combo = combinations[answer.choices[position]];
      operations += combo.count;
      if (combo.count > placement.max_actions ||
          operations > placement.cumulative_limit) {
        throw runtime_error("operation schedule limit exceeded");
      }
      for (int di = 0; di < STAMP_SIZE; ++di) {
        for (int dj = 0; dj < STAMP_SIZE; ++dj) {
          const int board_index =
              (placement.row + di) * BOARD_SIZE + placement.column + dj;
          board[board_index] = add_mod(
              board[board_index], combo.add[di * STAMP_SIZE + dj]);
        }
      }
    }
    long long score = 0;
    for (uint32_t value : board) score += value;
    if (operations != answer.operations || board != answer.board ||
        score != answer.finalized_score) {
      throw runtime_error("incremental state differs from full replay");
    }
  }

 private:
  static uint32_t add_mod(uint32_t left, uint32_t right) {
    uint32_t result = left + right;
    if (result >= MODULO) result -= MODULO;
    return result;
  }

  void add_combination(const array<int, 7>& ids, int count) {
    Combo combo;
    combo.count = static_cast<uint8_t>(count);
    for (int i = 0; i < count; ++i) {
      combo.stamp_ids[i] = static_cast<uint8_t>(ids[i]);
      for (int cell = 0; cell < STAMP_SIZE * STAMP_SIZE; ++cell) {
        combo.add[cell] = add_mod(combo.add[cell], stamps[ids[i]][cell]);
      }
    }
    combinations.push_back(combo);
  }

  void enumerate_combinations(
      int count, int depth, int minimum_stamp, array<int, 7>& ids) {
    if (depth == count) {
      add_combination(ids, count);
      return;
    }
    for (int stamp = minimum_stamp; stamp < STAMP_COUNT; ++stamp) {
      ids[depth] = stamp;
      enumerate_combinations(count, depth + 1, stamp, ids);
    }
  }

  void build_combinations() {
    array<int, 7> ids{};
    for (int count = 0; count <= 4; ++count) {
      enumerate_combinations(count, 0, 0, ids);
    }
    // TODO(AHC032): 最後の3x3だけ、5/6/7押しの候補も試す。
    // (8,8)は最後の(6,6)への配置以外では変わらない。そこで、その1マスが
    // 高くなる合成を各枚数で上位K個残す。これは速度のための近似選抜。
    // 0～4押しも全て残す。最後の7押し用に、build_placementsで手数を予約する。
    // 参考: https://img.atcoder.jp/ahc032/editorial.pdf (スタンプの合成)
    if (AHC032_END_COMBOS > 0) {
      for (int count = 5; count <= 7; ++count) {
        const size_t begin = combinations.size();
        enumerate_combinations(count, 0, 0, ids);
        const auto better = [&](const Combo& a, const Combo& b) {
          const auto av = add_mod(initial_board.back(), a.add.back());
          const auto bv = add_mod(initial_board.back(), b.add.back());
          if (av != bv) return av > bv;
          return a.stamp_ids < b.stamp_ids;
        };
        const size_t keep = min(static_cast<size_t>(AHC032_END_COMBOS),
                                combinations.size() - begin);
        auto first = combinations.begin() + static_cast<ptrdiff_t>(begin);
        auto last = first + static_cast<ptrdiff_t>(keep);
        if (last != combinations.end()) nth_element(first, last, combinations.end(), better);
        sort(first, last, better);
        combinations.resize(begin + keep);
      }
    }
    if (combinations.size() > numeric_limits<Action>::max()) {
      throw runtime_error("too many combinations for uint16_t Action");
    }
    for (int id = 0; id < static_cast<int>(combinations.size()); ++id) {
      for (int limit = combinations[id].count; limit <= 7; ++limit) {
        allowed_actions[limit].push_back(static_cast<Action>(id));
      }
    }
  }

  void build_placements() {
    int position = 0;
    int cumulative_twice = 0;
    const auto add = [&](int row, int column) {
      const bool bottom = row == PLACEMENTS_PER_AXIS - 1;
      const bool right = column == PLACEMENTS_PER_AXIS - 1;
      int maximum = 2;
      int budget_increase_twice = 3;
      if (bottom && right) {
        maximum = AHC032_END_COMBOS > 0 ? 7 : 4;
        budget_increase_twice = 6;
      } else if (bottom || right) {
        maximum = 3;
        budget_increase_twice = 4;
      }
      cumulative_twice += budget_increase_twice;
      // TODO(AHC032): 最後に7手残す。候補だけ増やしても、以前の配分では
      // 残り3～4手の親ばかりになり、5～7押し候補を実際には試せない。
      const int limit = AHC032_END_COMBOS > 0 && !(bottom && right)
          ? min((cumulative_twice + 1) / 2, OPERATION_LIMIT - 7)
          : (cumulative_twice + 1) / 2;
      placements[position++] = {
          static_cast<uint8_t>(row),
          static_cast<uint8_t>(column),
          static_cast<uint8_t>(maximum),
          static_cast<uint8_t>(limit)};
    };

    // 未処理領域の上辺、左辺の順で1層ずつ確定する。
    for (int layer = 0; layer < PLACEMENTS_PER_AXIS; ++layer) {
      for (int column = layer; column < PLACEMENTS_PER_AXIS; ++column) {
        add(layer, column);
      }
      for (int row = layer + 1; row < PLACEMENTS_PER_AXIS; ++row) {
        add(row, layer);
      }
    }
    if (position != PLACEMENT_COUNT ||
        placements.back().cumulative_limit != OPERATION_LIMIT) {
      throw runtime_error("invalid placement schedule");
    }
  }
};

void print_answer(
    const ModStampProblem& problem, const ModStampProblem::State& answer) {
  cout << answer.operations << '\n';
  for (int position = 0; position < PLACEMENT_COUNT; ++position) {
    const ModStampProblem::Placement& placement =
        problem.placements[position];
    const ModStampProblem::Combo& combo =
        problem.combinations[answer.choices[position]];
    for (int i = 0; i < combo.count; ++i) {
      cout << static_cast<int>(combo.stamp_ids[i]) << ' '
           << static_cast<int>(placement.row) << ' '
           << static_cast<int>(placement.column) << '\n';
    }
  }
}

// ============================================================================
// ここから下は探索の呼び出し。候補buffer・上位N件選抜・StateコピーはRunner側。
// ============================================================================
int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  ModStampProblem problem;
  problem.read_input();
  ModStampProblem::State initial = problem.make_initial_state();
  ActionBeamRunner<ModStampProblem> beam(
      problem, initial, problem.initial_score(),
      AHC032_BEAM_WIDTH);  // TODO(AHC032): ビーム幅。
  const int advanced = beam.run(PLACEMENT_COUNT);
  if (advanced != PLACEMENT_COUNT) {
    throw runtime_error("beam stopped before the final placement");
  }
  problem.validate(beam.best());
  print_answer(problem, beam.best());
  return 0;
}
