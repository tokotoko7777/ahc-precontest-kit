#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// 状態を丸ごと持つ、分かりやすさ優先のビームサーチ。
// State が小さい時や、まずビームサーチを試したい時に向いている。
// 状態が大きくコピーが重い時は tree-beam-search.hpp も検討する。
//
// 使い方:
// SimpleBeamSearch<State, long long> beam(initial_state, 100);
// for (int turn = 0; turn < turns; ++turn) {
//   if (!beam.step(
//           [](const State& state) { return state.next_states(); },
//           [](const State& state) { return state.score; })) {
//     break;
//   }
// }
// State answer = beam.best();
//
// step は子が1つもなければ false を返し、現在の層をそのまま残す。
// 一部の状態だけ子を持たない場合、その状態は次の層には残らない。
// 終端状態も答えの候補にしたい時は、step の前後で states() を調べて
// 自分で最良の終端状態を保存する。
template <class State, class Score>
struct SimpleBeamSearch {
  struct Candidate {
    Score score;
    State state;
    std::size_t order;
  };

  explicit SimpleBeamSearch(
      State initial_state,
      int beam_width,
      bool maximize = true)
      : beam_width_(beam_width), maximize_(maximize) {
    if (beam_width <= 0) {
      throw std::invalid_argument("beam_width must be positive");
    }
    beam_.push_back(std::move(initial_state));
  }

  // 1層進める。expand(state) は次の State を並べたコンテナを返す。
  // constコンテナはコピーし、非constコンテナの要素はmoveして消費する。
  // evaluate(state) はビーム内での順位を決める値を返す。
  template <class Expand, class Evaluate>
  bool step(Expand&& expand, Evaluate&& evaluate) {
    candidate_buffer_.clear();
    std::size_t order = 0;

    for (const State& state : beam_) {
      auto&& next_states = expand(state);
      for (auto&& next_state : next_states) {
        Score score = evaluate(next_state);
        candidate_buffer_.push_back(
            {std::move(score), std::move(next_state), order++});
      }
    }

    if (candidate_buffer_.empty()) return false;
    keep_best_candidates();
    return true;
  }

  // 同じ key の状態は、評価値が一番良い候補だけを残してから幅を絞る。
  // make_key(state) は int、uint64_t、string などを返すようにする。
  template <class Expand, class Evaluate, class MakeKey>
  bool step_with_key(
      Expand&& expand,
      Evaluate&& evaluate,
      MakeKey&& make_key) {
    using Key =
        std::decay_t<decltype(make_key(std::declval<const State&>()))>;

    candidate_buffer_.clear();
    std::unordered_map<Key, std::size_t> index_by_key;
    index_by_key.reserve(std::max(
        candidate_buffer_.capacity(), static_cast<std::size_t>(beam_width_)));
    std::size_t order = 0;

    for (const State& state : beam_) {
      auto&& next_states = expand(state);
      for (auto&& next_state : next_states) {
        Score score = evaluate(next_state);
        Key key = make_key(next_state);
        Candidate candidate{
            std::move(score), std::move(next_state), order++};

        const auto found = index_by_key.find(key);
        if (found == index_by_key.end()) {
          const std::size_t index = candidate_buffer_.size();
          index_by_key.emplace(std::move(key), index);
          candidate_buffer_.push_back(std::move(candidate));
        } else if (score_is_better(
                       candidate.score,
                       candidate_buffer_[found->second].score)) {
          candidate_buffer_[found->second] = std::move(candidate);
        }
      }
    }

    if (candidate_buffer_.empty()) return false;
    keep_best_candidates();
    return true;
  }

  // 現在の層。step 成功後は評価値が良い順に並んでいる。
  const std::vector<State>& states() const { return beam_; }

  const State& best() const {
    assert(!beam_.empty());
    return beam_.front();
  }

  State& best() {
    assert(!beam_.empty());
    return beam_.front();
  }

  int depth() const { return depth_; }

  int width() const { return beam_width_; }

  // 1層に出る候補数の目安が分かる時は、再確保を避けられる。
  void reserve_candidates(std::size_t count) {
    candidate_buffer_.reserve(count);
    candidate_ids_.reserve(count);
  }

  // 幅を小さくした時は、現在の層もその場で切り詰める。
  void set_width(int beam_width) {
    if (beam_width <= 0) {
      throw std::invalid_argument("beam_width must be positive");
    }
    beam_width_ = beam_width;
    if (beam_.size() > static_cast<std::size_t>(beam_width_)) {
      beam_.resize(static_cast<std::size_t>(beam_width_));
    }
  }

 private:
  int beam_width_;
  bool maximize_;
  int depth_ = 0;
  std::vector<State> beam_;
  std::vector<State> next_beam_;
  std::vector<Candidate> candidate_buffer_;
  std::vector<std::size_t> candidate_ids_;

  // 浮動小数点の NaN は最大化・最小化のどちらでも最悪とする。
  // これにより、候補比較が strict weak ordering を壊さない。
  bool score_is_better(const Score& a, const Score& b) const {
    if constexpr (std::is_floating_point_v<Score>) {
      const bool a_is_nan = std::isnan(a);
      const bool b_is_nan = std::isnan(b);
      if (a_is_nan != b_is_nan) return !a_is_nan;
      if (a_is_nan) return false;
    }
    return maximize_ ? b < a : a < b;
  }

  bool candidate_is_better(std::size_t a, std::size_t b) const {
    const Candidate& left = candidate_buffer_[a];
    const Candidate& right = candidate_buffer_[b];
    if (score_is_better(left.score, right.score)) return true;
    if (score_is_better(right.score, left.score)) return false;
    return left.order < right.order;
  }

  void keep_best_candidates() {
    const std::size_t kept = std::min(
        static_cast<std::size_t>(beam_width_), candidate_buffer_.size());

    candidate_ids_.resize(candidate_buffer_.size());
    std::iota(candidate_ids_.begin(), candidate_ids_.end(), std::size_t{0});
    const auto is_better_id = [&](std::size_t a, std::size_t b) {
      return candidate_is_better(a, b);
    };

    // State 本体ではなく番号だけを並べ替える。大きな State でも、
    // 選抜中に何度も move されない。
    if (kept < candidate_ids_.size()) {
      std::nth_element(candidate_ids_.begin(),
                       candidate_ids_.begin() + kept,
                       candidate_ids_.end(), is_better_id);
      candidate_ids_.resize(kept);
    }
    std::sort(candidate_ids_.begin(), candidate_ids_.end(), is_better_id);

    next_beam_.clear();
    next_beam_.reserve(kept);
    for (std::size_t id : candidate_ids_) {
      next_beam_.push_back(std::move(candidate_buffer_[id].state));
    }
    beam_.swap(next_beam_);
    candidate_buffer_.clear();
    candidate_ids_.clear();
    ++depth_;
  }
};

// 指定した深さまで進める簡単なラッパー。
// State の中に操作履歴も入れておくと、返り値から答えを復元できる。
// 子を持たない状態は捨て、最も深い空でない層の評価値最大/最小の状態を返す。
template <class State, class Expand, class Evaluate>
State simple_beam_search(
    State initial_state,
    int turns,
    int beam_width,
    Expand expand,
    Evaluate evaluate,
    bool maximize = true) {
  if (turns < 0) {
    throw std::invalid_argument("turns must be non-negative");
  }

  using Score = std::decay_t<decltype(evaluate(initial_state))>;
  SimpleBeamSearch<State, Score> beam(
      std::move(initial_state), beam_width, maximize);
  for (int turn = 0; turn < turns; ++turn) {
    if (!beam.step(expand, evaluate)) break;
  }
  return std::move(beam.best());
}

// 同じ key の候補を1つにまとめる固定深さ版。
template <class State, class Expand, class Evaluate, class MakeKey>
State simple_beam_search_with_key(
    State initial_state,
    int turns,
    int beam_width,
    Expand expand,
    Evaluate evaluate,
    MakeKey make_key,
    bool maximize = true) {
  if (turns < 0) {
    throw std::invalid_argument("turns must be non-negative");
  }

  using Score = std::decay_t<decltype(evaluate(initial_state))>;
  SimpleBeamSearch<State, Score> beam(
      std::move(initial_state), beam_width, maximize);
  for (int turn = 0; turn < turns; ++turn) {
    if (!beam.step_with_key(expand, evaluate, make_key)) break;
  }
  return std::move(beam.best());
}
