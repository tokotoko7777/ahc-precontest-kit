#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// 状態全体をコピーせず、apply / revert で状態を移動するビームサーチ。
// 全ての行動で世代がちょうど1進む問題に使う。
// 行動ごとに2世代、3世代と飛ぶ場合は cost-tree-beam-search.hpp を使う。
//
// 生き残った経路を木として共有し、その木をDFSで1回だけ巡回する。
// 各候補へrootから移動し直さないため、深い探索でも状態更新回数を抑えやすい。
//
// State は現在状態、Action は1手、Score は評価値の型。
// expand(state) は現在状態から可能な Action を並べたコンテナを返す。
// constコンテナはコピーし、非constコンテナの要素はmoveして消費する。
// apply(state, action) と revert(state, action) は必ず逆の操作にする。
// 各コールバック自身は探索中に例外を投げない前提。
//
// 使い方:
// TreeBeamSearch<State, Move, long long> beam(initial, initial_score, 200);
// for (int turn = 0; turn < 100; ++turn) {
//   if (!beam.step(expand, apply, revert, evaluate)) break;
// }
// vector<Move> answer = beam.restore();
//
// 同じ盤面を1つにまとめる場合:
// beam.step_with_key(expand, apply, revert, evaluate,
//                    [](const State& s) { return s.hash; });
template <class State, class Action, class Score>
struct TreeBeamSearch {
  struct Node {
    int parent;
    int depth;
    std::optional<Action> action;
    Score score;

    // 現在のビームへ続く枝だけを、配列上の連結リストで持つ。
    int first_child = -1;
    int previous_sibling = -1;
    int next_sibling = -1;
    bool active = true;
    int beam_rank = -1;
  };

  struct Candidate {
    int parent;
    Action action;
    Score score;
    int parent_rank;
    std::uint64_t action_order;
  };

  State state;
  int beam_width;
  bool maximize;
  int current_node = 0;
  std::vector<Node> nodes;
  std::vector<int> beam;
  std::vector<Candidate> candidate_buffer;

  TreeBeamSearch(
      State initial_state,
      Score initial_score,
      int beam_width_value,
      bool maximize_value = true)
      : state(std::move(initial_state)),
        beam_width(beam_width_value),
        maximize(maximize_value) {
    if (beam_width_value <= 0) {
      throw std::invalid_argument("beam_width must be positive");
    }
    nodes.push_back(
        {-1, 0, std::nullopt, std::move(initial_score),
         -1, -1, -1, true, 0});
    beam.push_back(0);
    candidate_buffer.reserve(static_cast<std::size_t>(beam_width_value) * 4);
    selection_buffer.reserve(static_cast<std::size_t>(beam_width_value) * 4);
    move_buffer.reserve(64);
  }

  void set_beam_width(int new_beam_width) {
    if (new_beam_width <= 0) {
      throw std::invalid_argument("beam_width must be positive");
    }
    beam_width = new_beam_width;
    if (beam.size() > static_cast<std::size_t>(beam_width)) {
      for (std::size_t rank = static_cast<std::size_t>(beam_width);
           rank < beam.size(); ++rank) {
        prune_empty_branch(beam[rank]);
      }
      beam.resize(static_cast<std::size_t>(beam_width));
    }
  }

  // 長い探索で再確保を避けたい場合だけ使う。使わなくても正しく動く。
  // nodeは再利用しないので、上限の目安は1 + 幅 * 成功step数。
  void reserve_nodes(std::size_t count) { nodes.reserve(count); }

  void reserve_candidates(std::size_t count) {
    candidate_buffer.reserve(count);
    selection_buffer.reserve(count);
  }

  int depth() const {
    assert(!beam.empty());
    return nodes[beam.front()].depth;
  }

  // 現在のビームを共有履歴木のDFSで巡回する。
  // visit(rank, state) のstateはそのrankの状態。終了時はrootへ戻る。
  template <class Visit, class Apply, class Revert>
  void for_each_state(Visit&& visit, Apply&& apply, Revert&& revert) {
    for_each_active_leaf(
        [&](int node) {
          visit(nodes[node].beam_rank, static_cast<const State&>(state));
        },
        apply,
        revert);
  }

  // 候補が1つもなければ false。それ以外は1世代進めて true。
  template <class Expand, class Apply, class Revert, class Evaluate>
  bool step(Expand expand, Apply apply, Revert revert, Evaluate evaluate) {
    candidate_buffer.clear();
    for_each_active_leaf(
        [&](int parent) {
          auto&& actions = expand(state);
          std::uint64_t action_order = 0;
          for (auto&& expanded_action : actions) {
            Action action = std::move(expanded_action);
            apply(state, action);
            Score score = evaluate(state);
            revert(state, action);
            candidate_buffer.push_back({parent,
                                        std::move(action),
                                        std::move(score),
                                        nodes[parent].beam_rank,
                                        action_order++});
          }
        },
        apply,
        revert);

    return select_and_advance();
  }

  // 同じ key の状態は、評価値が一番良い候補だけを残す。
  // 重複除去は上位 beam_width 件を選ぶ前に行う。
  template <class Expand,
            class Apply,
            class Revert,
            class Evaluate,
            class MakeKey>
  bool step_with_key(
      Expand expand,
      Apply apply,
      Revert revert,
      Evaluate evaluate,
      MakeKey make_key) {
    using Key = std::decay_t<decltype(make_key(state))>;

    candidate_buffer.clear();
    std::unordered_map<Key, int> index_by_key;
    index_by_key.reserve(static_cast<std::size_t>(beam_width) * 4);
    for_each_active_leaf(
        [&](int parent) {
          auto&& actions = expand(state);
          std::uint64_t action_order = 0;
          for (auto&& expanded_action : actions) {
            Action action = std::move(expanded_action);
            apply(state, action);
            Score score = evaluate(state);
            Key key = make_key(state);
            revert(state, action);

            Candidate candidate{
                parent,
                std::move(action),
                std::move(score),
                nodes[parent].beam_rank,
                action_order++};
            const auto found = index_by_key.find(key);
            if (found == index_by_key.end()) {
              const int index = static_cast<int>(candidate_buffer.size());
              index_by_key.emplace(std::move(key), index);
              candidate_buffer.push_back(std::move(candidate));
            } else if (candidate_is_better(
                           candidate,
                           candidate_buffer[found->second])) {
              candidate_buffer[found->second] = std::move(candidate);
            }
          }
        },
        apply,
        revert);

    return select_and_advance();
  }

  const Score& best_score() const {
    assert(!beam.empty());
    return nodes[beam.front()].score;
  }

  // rank=0 が現在のビームで最良の候補。
  std::vector<Action> restore(int rank = 0) const {
    assert(0 <= rank && rank < static_cast<int>(beam.size()));
    int node = beam[rank];
    std::vector<Action> actions;
    actions.reserve(static_cast<std::size_t>(nodes[node].depth));
    while (nodes[node].parent != -1) {
      actions.push_back(*nodes[node].action);
      node = nodes[node].parent;
    }
    std::reverse(actions.begin(), actions.end());
    return actions;
  }

  // stateを保存済みnodeの状態へ動かす。通常はstepが自動で管理する。
  template <class Apply, class Revert>
  void move_to(int target, Apply& apply, Revert& revert) {
    assert(0 <= target && target < static_cast<int>(nodes.size()));

    int from = current_node;
    int to = target;
    move_buffer.clear();

    while (nodes[from].depth > nodes[to].depth) {
      revert(state, *nodes[from].action);
      from = nodes[from].parent;
    }
    while (nodes[to].depth > nodes[from].depth) {
      move_buffer.push_back(to);
      to = nodes[to].parent;
    }
    while (from != to) {
      revert(state, *nodes[from].action);
      from = nodes[from].parent;
      move_buffer.push_back(to);
      to = nodes[to].parent;
    }

    std::reverse(move_buffer.begin(), move_buffer.end());
    for (int node : move_buffer) apply(state, *nodes[node].action);
    current_node = target;
  }

 private:
  std::vector<int> selection_buffer;
  std::vector<int> move_buffer;

  bool score_is_nan(const Score& value) const {
    if constexpr (std::is_floating_point_v<Score>) {
      return std::isnan(value);
    } else {
      static_cast<void>(value);
      return false;
    }
  }

  // 浮動小数点のNaNは常に最下位。同点なら現在の親rank、行動順で決める。
  bool score_is_better(const Score& a, const Score& b) const {
    const bool a_nan = score_is_nan(a);
    const bool b_nan = score_is_nan(b);
    if (a_nan != b_nan) return !a_nan;
    if (a_nan) return false;
    return maximize ? b < a : a < b;
  }

  bool candidate_is_better(
      const Candidate& first, const Candidate& second) const {
    if (score_is_better(first.score, second.score)) return true;
    if (score_is_better(second.score, first.score)) return false;
    if (first.parent_rank != second.parent_rank) {
      return first.parent_rank < second.parent_rank;
    }
    return first.action_order < second.action_order;
  }

  template <class VisitLeaf, class Apply, class Revert>
  void for_each_active_leaf(
      VisitLeaf visit_leaf, Apply& apply, Revert& revert) {
    // 外からmove_toを呼ばれていても、探索開始時はrootへ戻す。
    move_to(0, apply, revert);

    int node = 0;
    bool entering = true;
    while (true) {
      if (entering) {
        if (node != 0) {
          apply(state, *nodes[node].action);
          current_node = node;
        }

        const int child = nodes[node].first_child;
        if (child == -1) {
          visit_leaf(node);
        } else {
          node = child;
          entering = true;
          continue;
        }
      }

      if (node == 0) break;

      const int sibling = nodes[node].next_sibling;
      const int parent = nodes[node].parent;
      revert(state, *nodes[node].action);
      current_node = parent;

      if (sibling != -1) {
        node = sibling;
        entering = true;
      } else {
        node = parent;
        entering = false;
      }
    }

    current_node = 0;
  }

  bool select_and_advance() {
    if (candidate_buffer.empty()) return false;

    const int candidate_count = static_cast<int>(candidate_buffer.size());
    const int kept = std::min(beam_width, candidate_count);
    selection_buffer.resize(static_cast<std::size_t>(candidate_count));
    std::iota(selection_buffer.begin(), selection_buffer.end(), 0);

    const auto better_index = [&](int a, int b) {
      return candidate_is_better(candidate_buffer[a], candidate_buffer[b]);
    };

    if (kept < candidate_count) {
      std::nth_element(
          selection_buffer.begin(),
          selection_buffer.begin() + kept,
          selection_buffer.end(),
          better_index);
      selection_buffer.resize(static_cast<std::size_t>(kept));
    }
    std::sort(selection_buffer.begin(), selection_buffer.end(), better_index);

    const std::vector<int> old_beam = beam;
    beam.clear();
    beam.reserve(static_cast<std::size_t>(kept));

    // 選ばれた候補だけをNodeへ変換する。
    for (int index : selection_buffer) {
      Candidate& candidate = candidate_buffer[index];
      const int parent = candidate.parent;
      const int node = static_cast<int>(nodes.size());
      const int rank = static_cast<int>(beam.size());
      nodes.push_back({parent,
                       nodes[parent].depth + 1,
                       std::optional<Action>(std::move(candidate.action)),
                       std::move(candidate.score),
                       -1,
                       -1,
                       -1,
                       true,
                       rank});
      attach_child(parent, node);
      beam.push_back(node);
    }

    // 子が1つも選ばれなかった古い葉と、不要になった祖先を外す。
    for (int old_leaf : old_beam) prune_empty_branch(old_leaf);
    return true;
  }

  void attach_child(int parent, int child) {
    const int old_first = nodes[parent].first_child;
    nodes[child].next_sibling = old_first;
    if (old_first != -1) nodes[old_first].previous_sibling = child;
    nodes[parent].first_child = child;
  }

  void prune_empty_branch(int node) {
    while (node != 0 && nodes[node].active &&
           nodes[node].first_child == -1) {
      const int parent = nodes[node].parent;
      const int previous = nodes[node].previous_sibling;
      const int next = nodes[node].next_sibling;

      if (previous == -1) {
        nodes[parent].first_child = next;
      } else {
        nodes[previous].next_sibling = next;
      }
      if (next != -1) nodes[next].previous_sibling = previous;

      nodes[node].previous_sibling = -1;
      nodes[node].next_sibling = -1;
      nodes[node].active = false;
      node = parent;
    }
  }
};
