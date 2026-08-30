#include <algorithm>
#include <cassert>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// 状態全体をコピーせず、apply / revert で状態を移動するビームサーチ。
// 最初は simple-beam-search.hpp の方が使いやすい。
//
// State は現在状態、Action は1手、Score は評価値の型。
// expand(state) は現在状態から可能な Action の vector を返す。
// apply(state, action) と revert(state, action) は必ず逆の操作にする。
//
// 使い方:
// TreeBeamSearch<State, Move, long long> beam(initial, initial_score, 100);
// beam.step(expand, apply, revert, evaluate);
// // hash が同じ状態を1つにまとめたい場合:
// beam.step_with_key(expand, apply, revert, evaluate,
//                    [](const State& state) { return state.hash; });
// vector<Move> answer = beam.restore();
template <class State, class Action, class Score>
struct TreeBeamSearch {
  struct Node {
    int parent;
    int depth;
    std::optional<Action> action;
    Score score;
  };

  struct Candidate {
    int parent;
    Action action;
    Score score;
    int order;
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
      int beam_width,
      bool maximize = true)
      : state(std::move(initial_state)),
        beam_width(beam_width),
        maximize(maximize) {
    assert(beam_width > 0);
    nodes.push_back({-1, 0, std::nullopt, std::move(initial_score)});
    beam.push_back(0);
    candidate_buffer.reserve(static_cast<std::size_t>(beam_width) * 4);
  }

  // 候補が1つもなければ false。それ以外は1ターン進めて true。
  template <class Expand, class Apply, class Revert, class Evaluate>
  bool step(Expand expand, Apply apply, Revert revert, Evaluate evaluate) {
    candidate_buffer.clear();
    std::vector<Candidate>& candidates = candidate_buffer;
    int order = 0;

    for (int parent : beam) {
      move_to(parent, apply, revert);
      auto actions = expand(state);

      for (Action& action : actions) {
        apply(state, action);
        const Score score = evaluate(state);
        revert(state, action);
        candidates.push_back({parent, std::move(action), score, order++});
      }
    }

    if (candidates.empty()) return false;

    keep_best_candidates(candidates);

    beam.clear();
    beam.reserve(candidates.size());
    for (Candidate& candidate : candidates) {
      const int depth = nodes[candidate.parent].depth + 1;
      nodes.push_back({candidate.parent,
                       depth,
                       std::move(candidate.action),
                       std::move(candidate.score)});
      beam.push_back(static_cast<int>(nodes.size()) - 1);
    }
    return true;
  }

  // 同じ key の状態は、評価値が一番良い候補だけを残す。
  // make_key(state) は uint64_t、int、string などを返すようにする。
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
    std::vector<Candidate>& candidates = candidate_buffer;
    std::unordered_map<Key, int> index_by_key;
    int order = 0;

    for (int parent : beam) {
      move_to(parent, apply, revert);
      auto actions = expand(state);

      for (Action& action : actions) {
        apply(state, action);
        const Score score = evaluate(state);
        Key key = make_key(state);
        revert(state, action);
        Candidate candidate{parent, std::move(action), score, order++};
        const auto found = index_by_key.find(key);
        if (found == index_by_key.end()) {
          const int index = static_cast<int>(candidates.size());
          index_by_key.emplace(std::move(key), index);
          candidates.push_back(std::move(candidate));
        } else if (score_is_better(candidate.score,
                                   candidates[found->second].score)) {
          candidates[found->second] = std::move(candidate);
        }
      }
    }

    if (candidates.empty()) return false;

    keep_best_candidates(candidates);
    beam.clear();
    beam.reserve(candidates.size());

    for (Candidate& candidate : candidates) {
      const int depth = nodes[candidate.parent].depth + 1;
      nodes.push_back({candidate.parent,
                       depth,
                       std::move(candidate.action),
                       std::move(candidate.score)});
      beam.push_back(static_cast<int>(nodes.size()) - 1);
    }
    return true;
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
    while (nodes[node].parent != -1) {
      actions.push_back(*nodes[node].action);
      node = nodes[node].parent;
    }
    std::reverse(actions.begin(), actions.end());
    return actions;
  }

  template <class Apply, class Revert>
  void move_to(int target, Apply& apply, Revert& revert) {
    assert(0 <= target && target < static_cast<int>(nodes.size()));

    int from = current_node;
    int to = target;
    std::vector<int> path_down;

    while (nodes[from].depth > nodes[to].depth) {
      revert(state, *nodes[from].action);
      from = nodes[from].parent;
    }
    while (nodes[to].depth > nodes[from].depth) {
      path_down.push_back(to);
      to = nodes[to].parent;
    }
    while (from != to) {
      revert(state, *nodes[from].action);
      from = nodes[from].parent;
      path_down.push_back(to);
      to = nodes[to].parent;
    }

    std::reverse(path_down.begin(), path_down.end());
    for (int node : path_down) apply(state, *nodes[node].action);
    current_node = target;
  }

 private:
  bool score_is_better(const Score& a, const Score& b) const {
    return maximize ? b < a : a < b;
  }

  void keep_best_candidates(std::vector<Candidate>& candidates) const {
    const auto is_better = [&](const Candidate& a, const Candidate& b) {
      if (score_is_better(a.score, b.score)) return true;
      if (score_is_better(b.score, a.score)) return false;
      return a.order < b.order;
    };
    const int kept =
        std::min(beam_width, static_cast<int>(candidates.size()));
    std::partial_sort(candidates.begin(), candidates.begin() + kept,
                      candidates.end(), is_better);
    candidates.resize(kept);
  }
};
