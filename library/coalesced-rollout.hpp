#include <cstddef>
#include <cstdint>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <vector>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/coalesced-rollout.hpp

// 同じ未来を辿る少数候補を同時に進め、同じ途中状態になったら残りを共有する。
// hashによる近似ではない。等価なStateなら以後の遷移と最終評価も必ず同じ、が条件。
// 時刻は全候補で同じstep引数を使う。将来に効く履歴・乱数状態はState/固定Scenarioへ
// 全て入れる。最初のActionが違うだけで未来の方策も変わるなら、その情報もStateへ入れる。
// callbackは外部状態を変更しないこと（呼び出し回数は省略される）。
//
// TODO: Stateはsimulationの全状態、Scoreは最終評価の型を書く。
// TODO: initialに、比較したい各Actionを1回反映したStateを列挙順で入れる。
// TODO: advance(State&, int step)へ、共通Scenarioのstep番を使う1遷移を書く。
// TODO: evaluate(const State&)は最後の評価値を返す。
// TODO: State::operator==は「同じ未来を持つ」時だけtrue。等価判定を第5引数にも渡せる。
//
// CoalescedRollout<MyState, long long> cache;
// const auto& scores = cache.evaluate(initial, steps, advance, evaluate);
// scores[i]はinitial[i]を最後まで実行した正確な評価。次のevaluateまで有効。
// evaluate<false>(...)は共有OFFの比較用。同じ入力なら全scoreが一致する。
// Stateはコピー構築・代入可能、Scoreはコピー構築可能にする（default構築は不要）。
// 等価判定は1段O(候補数^2)。数個の候補向けで、大きなビームには使わない。
template <class State, class Score>
class CoalescedRollout {
 public:
  void reserve(std::size_t count) {
    states_.reserve(count);
    parent_.reserve(count);
    active_.reserve(count);
    scores_.reserve(count);
  }

  template <bool Merge = true, class Advance, class Evaluate,
            class Equal = std::equal_to<State>>
  const std::vector<Score>& evaluate(const std::vector<State>& initial,
                                    int steps, Advance&& advance,
                                    Evaluate&& evaluate_final, Equal equal = {}) {
    if (steps < 0) throw std::invalid_argument("steps must be non-negative");
    states_ = initial;
    parent_.resize(initial.size());
    active_.resize(initial.size());
    std::iota(parent_.begin(), parent_.end(), std::size_t{0});
    std::iota(active_.begin(), active_.end(), std::size_t{0});
    last_transitions_ = 0;
    if constexpr (Merge) merge_equal(equal);
    for (int step = 0; step < steps; ++step) {
      for (std::size_t index : active_) advance(states_[index], step);
      last_transitions_ += active_.size();
      if constexpr (Merge) merge_equal(equal);
    }
    scores_.clear();
    for (std::size_t index = 0; index < initial.size(); ++index) {
      std::size_t root = index;
      while (parent_[root] != root) root = parent_[root];
      // 代表は常に先の候補なので、代表の最終評価は既に計算済み。
      if (root == index) scores_.push_back(evaluate_final(states_[index]));
      else scores_.push_back(scores_[root]);
    }
    return scores_;
  }

  std::uint64_t last_transitions() const { return last_transitions_; }

 private:
  template <class Equal>
  void merge_equal(Equal& equal) {
    std::size_t kept = 0;
    for (std::size_t index : active_) {
      std::size_t earlier = 0;
      while (earlier < kept && !equal(states_[active_[earlier]], states_[index])) ++earlier;
      if (earlier == kept) active_[kept++] = index;
      else parent_[index] = active_[earlier];
    }
    active_.resize(kept);
  }

  std::vector<State> states_;
  std::vector<std::size_t> parent_, active_;
  std::vector<Score> scores_;
  std::uint64_t last_transitions_ = 0;
};
