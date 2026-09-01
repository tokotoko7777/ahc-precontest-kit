#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "library/cost-tree-beam-search.hpp"
#include "library/tree-beam-search.hpp"

namespace {

std::size_t counted_allocations = 0;
bool count_allocations = false;

}  // namespace

void* operator new(std::size_t size) {
  if (count_allocations) ++counted_allocations;
  if (size == 0) size = 1;
  if (void* pointer = std::malloc(size)) return pointer;
  throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
  if (count_allocations) ++counted_allocations;
  if (size == 0) size = 1;
  if (void* pointer = std::malloc(size)) return pointer;
  throw std::bad_alloc();
}

[[gnu::noinline]] void release_test_memory(void* pointer) noexcept {
  std::free(pointer);
}

void operator delete(void* pointer) noexcept { release_test_memory(pointer); }
void operator delete[](void* pointer) noexcept { release_test_memory(pointer); }
void operator delete(void* pointer, std::size_t) noexcept {
  release_test_memory(pointer);
}
void operator delete[](void* pointer, std::size_t) noexcept {
  release_test_memory(pointer);
}

namespace {

struct FixedMove {
  int delta;
  int id;

  bool operator==(const FixedMove& other) const {
    return delta == other.delta && id == other.id;
  }
};

struct FixedState {
  int value = 0;
  int depth = 0;
  long long code = 0;
};

void apply_fixed(FixedState& state, const FixedMove& move) {
  state.value += move.delta;
  ++state.depth;
  state.code = state.code * 4 + move.id + 1;
}

void revert_fixed(FixedState& state, const FixedMove& move) {
  state.code = (state.code - move.id - 1) / 4;
  --state.depth;
  state.value -= move.delta;
}

void test_fixed_observer(bool keyed) {
  TreeBeamSearch<FixedState, FixedMove, int> beam(FixedState{}, 0, 1);
  const std::array<FixedMove, 2> actions{{{1, 0}, {2, 1}}};
  std::array<int, 2> observed{{0, 0}};
  std::vector<FixedMove> terminal_path;
  terminal_path.reserve(8);

  const auto expand = [&](const FixedState&)
      -> const std::array<FixedMove, 2>& { return actions; };
  const auto evaluate = [](const FixedState& state) { return state.value; };
  const auto observer = [&](int parent_rank,
                            const FixedMove& move,
                            const FixedState& child,
                            const int& score) {
    assert(parent_rank == 0);
    assert(child.depth == 1);
    assert(child.value == move.delta);
    assert(score == child.value);
    ++observed[move.id];
    if (move.id == 0) {
      beam.restore_candidate(parent_rank, move, terminal_path);
    }
  };

  bool advanced;
  if (keyed) {
    advanced = beam.step_with_key_and_observe(
        expand, apply_fixed, revert_fixed, evaluate,
        [](const FixedState&) { return 0; }, observer);
  } else {
    advanced = beam.step_and_observe(
        expand, apply_fixed, revert_fixed, evaluate, observer);
  }
  assert(advanced);
  assert(observed[0] == 1 && observed[1] == 1);
  assert(terminal_path == std::vector<FixedMove>({{1, 0}}));
  assert(beam.restore() == std::vector<FixedMove>({{2, 1}}));
  assert(beam.size() == 1);
  assert(beam.state.depth == 0 && beam.state.value == 0);

  std::vector<FixedMove> reused;
  reused.reserve(20);
  const std::size_t old_capacity = reused.capacity();
  beam.restore(0, reused);
  assert(reused == beam.restore());
  assert(reused.capacity() == old_capacity);
}

void test_tree_buffer_reuse() {
  struct State {
    int value = 0;
  };
  const std::array<int, 2> actions{{1, 2}};
  const auto expand = [&](const State&) -> const std::array<int, 2>& {
    return actions;
  };
  const auto apply = [](State& state, int move) { state.value += move; };
  const auto revert = [](State& state, int move) { state.value -= move; };
  const auto evaluate = [](const State& state) { return state.value; };

  TreeBeamSearch<State, int, int> beam(State{}, 0, 4);
  beam.reserve_nodes(300);
  beam.reserve_candidates(8);
  assert(beam.step(expand, apply, revert, evaluate));
  assert(beam.step(expand, apply, revert, evaluate));

  counted_allocations = 0;
  count_allocations = true;
  for (int turn = 0; turn < 50; ++turn) {
    assert(beam.step(expand, apply, revert, evaluate));
  }
  count_allocations = false;
  assert(counted_allocations == 0);
}

void test_move_only_callbacks() {
  const std::array<FixedMove, 1> actions{{{3, 0}}};
  auto expand = [guard = std::make_unique<int>(1), &actions](
                    const FixedState&) -> const std::array<FixedMove, 1>& {
    assert(*guard == 1);
    return actions;
  };
  auto apply = [guard = std::make_unique<int>(2)](
                   FixedState& state, const FixedMove& move) {
    assert(*guard == 2);
    apply_fixed(state, move);
  };
  auto revert = [guard = std::make_unique<int>(3)](
                    FixedState& state, const FixedMove& move) {
    assert(*guard == 3);
    revert_fixed(state, move);
  };
  auto evaluate = [guard = std::make_unique<int>(4)](
                      const FixedState& state) {
    assert(*guard == 4);
    return state.value;
  };
  auto observer = [guard = std::make_unique<int>(5)](
                      int, const FixedMove&, const FixedState&, const int&) {
    assert(*guard == 5);
  };

  TreeBeamSearch<FixedState, FixedMove, int> beam(FixedState{}, 0, 1);
  assert(beam.step_and_observe(
      expand, apply, revert, evaluate, observer));
  assert(beam.best_score() == 3);
}

struct UndoWritingMove {
  int next_value;
  int old_value = -1;

  bool operator==(const UndoWritingMove& other) const {
    return next_value == other.next_value && old_value == other.old_value;
  }
};

struct UndoWritingState {
  int value = 7;
};

void test_tree_action_written_by_apply() {
  TreeBeamSearch<UndoWritingState, UndoWritingMove, int> beam(
      UndoWritingState{}, 7, 1);
  const std::array<UndoWritingMove, 1> actions{{{23, -1}}};
  bool reverted_with_updated_action = false;
  std::vector<UndoWritingMove> observed_path;

  assert(beam.step_and_observe(
      [&](const UndoWritingState& state)
          -> const std::array<UndoWritingMove, 1>& {
        assert(state.value == 7);
        return actions;
      },
      [](UndoWritingState& state, UndoWritingMove& move) {
        move.old_value = state.value;
        state.value = move.next_value;
      },
      [&](UndoWritingState& state, const UndoWritingMove& move) {
        assert(move.old_value == 7);
        state.value = move.old_value;
        reverted_with_updated_action = true;
      },
      [](const UndoWritingState& state) { return state.value; },
      [&](int parent_rank,
          const UndoWritingMove& move,
          const UndoWritingState& child,
          const int& score) {
        assert(move.old_value == 7);
        assert(child.value == 23);
        assert(score == 23);
        beam.restore_candidate(parent_rank, move, observed_path);
      }));

  assert(reverted_with_updated_action);
  assert(beam.state.value == 7);
  assert(observed_path == std::vector<UndoWritingMove>({{23, 7}}));
  assert(beam.restore() == observed_path);
}

struct CostMove {
  int delta;
  int advance;
  int id;

  bool operator==(const CostMove& other) const {
    return delta == other.delta && advance == other.advance && id == other.id;
  }
};

struct CostState {
  int value = 0;
  int generation = 0;
  long long code = 0;
};

struct NonDefaultScore {
  int value;

  explicit NonDefaultScore(int initial_value) : value(initial_value) {}
  NonDefaultScore(const NonDefaultScore&) = delete;
  NonDefaultScore& operator=(const NonDefaultScore&) = delete;
  NonDefaultScore(NonDefaultScore&&) noexcept = default;
  NonDefaultScore& operator=(NonDefaultScore&&) = delete;
};

bool operator<(const NonDefaultScore& left,
               const NonDefaultScore& right) {
  return left.value < right.value;
}

void apply_cost(CostState& state, const CostMove& move) {
  state.value += move.delta;
  state.generation += move.advance;
  state.code = state.code * 4 + move.id + 1;
}

void revert_cost(CostState& state, const CostMove& move) {
  state.code = (state.code - move.id - 1) / 4;
  state.generation -= move.advance;
  state.value -= move.delta;
}

void test_cost_observer(bool keyed) {
  CostTreeBeamSearch<CostState, CostMove, int, int> beam(
      CostState{}, 0, 1, 1);
  const std::array<CostMove, 2> actions{{{1, 1, 0}, {2, 1, 1}}};
  std::array<int, 2> observed{{0, 0}};
  std::vector<CostMove> terminal_path;
  terminal_path.reserve(8);

  const auto expand = [&](const CostState&)
      -> const std::array<CostMove, 2>& { return actions; };
  const auto evaluate = [](const CostState& state) { return state.value; };
  const auto advance = [](const CostMove& move) { return move.advance; };
  const auto observer = [&](int parent_rank,
                            const CostMove& move,
                            const CostState& child,
                            const int& score,
                            int next_generation) {
    assert(parent_rank == 0);
    assert(next_generation == 1);
    assert(child.generation == next_generation);
    assert(child.value == move.delta);
    assert(score == child.value);
    ++observed[move.id];
    if (move.id == 0) {
      beam.restore_candidate(parent_rank, move, terminal_path);
    }
  };

  bool advanced;
  if (keyed) {
    advanced = beam.step_with_key_and_observe(
        expand, apply_cost, revert_cost, evaluate, advance,
        [](const CostState&) { return 0; }, observer);
  } else {
    advanced = beam.step_and_observe(
        expand, apply_cost, revert_cost, evaluate, advance, observer);
  }
  assert(advanced);
  assert(observed[0] == 1 && observed[1] == 1);
  assert(terminal_path == std::vector<CostMove>({{1, 1, 0}}));
  assert(beam.restore() == std::vector<CostMove>({{2, 1, 1}}));
  assert(beam.size() == 1 && beam.best_score() == 2);

  std::vector<CostMove> reused;
  reused.reserve(20);
  const std::size_t old_capacity = reused.capacity();
  beam.restore(0, reused);
  assert(reused == beam.restore());
  assert(reused.capacity() == old_capacity);
}

void test_cost_dynamic_width() {
  const auto evaluate = [](const CostState& state) { return state.value; };
  const auto advance = [](const CostMove& move) { return move.advance; };
  const auto expand = [](const CostState& state) {
    if (state.generation != 0) return std::vector<CostMove>{};
    return std::vector<CostMove>{
        {30, 3, 0}, {20, 3, 1}, {10, 3, 2}, {1, 1, 0}};
  };

  CostTreeBeamSearch<CostState, CostMove, int> beam(
      CostState{}, 0, 3, 3);
  assert(beam.beam_width() == 3);
  assert(beam.max_generation() == 3);
  assert(beam.step(expand, apply_cost, revert_cost, evaluate, advance));
  assert(beam.generation() == 1 && beam.size() == 1);

  // generation 3には既に3候補が予約済み。縮小は未来層にも即反映する。
  beam.set_beam_width(1);
  assert(beam.beam_width() == 1);
  beam.set_beam_width(5);
  assert(beam.beam_width() == 5);
  assert(beam.step(expand, apply_cost, revert_cost, evaluate, advance));
  assert(beam.generation() == 3);
  assert(beam.size() == 1);
  assert(beam.best_score() == 30);
  assert(beam.restore() == std::vector<CostMove>({{30, 3, 0}}));

  CostTreeBeamSearch<CostState, CostMove, int> current(
      CostState{}, 0, 3, 1);
  const auto three_now = [](const CostState&) {
    return std::vector<CostMove>{{1, 1, 0}, {3, 1, 1}, {2, 1, 2}};
  };
  assert(current.step(
      three_now, apply_cost, revert_cost, evaluate, advance));
  assert(current.size() == 3);
  current.set_beam_width(1);
  assert(current.size() == 1 && current.best_score() == 3);

  bool threw = false;
  try {
    current.set_beam_width(0);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void test_cost_width_with_non_default_score() {
  CostTreeBeamSearch<CostState, CostMove, NonDefaultScore> beam(
      CostState{}, NonDefaultScore(0), 3, 1);
  assert(beam.step(
      [](const CostState&) {
        return std::vector<CostMove>{
            {1, 1, 0}, {3, 1, 1}, {2, 1, 2}};
      },
      apply_cost, revert_cost,
      [](const CostState& state) {
        return NonDefaultScore(state.value);
      },
      [](const CostMove& move) { return move.advance; }));
  assert(beam.size() == 3);
  beam.set_beam_width(1);
  assert(beam.size() == 1);
  assert(beam.best_score().value == 3);
}

void test_cost_move_only_callbacks() {
  const std::array<CostMove, 1> actions{{{4, 1, 0}}};
  auto expand = [guard = std::make_unique<int>(1), &actions](
                    const CostState&) -> const std::array<CostMove, 1>& {
    assert(*guard == 1);
    return actions;
  };
  auto apply = [guard = std::make_unique<int>(2)](
                   CostState& state, const CostMove& move) {
    assert(*guard == 2);
    apply_cost(state, move);
  };
  auto revert = [guard = std::make_unique<int>(3)](
                    CostState& state, const CostMove& move) {
    assert(*guard == 3);
    revert_cost(state, move);
  };
  auto evaluate = [guard = std::make_unique<int>(4)](
                      const CostState& state) {
    assert(*guard == 4);
    return state.value;
  };
  auto advance = [guard = std::make_unique<int>(5)](const CostMove& move) {
    assert(*guard == 5);
    return move.advance;
  };
  auto observer = [guard = std::make_unique<int>(6)](
                      int,
                      const CostMove&,
                      const CostState&,
                      const int&,
                      int) { assert(*guard == 6); };

  CostTreeBeamSearch<CostState, CostMove, int> beam(
      CostState{}, 0, 1, 1);
  assert(beam.step_and_observe(
      expand, apply, revert, evaluate, advance, observer));
  assert(beam.best_score() == 4);
}

void test_cost_action_written_by_apply_and_generation_limit() {
  struct Move {
    int next_value;
    int advance;
    int old_value = -1;

    bool operator==(const Move& other) const {
      return next_value == other.next_value &&
             advance == other.advance && old_value == other.old_value;
    }
  };
  struct State {
    int value = 11;
  };

  CostTreeBeamSearch<State, Move, int> beam(State{}, 11, 1, 1);
  const std::array<Move, 2> actions{{{29, 1, -1}, {99, 2, -1}}};
  int apply_count = 0;
  int revert_count = 0;
  int observer_count = 0;
  std::vector<Move> observed_path;

  assert(beam.step_and_observe(
      [&](const State& state) -> const std::array<Move, 2>& {
        assert(state.value == 11);
        return actions;
      },
      [&](State& state, Move& move) {
        ++apply_count;
        move.old_value = state.value;
        state.value = move.next_value;
      },
      [&](State& state, const Move& move) {
        ++revert_count;
        assert(move.old_value == 11);
        state.value = move.old_value;
      },
      [](const State& state) { return state.value; },
      [](const Move& move) { return move.advance; },
      [&](int parent_rank,
          const Move& move,
          const State& child,
          const int& score,
          int next_generation) {
        ++observer_count;
        assert(next_generation == 1);
        assert(move.next_value == 29);
        assert(move.old_value == 11);
        assert(child.value == 29);
        assert(score == 29);
        beam.restore_candidate(parent_rank, move, observed_path);
      }));

  // advance=2はmax_generationを超えるため、applyもobserverも呼ばれない。
  assert(apply_count == 1);
  assert(revert_count == 1);
  assert(observer_count == 1);
  assert(observed_path == std::vector<Move>({{29, 1, 11}}));
  assert(beam.restore() == observed_path);
}

struct FixedReference {
  FixedState state;
  std::vector<FixedMove> path;
  int score;
  int parent_rank;
  int action_index;
};

int positive_mod(long long value, int modulus) {
  int result = static_cast<int>(value % modulus);
  if (result < 0) result += modulus;
  return result;
}

std::vector<FixedMove> make_fixed_moves(
    const FixedState& state, int seed) {
  if (state.depth >= 8) return {};
  std::vector<FixedMove> actions;
  actions.reserve(3);
  for (int id = 0; id < 3; ++id) {
    const int delta = positive_mod(
                          state.value * 7LL + state.depth * 11LL +
                              seed * 13LL + id * 5LL,
                          7) -
                      3;
    actions.push_back({delta, id});
  }
  return actions;
}

int fixed_score(const FixedState& state, int seed) {
  const int target = positive_mod(seed * 5LL, 9) - 4;
  return -std::abs(state.value - target) * 20 +
         positive_mod(state.code, 7);
}

void test_fixed_random_oracle(bool keyed) {
  for (int seed = 0; seed < 40; ++seed) {
    int width = 1 + seed % 4;
    TreeBeamSearch<FixedState, FixedMove, int> beam(
        FixedState{}, fixed_score(FixedState{}, seed), width);
    std::vector<FixedReference> active{{
        FixedState{}, {}, fixed_score(FixedState{}, seed), 0, 0}};

    for (int turn = 0; turn <= 8; ++turn) {
      const int new_width = 1 + positive_mod(seed * 3LL + turn * 5LL, 5);
      if (new_width < width &&
          active.size() > static_cast<std::size_t>(new_width)) {
        active.resize(static_cast<std::size_t>(new_width));
      }
      width = new_width;
      beam.set_beam_width(width);

      std::vector<FixedReference> candidates;
      int expected_observed = 0;
      for (int parent_rank = 0;
           parent_rank < static_cast<int>(active.size()); ++parent_rank) {
        const std::vector<FixedMove> actions =
            make_fixed_moves(active[parent_rank].state, seed);
        expected_observed += static_cast<int>(actions.size());
        for (int action_index = 0;
             action_index < static_cast<int>(actions.size()); ++action_index) {
          FixedReference child = active[parent_rank];
          apply_fixed(child.state, actions[action_index]);
          child.path.push_back(actions[action_index]);
          child.score = fixed_score(child.state, seed);
          child.parent_rank = parent_rank;
          child.action_index = action_index;
          candidates.push_back(std::move(child));
        }
      }

      const auto better = [](const FixedReference& a,
                             const FixedReference& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.parent_rank != b.parent_rank) {
          return a.parent_rank < b.parent_rank;
        }
        return a.action_index < b.action_index;
      };
      if (keyed) {
        std::vector<FixedReference> unique;
        std::unordered_map<int, int> index;
        for (FixedReference& candidate : candidates) {
          const int key = positive_mod(candidate.state.value, 5);
          const auto found = index.find(key);
          if (found == index.end()) {
            index.emplace(key, static_cast<int>(unique.size()));
            unique.push_back(std::move(candidate));
          } else if (better(candidate, unique[found->second])) {
            unique[found->second] = std::move(candidate);
          }
        }
        candidates = std::move(unique);
      }
      std::sort(candidates.begin(), candidates.end(), better);
      if (candidates.size() > static_cast<std::size_t>(width)) {
        candidates.resize(static_cast<std::size_t>(width));
      }

      int observed = 0;
      const auto expand = [&](const FixedState& state) {
        return make_fixed_moves(state, seed);
      };
      const auto evaluate = [&](const FixedState& state) {
        return fixed_score(state, seed);
      };
      const auto observer = [&](int,
                                const FixedMove&,
                                const FixedState&,
                                const int&) { ++observed; };
      bool advanced;
      if (keyed) {
        advanced = beam.step_with_key_and_observe(
            expand, apply_fixed, revert_fixed, evaluate,
            [](const FixedState& state) {
              return positive_mod(state.value, 5);
            },
            observer);
      } else {
        advanced = beam.step_and_observe(
            expand, apply_fixed, revert_fixed, evaluate, observer);
      }
      assert(observed == expected_observed);
      assert(advanced == !candidates.empty());
      if (!advanced) break;

      active = std::move(candidates);
      assert(beam.size() == static_cast<int>(active.size()));
      for (int rank = 0; rank < beam.size(); ++rank) {
        assert(beam.restore(rank) == active[rank].path);
        assert(beam.nodes[beam.beam[rank]].score == active[rank].score);
      }
    }
  }
}

struct LogicalOrder {
  int step;
  int parent_rank;
  int action_index;
};

struct CostReference {
  CostState state;
  std::vector<CostMove> path;
  int score;
  LogicalOrder order;
};

std::vector<CostMove> make_cost_moves(const CostState& state, int seed) {
  std::vector<CostMove> actions;
  actions.reserve(3);
  for (int id = 0; id < 3; ++id) {
    const int advance = 1 + positive_mod(
                                state.value * 3LL + state.generation * 5LL +
                                    seed * 7LL + id * 11LL,
                                3);
    const int delta = positive_mod(
                          state.value * 13LL + state.generation * 17LL +
                              seed * 19LL + id * 7LL,
                          9) -
                      4;
    actions.push_back({delta, advance, id});
  }
  return actions;
}

bool cost_better(const CostReference& a, const CostReference& b) {
  if (a.score != b.score) return a.score > b.score;
  if (a.order.step != b.order.step) return a.order.step < b.order.step;
  if (a.order.parent_rank != b.order.parent_rank) {
    return a.order.parent_rank < b.order.parent_rank;
  }
  return a.order.action_index < b.order.action_index;
}

void trim_reference(std::vector<CostReference>& entries, int width) {
  if (entries.size() > static_cast<std::size_t>(width)) {
    entries.resize(static_cast<std::size_t>(width));
  }
}

void test_cost_random_oracle(bool keyed) {
  constexpr int max_generation = 12;
  for (int seed = 0; seed < 40; ++seed) {
    int width = 1 + seed % 4;
    CostTreeBeamSearch<CostState, CostMove, int, int> beam(
        CostState{}, 0, width, max_generation);
    std::vector<CostReference> active{{CostState{}, {}, 0, {0, 0, 0}}};
    std::vector<std::vector<CostReference>> future(max_generation + 1);
    int generation = 0;
    int step_order = 1;

    while (true) {
      const int new_width =
          1 + positive_mod(seed * 5LL + step_order * 7LL, 5);
      width = new_width;
      beam.set_beam_width(width);
      trim_reference(active, width);
      for (std::vector<CostReference>& entries : future) {
        trim_reference(entries, width);
      }

      std::vector<std::vector<CostReference>> added(max_generation + 1);
      int expected_observed = 0;
      for (int parent_rank = 0;
           parent_rank < static_cast<int>(active.size()); ++parent_rank) {
        const std::vector<CostMove> actions =
            make_cost_moves(active[parent_rank].state, seed);
        for (int action_index = 0;
             action_index < static_cast<int>(actions.size()); ++action_index) {
          const CostMove& move = actions[action_index];
          if (generation + move.advance > max_generation) continue;
          ++expected_observed;
          CostReference child = active[parent_rank];
          apply_cost(child.state, move);
          child.path.push_back(move);
          child.score = child.state.value;
          child.order = {step_order, parent_rank, action_index};
          added[child.state.generation].push_back(std::move(child));
        }
      }

      for (int next_generation = generation + 1;
           next_generation <= max_generation; ++next_generation) {
        if (added[next_generation].empty()) continue;
        std::vector<CostReference> combined =
            std::move(future[next_generation]);
        for (CostReference& candidate : added[next_generation]) {
          combined.push_back(std::move(candidate));
        }

        if (keyed) {
          std::vector<CostReference> unique;
          std::unordered_map<int, int> index;
          for (CostReference& candidate : combined) {
            const int key = positive_mod(candidate.state.value, 5);
            const auto found = index.find(key);
            if (found == index.end()) {
              index.emplace(key, static_cast<int>(unique.size()));
              unique.push_back(std::move(candidate));
            } else if (cost_better(candidate, unique[found->second])) {
              unique[found->second] = std::move(candidate);
            }
          }
          combined = std::move(unique);
        }
        std::sort(combined.begin(), combined.end(), cost_better);
        trim_reference(combined, width);
        future[next_generation] = std::move(combined);
      }

      int observed = 0;
      const auto expand = [&](const CostState& state) {
        return make_cost_moves(state, seed);
      };
      const auto evaluate = [](const CostState& state) { return state.value; };
      const auto advance = [](const CostMove& move) { return move.advance; };
      const auto observer = [&](int,
                                const CostMove&,
                                const CostState& child,
                                const int&,
                                int next_generation) {
        assert(child.generation == next_generation);
        ++observed;
      };
      bool advanced;
      if (keyed) {
        advanced = beam.step_with_key_and_observe(
            expand, apply_cost, revert_cost, evaluate, advance,
            [](const CostState& state) {
              return positive_mod(state.value, 5);
            },
            observer);
      } else {
        advanced = beam.step_and_observe(
            expand, apply_cost, revert_cost, evaluate, advance, observer);
      }
      assert(observed == expected_observed);

      int next_generation = generation + 1;
      while (next_generation <= max_generation &&
             future[next_generation].empty()) {
        ++next_generation;
      }
      const bool expected_advanced = next_generation <= max_generation;
      assert(advanced == expected_advanced);
      ++step_order;
      if (!advanced) break;

      generation = next_generation;
      active = std::move(future[generation]);
      future[generation].clear();
      assert(beam.generation() == generation);
      assert(beam.size() == static_cast<int>(active.size()));
      for (int rank = 0; rank < beam.size(); ++rank) {
        assert(beam.restore(rank) == active[rank].path);
      }
    }
  }
}

}  // namespace

int main() {
  test_fixed_observer(false);
  test_fixed_observer(true);
  test_tree_buffer_reuse();
  test_move_only_callbacks();
  test_tree_action_written_by_apply();
  test_cost_observer(false);
  test_cost_observer(true);
  test_cost_dynamic_width();
  test_cost_width_with_non_default_score();
  test_cost_move_only_callbacks();
  test_cost_action_written_by_apply_and_generation_limit();
  test_fixed_random_oracle(false);
  test_fixed_random_oracle(true);
  test_cost_random_oracle(false);
  test_cost_random_oracle(true);
}
