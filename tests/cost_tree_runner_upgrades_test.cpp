#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

#include "library/cost-tree-beam-search.hpp"

struct VariableCostProblem {
  struct State {
    int value = 0;
    int generation = 0;
  };

  struct Move {
    int add = 0;
    int advance = 1;
  };

  using Score = int;

  std::array<Move, 2> moves{{{1, 1}, {4, 2}}};

  const std::array<Move, 2>& generate_moves(const State&) const {
    return moves;
  }

  void apply_move(State& state, Move& move) const {
    state.value += move.add;
    state.generation += move.advance;
  }

  void revert_move(State& state, const Move& move) const {
    state.value -= move.add;
    state.generation -= move.advance;
  }

  Score evaluate(const State& state) const {
    return state.value;
  }

  int get_advance(const Move& move) const {
    return move.advance;
  }

  std::uint64_t make_key(const State& state) const {
    return static_cast<std::uint64_t>(state.value);
  }
};

void test_runner_basic_and_state_visit() {
  VariableCostProblem problem;
  CostTreeBeamRunner<VariableCostProblem> runner(
      problem, VariableCostProblem::State{}, 0, 3, 5);
  runner.reserve_nodes(64);
  runner.reserve_candidates(16);

  assert(runner.step());
  assert(runner.generation() == 1);
  assert(runner.beam_width() == 3);
  assert(runner.max_generation() == 5);

  std::vector<int> values;
  runner.for_each_state([&](int rank, const VariableCostProblem::State& state) {
    assert(rank >= 0);
    assert(state.generation == runner.generation());
    values.push_back(state.value);
  });
  assert(values == std::vector<int>({1}));

  assert(runner.run() == 5);
  assert(runner.best_score() == 9);
  const std::vector<VariableCostProblem::Move> answer = runner.restore();
  int generation = 0;
  int value = 0;
  for (const auto& move : answer) {
    generation += move.advance;
    value += move.add;
  }
  assert(generation == 5);
  assert(value == 9);
}

void test_runner_key_observer_and_resize() {
  VariableCostProblem problem;
  CostTreeBeamRunner<VariableCostProblem> runner(
      problem, VariableCostProblem::State{}, 0, 4, 4);
  int observed = 0;
  std::vector<VariableCostProblem::Move> terminal;
  while (runner.generation() < 4) {
    const bool advanced = runner.step_with_key_and_observe(
        [&](int parent_rank,
            const VariableCostProblem::Move& move,
            const VariableCostProblem::State& state,
            int score,
            int next_generation) {
          ++observed;
          assert(score == state.value);
          assert(next_generation == state.generation);
          if (next_generation == 4 && terminal.empty()) {
            runner.restore_candidate(parent_rank, move, terminal);
          }
        });
    if (!advanced) break;
    runner.set_width(2);
  }
  assert(observed > 0);
  assert(runner.generation() == 4);
  assert(runner.size() <= 2);
  assert(!terminal.empty());

  CostTreeBeamRunner<VariableCostProblem> keyed(
      problem, VariableCostProblem::State{}, 0, 3, 5);
  assert(keyed.run_with_key() == 5);
  assert(keyed.best_score() == 9);
}

int main() {
  test_runner_basic_and_state_visit();
  test_runner_key_observer_and_resize();
}
