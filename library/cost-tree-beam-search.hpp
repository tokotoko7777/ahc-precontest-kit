#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <queue>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// 1手で複数世代進める場合に使う、状態をコピーしないビームサーチ。
// 同じ generation に到着する候補ごとに、上位 beam_width 個を残す。
// expand(state) は可能なActionを並べたコンテナを返す。
// constコンテナはコピーし、非constコンテナの要素はmoveして消費する。
// apply と revert は必ず逆の操作にする。advance は正の整数にする。
// 不正な幅、最大世代、advance には invalid_argument を投げる。
// 各コールバック自身は探索中に例外を投げない前提。
// revert は、対応する apply の変更を完全に元へ戻すようにする。
//
// 使い方:
// struct Move { int add; int advance; };
// struct State { int value = 0; };
// CostTreeBeamSearch<State, Move, long long> beam(
//     State{}, 0, 100, 50);  // 幅100、generation 50まで
// while (beam.step(
//     [](const State&) { return vector<Move>{{1, 1}, {3, 2}}; },
//     [](State& s, Move m) { s.value += m.add; },
//     [](State& s, Move m) { s.value -= m.add; },
//     [](const State& s) { return (long long)s.value; },
//     [](const Move& m) { return m.advance; })) {
// }
// vector<Move> answer = beam.restore();
//
// hash が同じ状態を1つにまとめる場合は step_with_key を使う。
// Key は標準では uint64_t。string などを使う場合は第4テンプレート引数に指定する。
template <class State,
          class Action,
          class Score,
          class Key = std::uint64_t,
          class KeyHash = std::hash<Key>>
struct CostTreeBeamSearch {
  CostTreeBeamSearch(State initial_state,
                     Score initial_score,
                     int beam_width,
                     int max_generation,
                     bool maximize = true)
      : state_(std::move(initial_state)),
        beam_width_(beam_width),
        max_generation_(max_generation),
        maximize_(maximize) {
    if (beam_width_ <= 0) {
      throw std::invalid_argument("beam_width must be positive");
    }
    if (max_generation_ < 0) {
      throw std::invalid_argument("max_generation must be non-negative");
    }

    layers_.resize(static_cast<std::size_t>(max_generation_) + 1);
    first_candidate_.assign(
        static_cast<std::size_t>(max_generation_) + 1, -1);

    nodes_.push_back(Node{});
    nodes_[0].generation = 0;
    nodes_[0].alive = true;
    nodes_[0].scheduled = true;
    mark_.push_back(0);
    marked_first_child_.push_back(-1);
    marked_next_sibling_.push_back(-1);
    current_beam_rank_.push_back(0);

    beam_.push_back(
        Entry{0,
              std::move(initial_score),
              LogicalOrder{0, 0, 0},
              std::nullopt});
  }

  // 現在の generation を展開し、候補がある最小の generation へ進む。
  // 進めたら true。先がなければ現在のビームを残したまま false。
  template <class Expand,
            class Apply,
            class Revert,
            class Evaluate,
            class GetAdvance>
  bool step(Expand expand,
            Apply apply,
            Revert revert,
            Evaluate evaluate,
            GetAdvance get_advance) {
    return step_impl<false>(expand, apply, revert, evaluate, get_advance,
                            NoKeyMaker{});
  }

  // 同じ generation かつ同じ key の候補は、最良の1個だけを残す。
  // 1個のオブジェクトでは step と step_with_key を混ぜないこと。
  template <class Expand,
            class Apply,
            class Revert,
            class Evaluate,
            class GetAdvance,
            class MakeKey>
  bool step_with_key(Expand expand,
                     Apply apply,
                     Revert revert,
                     Evaluate evaluate,
                     GetAdvance get_advance,
                     MakeKey make_key) {
    return step_impl<true>(expand, apply, revert, evaluate, get_advance,
                           make_key);
  }

  template <class Expand,
            class Apply,
            class Revert,
            class Evaluate,
            class GetAdvance>
  int run(Expand expand,
          Apply apply,
          Revert revert,
          Evaluate evaluate,
          GetAdvance get_advance) {
    while (step(expand, apply, revert, evaluate, get_advance)) {
    }
    return generation_;
  }

  template <class Expand,
            class Apply,
            class Revert,
            class Evaluate,
            class GetAdvance,
            class MakeKey>
  int run_with_key(Expand expand,
                   Apply apply,
                   Revert revert,
                   Evaluate evaluate,
                   GetAdvance get_advance,
                   MakeKey make_key) {
    while (step_with_key(expand, apply, revert, evaluate, get_advance,
                         make_key)) {
    }
    return generation_;
  }

  int generation() const { return generation_; }

  int size() const { return static_cast<int>(beam_.size()); }

  // 長い探索で再確保を避けたい場合だけ使う。使わなくても正しく動く。
  // 削除nodeは再利用するので、同時に残る現在層・未来層・祖先が目安。
  void reserve_nodes(std::size_t count) {
    nodes_.reserve(count);
    mark_.reserve(count);
    marked_first_child_.reserve(count);
    marked_next_sibling_.reserve(count);
    current_beam_rank_.reserve(count);
  }

  void reserve_candidates(std::size_t count) {
    candidates_.reserve(count);
    choices_.reserve(count);
  }

  // 現在のビームを共有履歴木のDFSで巡回する。
  // visit(rank, state) のstateはそのrankの状態。終了時はrootへ戻る。
  template <class Visit, class Apply, class Revert>
  void for_each_state(Visit&& visit, Apply&& apply, Revert&& revert) {
    begin_marking();
    traversal_stack_.clear();
    traversal_stack_.push_back(
        TraversalFrame{0, marked_first_child_[0]});

    if (nodes_[0].scheduled && nodes_[0].generation == generation_) {
      visit(current_beam_rank_[0], static_cast<const State&>(state_));
    }

    while (!traversal_stack_.empty()) {
      TraversalFrame& frame = traversal_stack_.back();
      if (frame.next_child == -1) {
        const int node = frame.node;
        traversal_stack_.pop_back();
        if (node != 0) revert(state_, *nodes_[node].action);
        continue;
      }

      const int child = frame.next_child;
      frame.next_child = marked_next_sibling_[child];
      apply(state_, *nodes_[child].action);
      if (nodes_[child].scheduled &&
          nodes_[child].generation == generation_) {
        visit(current_beam_rank_[child],
              static_cast<const State&>(state_));
      }
      traversal_stack_.push_back(
          TraversalFrame{child, marked_first_child_[child]});
    }
  }

  const Score& best_score() const {
    assert(!beam_.empty());
    return beam_.front().score;
  }

  // rank=0 が現在の generation で最良の候補。
  std::vector<Action> restore(int rank = 0) const {
    assert(0 <= rank && rank < static_cast<int>(beam_.size()));
    int node = beam_[rank].node;
    std::vector<Action> actions;
    while (node != 0) {
      assert(nodes_[node].action.has_value());
      actions.push_back(*nodes_[node].action);
      node = nodes_[node].parent;
    }
    std::reverse(actions.begin(), actions.end());
    return actions;
  }

 private:
  struct Node {
    int parent = -1;
    int first_child = -1;
    int previous_sibling = -1;
    int next_sibling = -1;
    int generation = 0;
    bool alive = false;
    bool scheduled = false;
    std::optional<Action> action;
  };

  struct LogicalOrder {
    std::uint64_t step;
    int parent_rank;
    int action_index;
  };

  struct Entry {
    int node;
    Score score;
    LogicalOrder order;
    std::optional<Key> key;
  };

  struct Candidate {
    int parent;
    int next_same_generation;
    Action action;
    Score score;
    LogicalOrder order;
    std::optional<Key> key;
  };

  struct Choice {
    bool is_old;
    int index;
  };

  struct TraversalFrame {
    int node;
    int next_child;
  };

  struct NoKeyMaker {};

  State state_;
  int beam_width_;
  int max_generation_;
  bool maximize_;
  int generation_ = 0;
  int mode_ = 0;  // 0: 未決定、1: keyなし、2: keyあり
  std::uint64_t next_step_order_ = 1;
  std::uint64_t current_step_order_ = 0;

  std::vector<Node> nodes_;
  std::vector<int> free_nodes_;
  std::vector<Entry> beam_;
  std::vector<std::vector<Entry>> layers_;
  std::priority_queue<int, std::vector<int>, std::greater<int>>
      ready_generations_;

  std::vector<Candidate> candidates_;
  std::vector<int> first_candidate_;
  std::vector<int> touched_generations_;
  std::vector<Choice> choices_;
  std::unordered_map<Key, int, KeyHash> choice_by_key_;

  std::uint32_t mark_stamp_ = 0;
  std::vector<std::uint32_t> mark_;
  std::vector<int> marked_first_child_;
  std::vector<int> marked_next_sibling_;
  std::vector<int> current_beam_rank_;
  std::vector<TraversalFrame> traversal_stack_;

  bool score_is_nan(const Score& score) const {
    if constexpr (std::is_floating_point_v<Score>) {
      return std::isnan(score);
    } else {
      (void)score;
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

  const Score& choice_score(const Choice& choice,
                            const std::vector<Entry>& old_entries) const {
    return choice.is_old ? old_entries[choice.index].score
                         : candidates_[choice.index].score;
  }

  const LogicalOrder& choice_order(
      const Choice& choice,
      const std::vector<Entry>& old_entries) const {
    return choice.is_old ? old_entries[choice.index].order
                         : candidates_[choice.index].order;
  }

  bool order_is_earlier(const LogicalOrder& a,
                        const LogicalOrder& b) const {
    if (a.step != b.step) return a.step < b.step;
    if (a.parent_rank != b.parent_rank) {
      return a.parent_rank < b.parent_rank;
    }
    return a.action_index < b.action_index;
  }

  const Key& choice_key(const Choice& choice,
                        const std::vector<Entry>& old_entries) const {
    const std::optional<Key>& key =
        choice.is_old ? old_entries[choice.index].key
                      : candidates_[choice.index].key;
    assert(key.has_value());
    return *key;
  }

  bool choice_is_better(const Choice& a,
                        const Choice& b,
                        const std::vector<Entry>& old_entries) const {
    const Score& a_score = choice_score(a, old_entries);
    const Score& b_score = choice_score(b, old_entries);
    if (score_is_better(a_score, b_score)) return true;
    if (score_is_better(b_score, a_score)) return false;
    return order_is_earlier(choice_order(a, old_entries),
                            choice_order(b, old_entries));
  }

  int create_node(int parent, int generation, Action action) {
    int node;
    if (free_nodes_.empty()) {
      node = static_cast<int>(nodes_.size());
      nodes_.push_back(Node{});
      mark_.push_back(0);
      marked_first_child_.push_back(-1);
      marked_next_sibling_.push_back(-1);
      current_beam_rank_.push_back(-1);
    } else {
      node = free_nodes_.back();
      free_nodes_.pop_back();
    }

    Node& created = nodes_[node];
    created.parent = parent;
    created.first_child = -1;
    created.previous_sibling = -1;
    created.next_sibling = nodes_[parent].first_child;
    created.generation = generation;
    created.alive = true;
    created.scheduled = true;
    created.action.reset();
    created.action.emplace(std::move(action));
    mark_[node] = 0;
    marked_first_child_[node] = -1;
    marked_next_sibling_[node] = -1;
    current_beam_rank_[node] = -1;

    if (created.next_sibling != -1) {
      nodes_[created.next_sibling].previous_sibling = node;
    }
    nodes_[parent].first_child = node;
    return node;
  }

  void remove_scheduled_leaf(int node) {
    assert(0 <= node && node < static_cast<int>(nodes_.size()));
    assert(nodes_[node].alive);
    nodes_[node].scheduled = false;

    while (node != 0 && !nodes_[node].scheduled &&
           nodes_[node].first_child == -1) {
      const int parent = nodes_[node].parent;
      const int previous = nodes_[node].previous_sibling;
      const int next = nodes_[node].next_sibling;

      if (previous == -1) {
        nodes_[parent].first_child = next;
      } else {
        nodes_[previous].next_sibling = next;
      }
      if (next != -1) nodes_[next].previous_sibling = previous;

      nodes_[node].action.reset();
      nodes_[node].alive = false;
      nodes_[node].parent = -1;
      nodes_[node].previous_sibling = -1;
      nodes_[node].next_sibling = -1;
      free_nodes_.push_back(node);
      node = parent;
    }
  }

  void begin_marking() {
    ++mark_stamp_;
    if (mark_stamp_ == 0) {
      std::fill(mark_.begin(), mark_.end(), 0);
      mark_stamp_ = 1;
    }

    for (int rank = 0; rank < static_cast<int>(beam_.size()); ++rank) {
      current_beam_rank_[beam_[rank].node] = rank;
    }

    // 先頭（高順位）の親から展開される順になるよう、先頭挿入は逆順で行う。
    for (auto beam_it = beam_.rbegin(); beam_it != beam_.rend(); ++beam_it) {
      const Entry& entry = *beam_it;
      int node = entry.node;
      int child = -1;

      while (node != -1 && mark_[node] != mark_stamp_) {
        mark_[node] = mark_stamp_;
        marked_first_child_[node] = -1;
        marked_next_sibling_[node] = -1;
        if (child != -1) {
          marked_next_sibling_[child] = marked_first_child_[node];
          marked_first_child_[node] = child;
        }
        child = node;
        node = nodes_[node].parent;
      }

      if (child != -1 && node != -1) {
        marked_next_sibling_[child] = marked_first_child_[node];
        marked_first_child_[node] = child;
      }
    }
  }

  template <bool UseKey,
            class Expand,
            class Apply,
            class Revert,
            class Evaluate,
            class GetAdvance,
            class MakeKey>
  bool expand_node(int node,
                   Expand& expand,
                   Apply& apply,
                   Revert& revert,
                   Evaluate& evaluate,
                   GetAdvance& get_advance,
                   MakeKey& make_key) {
    assert(current_beam_rank_[node] >= 0);
    auto&& actions = expand(static_cast<const State&>(state_));
    int action_index = 0;
    for (auto&& expanded_action : actions) {
      Action action = std::move(expanded_action);
      const int this_action_index = action_index++;
      const long long advance =
          static_cast<long long>(get_advance(static_cast<const Action&>(action)));
      if (advance <= 0) {
        return false;
      }
      if (advance >
          static_cast<long long>(max_generation_ - generation_)) {
        continue;
      }
      const int next_generation = generation_ + static_cast<int>(advance);

      apply(state_, action);
      Score score = evaluate(static_cast<const State&>(state_));
      std::optional<Key> key;
      if constexpr (UseKey) {
        key.emplace(make_key(static_cast<const State&>(state_)));
      }
      revert(state_, action);

      if (first_candidate_[next_generation] == -1) {
        touched_generations_.push_back(next_generation);
      }
      const int candidate = static_cast<int>(candidates_.size());
      candidates_.push_back(
          Candidate{node,
                    first_candidate_[next_generation],
                    std::move(action),
                    std::move(score),
                    LogicalOrder{current_step_order_,
                                 current_beam_rank_[node],
                                 this_action_index},
                    std::move(key)});
      first_candidate_[next_generation] = candidate;
    }
    return true;
  }

  template <bool UseKey>
  void merge_generation(int generation) {
    std::vector<Entry>& old_entries = layers_[generation];
    choices_.clear();

    int new_count = 0;
    for (int candidate = first_candidate_[generation]; candidate != -1;
         candidate = candidates_[candidate].next_same_generation) {
      ++new_count;
    }
    choices_.reserve(old_entries.size() + new_count);

    if constexpr (UseKey) {
      choice_by_key_.clear();
      choice_by_key_.reserve(old_entries.size() + new_count);

      const auto add_choice = [&](Choice choice) {
        const Key& key = choice_key(choice, old_entries);
        const auto found = choice_by_key_.find(key);
        if (found == choice_by_key_.end()) {
          const int index = static_cast<int>(choices_.size());
          choice_by_key_.emplace(key, index);
          choices_.push_back(choice);
        } else if (choice_is_better(
                       choice, choices_[found->second], old_entries)) {
          choices_[found->second] = choice;
        }
      };

      for (int i = 0; i < static_cast<int>(old_entries.size()); ++i) {
        add_choice(Choice{true, i});
      }
      for (int candidate = first_candidate_[generation]; candidate != -1;
           candidate = candidates_[candidate].next_same_generation) {
        add_choice(Choice{false, candidate});
      }
    } else {
      for (int i = 0; i < static_cast<int>(old_entries.size()); ++i) {
        choices_.push_back(Choice{true, i});
      }
      for (int candidate = first_candidate_[generation]; candidate != -1;
           candidate = candidates_[candidate].next_same_generation) {
        choices_.push_back(Choice{false, candidate});
      }
    }

    assert(!choices_.empty());
    const auto is_better = [&](const Choice& a, const Choice& b) {
      return choice_is_better(a, b, old_entries);
    };
    const int kept =
        std::min(beam_width_, static_cast<int>(choices_.size()));
    if (kept < static_cast<int>(choices_.size())) {
      std::nth_element(choices_.begin(), choices_.begin() + kept,
                       choices_.end(), is_better);
      choices_.resize(kept);
    }
    std::sort(choices_.begin(), choices_.end(), is_better);

    std::vector<unsigned char> keep_old(old_entries.size(), 0);
    std::vector<Entry> next_entries;
    next_entries.reserve(choices_.size());

    for (const Choice& choice : choices_) {
      if (choice.is_old) {
        keep_old[choice.index] = 1;
        next_entries.push_back(std::move(old_entries[choice.index]));
      } else {
        Candidate& candidate = candidates_[choice.index];
        const int node = create_node(candidate.parent, generation,
                                     std::move(candidate.action));
        next_entries.push_back(
            Entry{node,
                  std::move(candidate.score),
                  candidate.order,
                  std::move(candidate.key)});
      }
    }

    std::vector<int> removed_nodes;
    for (int i = 0; i < static_cast<int>(old_entries.size()); ++i) {
      if (!keep_old[i]) removed_nodes.push_back(old_entries[i].node);
    }

    const bool was_empty = old_entries.empty();
    old_entries = std::move(next_entries);
    if (was_empty) ready_generations_.push(generation);
    for (int node : removed_nodes) remove_scheduled_leaf(node);
  }

  template <bool UseKey,
            class Expand,
            class Apply,
            class Revert,
            class Evaluate,
            class GetAdvance,
            class MakeKey>
  bool step_impl(Expand expand,
                 Apply apply,
                 Revert revert,
                 Evaluate evaluate,
                 GetAdvance get_advance,
                 MakeKey make_key) {
    const int requested_mode = UseKey ? 2 : 1;
    if (mode_ == 0) mode_ = requested_mode;
    if (mode_ != requested_mode) {
      throw std::invalid_argument(
          "do not mix keyed and non-keyed search");
    }

    candidates_.clear();
    touched_generations_.clear();
    current_step_order_ = next_step_order_++;
    bool valid_advance = true;
    for_each_state(
        [&](int rank, const State&) {
          if (!valid_advance) return;
          const int node = beam_[rank].node;
          valid_advance = expand_node<UseKey>(
              node, expand, apply, revert, evaluate, get_advance, make_key);
        },
        apply,
        revert);

    if (!valid_advance) {
      for (int next_generation : touched_generations_) {
        first_candidate_[next_generation] = -1;
      }
      candidates_.clear();
      touched_generations_.clear();
      throw std::invalid_argument("advance must be positive");
    }

    for (int next_generation : touched_generations_) {
      merge_generation<UseKey>(next_generation);
      first_candidate_[next_generation] = -1;
    }

    if (ready_generations_.empty()) return false;

    for (const Entry& entry : beam_) {
      remove_scheduled_leaf(entry.node);
    }
    beam_.clear();

    generation_ = ready_generations_.top();
    ready_generations_.pop();
    beam_ = std::move(layers_[generation_]);
    layers_[generation_].clear();
    return true;
  }
};
