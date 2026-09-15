#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// BEGIN LIBRARY: tree-beam-search.hpp
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
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/tree-beam-search.hpp

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
// revertを書くのが難しいが、Actionから次の順位を差分計算できる場合は
// action-beam-search.hppの方が単純に書ける。
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
//
// 幅から落ちる候補も含め、生成した全候補を調べたい場合は
// step_and_observe（key付きはstep_with_key_and_observe）を使う。
// observerは候補をapplyしたState上で、選抜前にちょうど1回呼ばれる。
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
    beam.reserve(static_cast<std::size_t>(beam_width_value));
    beam.push_back(0);
    candidate_buffer.reserve(static_cast<std::size_t>(beam_width_value) * 4);
    selection_buffer.reserve(static_cast<std::size_t>(beam_width_value) * 4);
    old_beam_buffer.reserve(static_cast<std::size_t>(beam_width_value));
    move_buffer.reserve(64);
  }

  void set_beam_width(int new_beam_width) {
    if (new_beam_width <= 0) {
      throw std::invalid_argument("beam_width must be positive");
    }
    if (new_beam_width > beam_width) {
      beam.reserve(static_cast<std::size_t>(new_beam_width));
      old_beam_buffer.reserve(static_cast<std::size_t>(new_beam_width));
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

  int size() const { return static_cast<int>(beam.size()); }

  // 直近stepの生成数、重複除去後の数、採用数。
  // buffered_peakは、同時に保持したAction候補の最大件数。
  std::size_t last_generated_count() const { return last_generated_count_; }
  std::size_t last_unique_count() const { return last_unique_count_; }
  std::size_t last_kept_count() const { return last_kept_count_; }
  std::size_t last_buffered_peak_count() const {
    return last_buffered_peak_count_;
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
  bool step(Expand&& expand,
            Apply&& apply,
            Revert&& revert,
            Evaluate&& evaluate) {
    NoObserver observer;
    return step_impl<false>(expand, apply, revert, evaluate, observer);
  }

  // observer(parent_rank, action, child_state, rank_score) を、生成した
  // 全候補について選抜前に呼ぶ。child_stateにはactionがapply済み。
  // observer内ではこのオブジェクトを変更せず、例外も投げないこと。
  template <class Expand,
            class Apply,
            class Revert,
            class Evaluate,
            class Observer>
  bool step_and_observe(Expand&& expand,
                        Apply&& apply,
                        Revert&& revert,
                        Evaluate&& evaluate,
                        Observer&& observer) {
    return step_impl<true>(expand, apply, revert, evaluate, observer);
  }

  // 同じ key の状態は、評価値が一番良い候補だけを残す。
  // 重複除去は上位 beam_width 件を選ぶ前に行う。
  template <class Expand,
            class Apply,
            class Revert,
            class Evaluate,
            class MakeKey>
  bool step_with_key(Expand&& expand,
                     Apply&& apply,
                     Revert&& revert,
                     Evaluate&& evaluate,
                     MakeKey&& make_key) {
    NoObserver observer;
    return step_with_key_impl<false>(
        expand, apply, revert, evaluate, make_key, observer);
  }

  // keyで重複除去する前の全候補をobserverで調べる。
  template <class Expand,
            class Apply,
            class Revert,
            class Evaluate,
            class MakeKey,
            class Observer>
  bool step_with_key_and_observe(Expand&& expand,
                                 Apply&& apply,
                                 Revert&& revert,
                                 Evaluate&& evaluate,
                                 MakeKey&& make_key,
                                 Observer&& observer) {
    return step_with_key_impl<true>(
        expand, apply, revert, evaluate, make_key, observer);
  }

  const Score& best_score() const {
    assert(!beam.empty());
    return nodes[beam.front()].score;
  }

  // rank=0 が現在のビームで最良の候補。outの容量は再利用する。
  void restore(int rank, std::vector<Action>& out) const {
    assert(0 <= rank && rank < static_cast<int>(beam.size()));
    int node = beam[rank];
    out.clear();
    out.reserve(static_cast<std::size_t>(nodes[node].depth));
    while (nodes[node].parent != -1) {
      out.push_back(*nodes[node].action);
      node = nodes[node].parent;
    }
    std::reverse(out.begin(), out.end());
  }

  std::vector<Action> restore(int rank = 0) const {
    std::vector<Action> actions;
    restore(rank, actions);
    return actions;
  }

  // observerで受け取ったparent_rankとactionから、その候補の経路を作る。
  // observer内で即時に呼ぶこと。parent_rankやaction、observerが受け取る
  // 各参照を保存して、step終了後に使ってはいけない。
  void restore_candidate(int parent_rank,
                         const Action& action,
                         std::vector<Action>& out) const {
    restore(parent_rank, out);
    out.push_back(action);
  }

  std::vector<Action> restore_candidate(
      int parent_rank, const Action& action) const {
    std::vector<Action> actions;
    restore_candidate(parent_rank, action, actions);
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
  struct NoObserver {};

  std::vector<int> selection_buffer;
  std::vector<int> old_beam_buffer;
  std::vector<int> move_buffer;
  std::size_t last_generated_count_ = 0;
  std::size_t last_unique_count_ = 0;
  std::size_t last_kept_count_ = 0;
  std::size_t last_buffered_peak_count_ = 0;

  void begin_candidate_step() {
    candidate_buffer.clear();
    selection_buffer.clear();
    last_generated_count_ = 0;
    last_unique_count_ = 0;
    last_kept_count_ = 0;
    last_buffered_peak_count_ = 0;
  }

  template <bool Observe,
            class Expand,
            class Apply,
            class Revert,
            class Evaluate,
            class Observer>
  bool step_impl(Expand& expand,
                 Apply& apply,
                 Revert& revert,
                 Evaluate& evaluate,
                 Observer& observer) {
    begin_candidate_step();
    for_each_active_leaf(
        [&](int parent) {
          auto&& actions = expand(state);
          std::uint64_t action_order = 0;
          for (auto&& expanded_action : actions) {
            Action action = std::move(expanded_action);
            apply(state, action);
            Score score = evaluate(state);
            if constexpr (Observe) {
              observer(nodes[parent].beam_rank,
                       static_cast<const Action&>(action),
                       static_cast<const State&>(state),
                       static_cast<const Score&>(score));
            }
            revert(state, action);
            ++last_generated_count_;
            candidate_buffer.push_back(
                {parent,
                 std::move(action),
                 std::move(score),
                 nodes[parent].beam_rank,
                 action_order++});
            last_buffered_peak_count_ = candidate_buffer.size();
          }
        },
        apply,
        revert);

    last_unique_count_ = last_generated_count_;
    return select_and_advance();
  }

  template <bool Observe,
            class Expand,
            class Apply,
            class Revert,
            class Evaluate,
            class MakeKey,
            class Observer>
  bool step_with_key_impl(Expand& expand,
                          Apply& apply,
                          Revert& revert,
                          Evaluate& evaluate,
                          MakeKey& make_key,
                          Observer& observer) {
    using Key = std::decay_t<decltype(make_key(state))>;

    begin_candidate_step();
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
            if constexpr (Observe) {
              observer(nodes[parent].beam_rank,
                       static_cast<const Action&>(action),
                       static_cast<const State&>(state),
                       static_cast<const Score&>(score));
            }
            revert(state, action);

            Candidate candidate{
                parent,
                std::move(action),
                std::move(score),
                nodes[parent].beam_rank,
                action_order++};
            ++last_generated_count_;
            // findの後で同じkeyを再hashして挿入しない。
            const auto [found, inserted] = index_by_key.try_emplace(
                std::move(key), static_cast<int>(candidate_buffer.size()));
            if (inserted) {
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

    last_unique_count_ = candidate_buffer.size();
    last_buffered_peak_count_ = candidate_buffer.size();
    return select_and_advance();
  }

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
    last_kept_count_ = static_cast<std::size_t>(kept);
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

    // old beam用bufferの容量を再利用し、新しいbeamのhot storageは
    // 同じvectorに固定する。予約後はheap確保を発生させない。
    old_beam_buffer.assign(beam.begin(), beam.end());
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
    for (int old_leaf : old_beam_buffer) prune_empty_branch(old_leaf);
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

// 問題依存コードをProblemへ集める、apply/revert型ビームの薄いRunner。
//
// 【使う人がmain.cpp側へ書く場所】
// 次のTODOだけを自分の問題に合わせる。Runner本体は通常変更しない。
// 空関数を配置済みの雛形: template/search/tree-beam.cpp
//
//   TODO: 【問題ごと】State、軽いMove、候補順位Scoreを書く。
//   using State, Move, Score
//   TODO: 【問題ごと】現在状態から試す合法手を列挙する。
//   generate_moves(const State&)       -> 次に試すMoveのコンテナ。
//   TODO: 【問題ごと】盤面、score、hashなどを1手分だけ進める。
//   apply_move(State&, Move&)          -> 1手進める。Moveへundo情報を書ける。
//   TODO: 【問題ごと】apply前と完全に同じ状態へ戻す。
//   revert_move(State&, const Move&)   -> apply_move前と完全に同じ状態へ戻す。
//   TODO: 【問題ごと】子Stateの順位値そのものを返す。
//   evaluate(const State&)             -> 子Stateの順位値そのもの。
//   TODO: 【必要な問題だけ】同一局面を表すkeyを書く。
//   make_key(const State&)             -> 同一局面のkey。step_with_key時だけ必要。
//
// Runnerは履歴木、DFS巡回、上位N個選択、重複除去、世代ループを担当する。
// ↓↓↓ ここから下はライブラリ本体。通常は編集しない。↓↓↓
template <class Problem>
struct TreeBeamRunner {
  using State = typename Problem::State;
  using Move = typename Problem::Move;
  using Score = typename Problem::Score;

  TreeBeamRunner(Problem& problem,
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
          return problem_.generate_moves(state);
        },
        [&](State& state, Move& move) { problem_.apply_move(state, move); },
        [&](State& state, const Move& move) {
          problem_.revert_move(state, move);
        },
        [&](const State& state) { return problem_.evaluate(state); });
  }

  bool step_with_key() {
    return beam_.step_with_key(
        [&](const State& state) -> decltype(auto) {
          return problem_.generate_moves(state);
        },
        [&](State& state, Move& move) { problem_.apply_move(state, move); },
        [&](State& state, const Move& move) {
          problem_.revert_move(state, move);
        },
        [&](const State& state) { return problem_.evaluate(state); },
        [&](const State& state) { return problem_.make_key(state); });
  }

  // key重複除去前の全候補をobserverで受け取る。
  // observer(parent_rank, move, child_state, rank_score)の形。
  template <class Observer>
  bool step_with_key_and_observe(Observer&& observer) {
    return beam_.step_with_key_and_observe(
        [&](const State& state) -> decltype(auto) {
          return problem_.generate_moves(state);
        },
        [&](State& state, Move& move) { problem_.apply_move(state, move); },
        [&](State& state, const Move& move) {
          problem_.revert_move(state, move);
        },
        [&](const State& state) { return problem_.evaluate(state); },
        [&](const State& state) { return problem_.make_key(state); },
        std::forward<Observer>(observer));
  }

  template <class Observer>
  bool step_and_observe(Observer&& observer) {
    return beam_.step_and_observe(
        [&](const State& state) -> decltype(auto) {
          return problem_.generate_moves(state);
        },
        [&](State& state, Move& move) { problem_.apply_move(state, move); },
        [&](State& state, const Move& move) {
          problem_.revert_move(state, move);
        },
        [&](const State& state) { return problem_.evaluate(state); },
        std::forward<Observer>(observer));
  }

  int run(int turns) {
    if (turns < 0) {
      throw std::invalid_argument("turns must be non-negative");
    }
    int advanced = 0;
    while (advanced < turns && step()) ++advanced;
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

  const Score& best_score() const { return beam_.best_score(); }
  // 現在の候補を全て調べる。visit(rank, const State&)で正式scoreを比較し、
  // 選んだrankをrestore(rank)へ渡せる。途中の近似評価と正式scoreが違う時に使う。
  // Stateは借用参照。保存せず、このRunnerをvisit内から変更しないこと。
  // 終了時はrootへ戻る。Problemに新しい関数を書く必要はない。
  template <class Visit>
  void for_each_state(Visit&& visit) {
    beam_.for_each_state(std::forward<Visit>(visit),
        [&](State& state, Move& move) { problem_.apply_move(state, move); },
        [&](State& state, const Move& move) { problem_.revert_move(state, move); });
  }
  int depth() const { return beam_.depth(); }
  int size() const { return beam_.size(); }
  std::vector<Move> restore(int rank = 0) const {
    return beam_.restore(rank);
  }
  void restore(int rank, std::vector<Move>& out) const {
    beam_.restore(rank, out);
  }
  std::vector<Move> restore_candidate(
      int parent_rank, const Move& move) const {
    return beam_.restore_candidate(parent_rank, move);
  }
  void restore_candidate(int parent_rank,
                         const Move& move,
                         std::vector<Move>& out) const {
    beam_.restore_candidate(parent_rank, move, out);
  }
  void set_width(int width) { beam_.set_beam_width(width); }
  void reserve_nodes(std::size_t count) { beam_.reserve_nodes(count); }
  void reserve_candidates(std::size_t count) {
    beam_.reserve_candidates(count);
  }
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

 private:
  Problem& problem_;
  TreeBeamSearch<State, Move, Score> beam_;
};
// END LIBRARY: tree-beam-search.hpp
// BEGIN LIBRARY: radix-heap.hpp
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/radix-heap.hpp

// 取り出すキーが単調非減少になる場合の高速な優先度付きキュー。
// Dijkstra の「距離」のような 0 以上の整数キーに使える。
// 使い方:
// RadixHeap<int> queue;
// queue.push(distance, vertex);
// auto [distance, vertex] = queue.pop();
template <class Value>
struct RadixHeap {
  using Entry = std::pair<std::uint64_t, Value>;

  std::array<std::vector<Entry>, 65> buckets;
  std::uint64_t last_key = 0;
  std::size_t element_count = 0;

  bool empty() const { return element_count == 0; }
  std::size_t size() const { return element_count; }
  std::uint64_t last() const { return last_key; }

  void push(std::uint64_t key, Value value) {
    assert(last_key <= key);
    buckets[bucket_index(key ^ last_key)].push_back(
        {key, std::move(value)});
    ++element_count;
  }

  Entry pop() {
    assert(!empty());
    if (buckets[0].empty()) redistribute();

    Entry result = std::move(buckets[0].back());
    buckets[0].pop_back();
    --element_count;
    return result;
  }

  void clear() {
    for (auto& bucket : buckets) bucket.clear();
    last_key = 0;
    element_count = 0;
  }

 private:
  static int bucket_index(std::uint64_t difference) {
    if (difference == 0) return 0;
    return 64 - __builtin_clzll(difference);
  }

  void redistribute() {
    int source = 1;
    while (buckets[source].empty()) ++source;

    last_key = buckets[source][0].first;
    for (const Entry& entry : buckets[source]) {
      last_key = std::min(last_key, entry.first);
    }
    for (Entry& entry : buckets[source]) {
      buckets[bucket_index(entry.first ^ last_key)].push_back(
          std::move(entry));
    }
    buckets[source].clear();
  }
};
// END LIBRARY: radix-heap.hpp

// AHC021 "Pyramid Sorting" の公式入力分布と公式得点を使う。
// https://atcoder.jp/contests/ahc021/tasks/ahc021_a
//
// 小さい番号から「親が全て確定済みのマス」へ運ぶ。
// 同じ距離なら大きい球を下へ押し下げる経路を高く評価し、
// 次の球をどの前線マスへ運ぶかをビームで比較する。
//
// 【問題に合わせて書き換える場所】
//   PyramidProblem の State / Move と5関数。
//   generate_moves / apply_move / revert_move / evaluate / make_key
//
// 【ライブラリが担当する場所】
//   Stateは1個だけ保持する。履歴木のDFS、apply/revertの呼び分け、
//   key重複除去、上位N個の選択、465世代のループはTreeBeamRunnerが行う。

constexpr int N = 30;
constexpr int CELL_COUNT = N * (N + 1) / 2;
constexpr int MAX_OPERATIONS = 10000;

struct PyramidProblem {
  // TODO: 【問題ごと】1手とundoに必要な情報をMoveへ書く。
  struct Move {
    // path[0]に対象の小さい球がいる。隣へ順番にswapし、
    // path.back()を今回の確定マスにする。
    std::vector<std::uint16_t> path;
    int rank_cost = 0;
  };

  // TODO: 【問題ごと】現在状態と、差分更新するscore・hash・cacheを書く。
  struct State {
    std::array<std::uint16_t, CELL_COUNT> value{};
    std::array<std::uint16_t, CELL_COUNT> position_of_value{};
    std::array<std::uint8_t, CELL_COUNT> fixed{};
    std::array<std::uint32_t, N> frontier{}; // 親が全て確定済みの未確定マス。
    int next_value = 0;
    int operations = 0;
    long long rank_cost = 0;
    std::uint64_t hash = 0;
  };

  // TODO: 【問題ごと】候補順位の型を選ぶ。この例は小さい方が良い。
  using Score = long long;

  // TODO: 【問題ごと】全Stateで共通の入力・隣接表・事前計算を置く。
  std::array<int, CELL_COUNT> row{};
  std::array<int, CELL_COUNT> column{};
  std::array<std::vector<int>, CELL_COUNT> adjacent;
  // 探索用の作業buffer。Stateの一部ではなく、各候補生成でclearして容量を再利用。
  mutable RadixHeap<int> path_queue;

  PyramidProblem() {
    for (int r = 0; r < N; ++r) {
      for (int c = 0; c <= r; ++c) {
        const int vertex = id(r, c);
        row[vertex] = r;
        column[vertex] = c;
      }
    }
    for (int r = 0; r < N; ++r) {
      for (int c = 0; c <= r; ++c) {
        const int vertex = id(r, c);
        const auto add = [&](int next_row, int next_column) {
          if (next_row < 0 || next_row >= N || next_column < 0 ||
              next_column > next_row) {
            return;
          }
          adjacent[vertex].push_back(id(next_row, next_column));
        };
        add(r, c - 1);
        add(r, c + 1);
        add(r - 1, c - 1);
        add(r - 1, c);
        add(r + 1, c);
        add(r + 1, c + 1);
      }
    }
  }

  static int id(int row, int column) {
    return row * (row + 1) / 2 + column;
  }

  static std::uint64_t hash_token(int position, int value) {
    std::uint64_t x = static_cast<std::uint64_t>(position) * CELL_COUNT +
                      value + 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
  }

  // TODO: 【問題ごと】必ず合法な初期Stateを作る。
  State initial_state(const std::array<int, CELL_COUNT>& permutation) const {
    State state;
    state.frontier[0] = 1;
    for (int position = 0; position < CELL_COUNT; ++position) {
      const int value = permutation[position];
      state.value[position] = static_cast<std::uint16_t>(value);
      state.position_of_value[value] =
          static_cast<std::uint16_t>(position);
      state.hash ^= hash_token(position, value);
    }
    return state;
  }

  bool parents_are_fixed(const State& state, int vertex) const {
    const int r = row[vertex];
    const int c = column[vertex];
    if (r == 0) return true;
    if (c > 0 && !state.fixed[id(r - 1, c - 1)]) return false;
    if (c < r && !state.fixed[id(r - 1, c)]) return false;
    return true;
  }

  // 小さい球が上へ動く時、交換相手の大きい球が下へ下がるほど嬉しい。
  int edge_cost(const State& state, int from, int to) const {
    if (row[to] < row[from]) return CELL_COUNT - state.value[to];
    return CELL_COUNT;
  }

  void set_frontier(State& state, int vertex, bool present) const {
    const std::uint32_t bit = std::uint32_t{1} << column[vertex];
    if (present) state.frontier[row[vertex]] |= bit;
    else state.frontier[row[vertex]] &= ~bit;
  }

  // TODO: 【問題ごと】現在Stateから試す合法Moveを全て返す。
  std::vector<Move> generate_moves(const State& state) const {
    if (state.next_value == CELL_COUNT) return {};
    const int start = state.position_of_value[state.next_value];
    std::array<int, CELL_COUNT> targets;
    int target_count = 0;
    for (int r = 0; r < N; ++r) {
      std::uint32_t mask = state.frontier[r];
      while (mask) {
        const int c = __builtin_ctz(mask);
        mask &= mask - 1;
        targets[target_count++] = id(r, c);
      }
    }
    if (target_count == 0) throw std::runtime_error("missing pyramid frontier");
    int unsettled_targets = target_count;
    constexpr int INF = std::numeric_limits<int>::max() / 4;
    std::array<int, CELL_COUNT> distance;
    std::array<int, CELL_COUNT> previous;
    distance.fill(INF);
    previous.fill(-1);
    auto& queue = path_queue;
    queue.clear();
    distance[start] = 0;
    // (距離, 頂点番号)の辞書順を整数1個で表し、同距離の経路も従来と同じにする。
    // 辺費用は1以上なので、追加keyは最後にpopしたkey以上になる。
    queue.push(start, start);
    while (!queue.empty()) {
      const auto [packed_distance, vertex] = queue.pop();
      const int current_distance = static_cast<int>(packed_distance / CELL_COUNT);
      if (distance[vertex] != current_distance) continue;
      // 必要な行き先の最短路が全て確定したら打ち切る。非負辺なので近似ではない。
      if ((state.frontier[row[vertex]] >> column[vertex]) & 1U) {
        if (--unsettled_targets == 0) break;
      }
      for (int next : adjacent[vertex]) {
        if (state.fixed[next]) continue;
        const int next_distance =
            current_distance + edge_cost(state, vertex, next);
        if (next_distance < distance[next]) {
          distance[next] = next_distance;
          previous[next] = vertex;
          queue.push(static_cast<std::uint64_t>(next_distance) * CELL_COUNT + next, next);
        }
      }
    }

    std::vector<Move> moves;
    moves.reserve(N);
    for (int index = 0; index < target_count; ++index) {
      const int target = targets[index];
      if (distance[target] == INF) continue;
      Move move;
      move.rank_cost = distance[target];
      // 経路の長さだけ数えて1回で確保。push_backの容量拡張を候補ごとに繰り返さない。
      int path_size = 0;
      for (int vertex = target; vertex != -1; vertex = previous[vertex]) ++path_size;
      move.path.resize(path_size);
      for (int vertex = target, path_index = path_size; vertex != -1; vertex = previous[vertex]) {
        move.path[--path_index] = static_cast<std::uint16_t>(vertex);
      }
      if (move.path.empty() || move.path.front() != start) continue;
      moves.push_back(std::move(move));
    }
    std::sort(moves.begin(), moves.end(), [](const Move& left,
                                              const Move& right) {
      if (left.rank_cost != right.rank_cost) {
        return left.rank_cost < right.rank_cost;
      }
      return left.path.back() < right.path.back();
    });
    return moves;
  }

  void swap_vertices(State& state, int first, int second) const {
    const int first_value = state.value[first];
    const int second_value = state.value[second];
    state.hash ^= hash_token(first, first_value);
    state.hash ^= hash_token(second, second_value);
    state.hash ^= hash_token(first, second_value);
    state.hash ^= hash_token(second, first_value);
    std::swap(state.value[first], state.value[second]);
    state.position_of_value[first_value] =
        static_cast<std::uint16_t>(second);
    state.position_of_value[second_value] =
        static_cast<std::uint16_t>(first);
  }

  // TODO: 【問題ごと】盤面と全cacheをMove 1手分だけ差分更新する。
  void apply_move(State& state, Move& move) const {
    for (std::size_t i = 1; i < move.path.size(); ++i) {
      swap_vertices(state, move.path[i - 1], move.path[i]);
    }
    const int target = move.path.back();
    state.fixed[target] = 1;
    set_frontier(state, target, false);
    if (row[target] + 1 < N) {
      for (int delta = 0; delta < 2; ++delta) {
        const int child = id(row[target] + 1, column[target] + delta);
        if (parents_are_fixed(state, child)) set_frontier(state, child, true);
      }
    }
    ++state.next_value;
    state.operations += static_cast<int>(move.path.size()) - 1;
    state.rank_cost += move.rank_cost;
  }

  // TODO: 【問題ごと】apply直前と完全に同じStateへ戻す。
  void revert_move(State& state, const Move& move) const {
    const int target = move.path.back();
    state.rank_cost -= move.rank_cost;
    state.operations -= static_cast<int>(move.path.size()) - 1;
    --state.next_value;
    if (row[target] + 1 < N) {
      // apply前は親targetが未確定なので、この2マスは必ず前線の外だった。
      set_frontier(state, id(row[target] + 1, column[target]), false);
      set_frontier(state, id(row[target] + 1, column[target] + 1), false);
    }
    set_frontier(state, target, true);
    state.fixed[target] = 0;
    for (std::size_t i = move.path.size(); i > 1; --i) {
      swap_vertices(state, move.path[i - 1], move.path[i - 2]);
    }
  }

  // TODO: 【問題ごと】現在Stateの順位値そのものを返す。差分値ではない。
  Score evaluate(const State& state) const { return state.rank_cost; }
  // TODO: 【必要な問題だけ】同じ未来を持つ局面が同じになるkeyを返す。
  std::uint64_t make_key(const State& state) const { return state.hash; }

  int count_errors(const State& state) const {
    int errors = 0;
    for (int r = 0; r + 1 < N; ++r) {
      for (int c = 0; c <= r; ++c) {
        const int upper = id(r, c);
        errors += state.value[upper] > state.value[id(r + 1, c)];
        errors += state.value[upper] > state.value[id(r + 1, c + 1)];
      }
    }
    return errors;
  }

  int official_score(const State& state) const {
    const int errors = count_errors(state);
    if (errors == 0) return 100000 - 5 * state.operations;
    return 50000 - 50 * errors;
  }

  void validate(const State& state) const {
    if (state.next_value != CELL_COUNT || state.operations > MAX_OPERATIONS ||
        count_errors(state) != 0) {
      throw std::runtime_error("constructed pyramid is not a legal solution");
    }
    std::uint64_t expected_hash = 0;
    for (int position = 0; position < CELL_COUNT; ++position) {
      const int value = state.value[position];
      if (state.position_of_value[value] != position ||
          !state.fixed[position]) {
        throw std::runtime_error("position or fixed cache is inconsistent");
      }
      expected_hash ^= hash_token(position, value);
    }
    if (expected_hash != state.hash) {
      throw std::runtime_error("incremental hash is inconsistent");
    }
    for (std::uint32_t mask : state.frontier) {
      if (mask != 0) throw std::runtime_error("completed pyramid has frontier entries");
    }
  }
};

struct PyramidResult {
  int score = 0;
  int operations = 0;
  double milliseconds = 0.0;
  std::vector<PyramidProblem::Move> answer;
};

#ifndef AHC021_FINAL_SCORE_SELECTION
#define AHC021_FINAL_SCORE_SELECTION 1
#endif

PyramidResult solve_case(const std::array<int, CELL_COUNT>& input,
                         int width) {
  PyramidProblem problem;
  const PyramidProblem::State initial = problem.initial_state(input);
  TreeBeamRunner<PyramidProblem> beam(
      problem, initial, problem.evaluate(initial), width, false);
  beam.reserve_nodes(1 + static_cast<std::size_t>(width) * CELL_COUNT);
  beam.reserve_candidates(static_cast<std::size_t>(width) * N);

  const auto start = std::chrono::steady_clock::now();
  const int advanced = beam.run_with_key(CELL_COUNT);
  if (advanced != CELL_COUNT) {
    throw std::runtime_error("beam stopped before placing every value");
  }

  int answer_rank = 0;
  auto answer_rank_score = beam.best_score();
#if AHC021_FINAL_SCORE_SELECTION
  // TODO(AHC021): 完成解を比較する本来の得点。全マス確定済みなので交換回数が少ない方が良い。
  // 途中評価rank_costの1位が正式scoreでも1位とは限らない。Stateのコピーはせず巡回する。
  int fewest_operations = std::numeric_limits<int>::max();
  beam.for_each_state([&](int rank, const PyramidProblem::State& state) {
    if (state.operations < fewest_operations ||
        (state.operations == fewest_operations && rank < answer_rank)) {
      fewest_operations = state.operations;
      answer_rank = rank;
      answer_rank_score = problem.evaluate(state);
    }
  });
#endif
  const auto finish = std::chrono::steady_clock::now();
  std::vector<PyramidProblem::Move> answer = beam.restore(answer_rank);
  PyramidProblem::State check = initial;
  for (PyramidProblem::Move move : answer) problem.apply_move(check, move);
  problem.validate(check);
  if (problem.evaluate(check) != answer_rank_score) {
    throw std::runtime_error("restored answer and rank score disagree");
  }
  return {problem.official_score(check),
          check.operations,
          std::chrono::duration<double, std::milli>(finish - start).count(),
          std::move(answer)};
}

#ifndef AHC021_BEAM_WIDTH
#define AHC021_BEAM_WIDTH 300 // TODO(AHC021): 提出環境で時間を測り、ビーム幅を調整する。
#endif

// TODO(AHC021): 入出力だけを問題に合わせて書く。ライブラリ本体の変更は不要。
int main() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);
  std::array<int, CELL_COUNT> input{};
  for (int& value : input) if (!(std::cin >> value)) return 1;
  auto sorted = input;
  std::sort(sorted.begin(), sorted.end());
  for (int i = 0; i < CELL_COUNT; ++i) if (sorted[i] != i) return 1;
  const auto result = solve_case(input, AHC021_BEAM_WIDTH);
  const PyramidProblem problem;
  std::cout << result.operations << '\n';
  for (const auto& move : result.answer) {
    for (std::size_t i = 1; i < move.path.size(); ++i) {
      const int from = move.path[i - 1], to = move.path[i];
      std::cout << problem.row[from] << ' ' << problem.column[from] << ' '
                << problem.row[to] << ' ' << problem.column[to] << '\n';
    }
  }
  return 0;
}
