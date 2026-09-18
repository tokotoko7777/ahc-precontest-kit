// GCC提出用の最適化。診断時は -DAHC_DISABLE_GCC_OPTIMIZE で無効化。
#if defined(__GNUC__) && !defined(__clang__) && !defined(AHC_DISABLE_GCC_OPTIMIZE)
#pragma GCC optimize("O3")
#endif

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>

#include "library/action-beam-search.hpp"
#include "library/simple-beam-search.hpp"
#include "library/simulated-annealing.hpp"
#include "library/tree-beam-search.hpp"

using Clock = std::chrono::steady_clock;

struct LargeState {
  std::array<int, 64> values{};
  long long score = 0;
  int turn = 0;
};

struct TreeState {
  long long score = 0;
  std::uint64_t hash = 0;
};

struct TreeMove {
  int add;
  std::uint64_t hash_delta;
};

struct ForwardMove {
  int index;
  int add;
};

struct ForwardProblem {
  using State = LargeState;
  using Action = ForwardMove;
  using Score = long long;

  std::array<Action, 24> generate_actions(const State& parent) const {
    std::array<Action, 24> moves;
    for (int i = 0; i < 24; ++i) {
      moves[static_cast<std::size_t>(i)] = {
          parent.turn % 64,
          (i * 17 + parent.turn) % 31};
    }
    return moves;
  }

  Score evaluate_action(const State& parent, const Action& move) const {
    return parent.score + move.add;
  }

  void apply_action(State& child, Action& move) const {
    child.values[static_cast<std::size_t>(move.index)] += move.add;
    child.score += move.add;
    ++child.turn;
  }
};

int main() {
  constexpr int width = 800;
  constexpr int branch = 24;
  constexpr int simple_turns = 24;
  constexpr int tree_turns = 120;

  auto begin = Clock::now();
  SimpleBeamSearch<LargeState, long long> simple(LargeState{}, width);
  simple.reserve_candidates(static_cast<std::size_t>(width * branch));
  for (int turn = 0; turn < simple_turns; ++turn) {
    simple.step(
        [](const LargeState& parent) {
          std::array<LargeState, branch> children;
          for (int i = 0; i < branch; ++i) {
            children[static_cast<std::size_t>(i)] = parent;
            LargeState& child = children[static_cast<std::size_t>(i)];
            child.values[static_cast<std::size_t>(parent.turn % 64)] += i + 1;
            child.score += static_cast<long long>((i * 17 + parent.turn) % 31);
            ++child.turn;
          }
          return children;
        },
        [](const LargeState& state) { return state.score; });
  }
  const auto after_simple = Clock::now();

  SimpleBeamSearch<LargeState, long long> simple_each(LargeState{}, width);
  simple_each.reserve_candidates(static_cast<std::size_t>(width * branch));
  for (int turn = 0; turn < simple_turns; ++turn) {
    simple_each.step_each(
        [](const LargeState& parent, auto&& emit) {
          for (int i = 0; i < branch; ++i) {
            LargeState child = parent;
            child.values[static_cast<std::size_t>(parent.turn % 64)] += i + 1;
            child.score += static_cast<long long>((i * 17 + parent.turn) % 31);
            ++child.turn;
            emit(std::move(child));
          }
        },
        [](const LargeState& state) { return state.score; });
  }
  const auto after_simple_each = Clock::now();

  ForwardProblem forward_problem;
  ActionBeamRunner<ForwardProblem> action_beam(
      forward_problem, LargeState{}, 0, width);
  action_beam.run(simple_turns);
  const auto after_action_beam = Clock::now();

  TreeBeamSearch<TreeState, TreeMove, long long> tree(TreeState{}, 0, width);
  tree.reserve_candidates(static_cast<std::size_t>(width * branch));
  tree.reserve_nodes(static_cast<std::size_t>(1 + width * tree_turns));
  const auto expand = [](const TreeState& state) {
    std::array<TreeMove, branch> moves;
    for (int i = 0; i < branch; ++i) {
      moves[static_cast<std::size_t>(i)] = {
          (i * 17 + static_cast<int>(state.hash & 15U)) % 31,
          static_cast<std::uint64_t>(i + 1) * 0x9e3779b97f4a7c15ULL};
    }
    return moves;
  };
  const auto apply = [](TreeState& state, const TreeMove& move) {
    state.score += move.add;
    state.hash ^= move.hash_delta;
  };
  const auto revert = [](TreeState& state, const TreeMove& move) {
    state.hash ^= move.hash_delta;
    state.score -= move.add;
  };
  for (int turn = 0; turn < tree_turns; ++turn) {
    tree.step(expand, apply, revert,
              [](const TreeState& state) { return state.score; });
  }
  const auto after_tree = Clock::now();

  SimulatedAnnealing sa(1.0, 1.0, 123456789);
  std::uint64_t accepted = 0;
  constexpr int sa_trials = 20'000'000;
  // 採用確率が乱数の最小正値より小さい悪化手の高速棄却を測る。
  for (int i = 0; i < sa_trials; ++i) {
    accepted += static_cast<std::uint64_t>(sa.accept(-1000.0));
  }
  const auto after_sa = Clock::now();

  const auto milliseconds = [](Clock::time_point first, Clock::time_point last) {
    return std::chrono::duration<double, std::milli>(last - first).count();
  };
  std::cout << "simple_range_ms " << milliseconds(begin, after_simple) << '\n';
  std::cout << "simple_each_ms "
            << milliseconds(after_simple, after_simple_each) << '\n';
  std::cout << "action_beam_ms "
            << milliseconds(after_simple_each, after_action_beam) << '\n';
  std::cout << "tree_ms " << milliseconds(after_action_beam, after_tree) << '\n';
  std::cout << "sa_hopeless_ms "
            << milliseconds(after_tree, after_sa) << '\n';
  std::cout << "checksum " << simple.best().score << ' '
            << simple_each.best().score << ' ' << action_beam.best().score
            << ' ' << tree.best_score() << ' ' << accepted << '\n';
}
