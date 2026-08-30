#include <algorithm>
#include <cassert>
#include <optional>
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
  };

  State state;
  int beam_width;
  bool maximize;
  int current_node = 0;
  std::vector<Node> nodes;
  std::vector<int> beam;

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
  }

  // 候補が1つもなければ false。それ以外は1ターン進めて true。
  template <class Expand, class Apply, class Revert, class Evaluate>
  bool step(Expand expand, Apply apply, Revert revert, Evaluate evaluate) {
    std::vector<Candidate> candidates;

    for (int parent : beam) {
      move_to(parent, apply, revert);
      auto actions = expand(state);

      for (Action& action : actions) {
        apply(state, action);
        const Score score = evaluate(state);
        revert(state, action);
        candidates.push_back({parent, std::move(action), score});
      }
    }

    if (candidates.empty()) return false;

    std::stable_sort(
        candidates.begin(), candidates.end(), [&](const Candidate& a,
                                                   const Candidate& b) {
          return maximize ? b.score < a.score : a.score < b.score;
        });
    if (static_cast<int>(candidates.size()) > beam_width) {
      candidates.resize(beam_width);
    }

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
};
