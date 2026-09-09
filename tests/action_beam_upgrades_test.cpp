#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "library/action-beam-search.hpp"

namespace {

struct State {
  int value = 0;
  std::vector<int> path;

  static int copy_count;

  State() = default;
  State(const State& other) : value(other.value), path(other.path) {
    ++copy_count;
  }
  State(State&&) noexcept = default;
  State& operator=(const State&) = default;
  State& operator=(State&&) noexcept = default;
};

int State::copy_count = 0;

struct Move {
  int delta;
  int id;
};

void test_materializes_only_top_n() {
  ActionBeamSearch<State, Move, int> beam(State{}, 0, 7);
  State::copy_count = 0;
  int apply_count = 0;

  assert(beam.step(
      [](const State&) {
        std::vector<Move> actions;
        actions.reserve(100);
        for (int i = 0; i < 100; ++i) actions.push_back({i, i});
        return actions;
      },
      [](const State& parent, const Move& move) {
        return parent.value + move.delta;
      },
      [&](State& child, Move& move) {
        ++apply_count;
        child.value += move.delta;
        child.path.push_back(move.id);
      }));

  assert(beam.size() == 7);
  assert(beam.best().value == 99);
  assert(beam.best_score() == 99);
  assert(apply_count == 7);
  assert(State::copy_count == 7);
  assert(beam.last_generated_count() == 100);
  assert(beam.last_unique_count() == 100);
  assert(beam.last_kept_count() == 7);
  assert(beam.last_buffered_peak_count() <= 14);
  for (int rank = 0; rank < 7; ++rank) {
    assert(beam.states()[static_cast<std::size_t>(rank)].value == 99 - rank);
    assert(beam.scores()[static_cast<std::size_t>(rank)] == 99 - rank);
  }
}

void test_full_buffer_selection_has_same_result() {
  ActionBeamSearch<State, Move, int> beam(State{}, 0, 7);
  beam.set_batched_selection(false);
  assert(!beam.batched_selection());
  int apply_count = 0;
  assert(beam.step(
      [](const State&) {
        std::vector<Move> actions;
        for (int i = 0; i < 100; ++i) actions.push_back({i, i});
        return actions;
      },
      [](const State&, const Move& move) { return move.delta; },
      [&](State& state, Move& move) {
        ++apply_count;
        state.value = move.delta;
      }));
  assert(beam.best().value == 99);
  assert(beam.last_buffered_peak_count() == 100);
  assert(beam.last_kept_count() == 7);
  assert(apply_count == 7);
}

void test_observer_sees_candidates_before_cutoff() {
  ActionBeamSearch<State, Move, int> beam(State{}, 0, 3);
  int observed = 0;
  int saved_terminal = -1;
  int apply_count = 0;
  assert(beam.step_and_observe(
      [](const State&) {
        std::vector<Move> actions;
        for (int i = 0; i < 20; ++i) actions.push_back({i, i});
        return actions;
      },
      [](const State&, const Move& move) { return move.delta; },
      [&](State& state, Move& move) {
        ++apply_count;
        state.value = move.delta;
      },
      [&](std::size_t parent_rank,
          const State& parent,
          const Move& move,
          const int& score) {
        assert(parent_rank == 0);
        assert(parent.value == 0);
        assert(score == move.delta);
        ++observed;
        if (move.id == 4) saved_terminal = move.id;
      }));
  assert(observed == 20);
  assert(saved_terminal == 4);
  assert(apply_count == 3);
}

void test_stable_ties() {
  ActionBeamSearch<State, Move, int> beam(State{}, 0, 2);
  const auto evaluate_tie = [](const State&, const Move&) { return 0; };
  const auto apply = [](State& state, Move& move) {
    state.path.push_back(move.id);
  };

  assert(beam.step(
      [](const State&) { return std::array<Move, 2>{{{0, 1}, {0, 2}}}; },
      evaluate_tie, apply));
  assert(beam.step(
      [](const State&) {
        return std::array<Move, 2>{{{0, 10}, {0, 11}}};
      },
      evaluate_tie, apply));

  assert((beam.states()[0].path == std::vector<int>{1, 10}));
  assert((beam.states()[1].path == std::vector<int>{1, 11}));
}

void test_minimize_and_nan() {
  ActionBeamSearch<State, Move, double> beam(State{}, 0.0, 3, false);
  const double nan = std::numeric_limits<double>::quiet_NaN();
  assert(beam.step(
      [](const State&) {
        return std::array<Move, 5>{{
            {4, 0}, {1, 1}, {3, 2}, {2, 3}, {0, 4}}};
      },
      [nan](const State&, const Move& move) {
        return move.id == 4 ? nan : static_cast<double>(move.delta);
      },
      [](State& state, Move& move) { state.value = move.delta; }));
  assert(beam.states()[0].value == 1);
  assert(beam.states()[1].value == 2);
  assert(beam.states()[2].value == 3);
}

void test_keyed_selection() {
  ActionBeamSearch<State, Move, int> beam(State{}, 0, 10);
  int apply_count = 0;
  assert(beam.step_with_key(
      [](const State&) {
        return std::array<Move, 6>{{
            {0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}}};
      },
      [](const State&, const Move& move) { return move.delta; },
      [](const State&, const Move& move) { return move.id % 3; },
      [&](State& state, Move& move) {
        ++apply_count;
        state.value += move.delta;
        state.path.push_back(move.id);
      }));

  assert(beam.last_generated_count() == 6);
  assert(beam.last_unique_count() == 3);
  assert(beam.last_kept_count() == 3);
  assert(apply_count == 3);
  assert(beam.states()[0].value == 5);
  assert(beam.states()[1].value == 4);
  assert(beam.states()[2].value == 3);
}

struct PairKey {
  int first;
  int second;
};

struct PairHash {
  std::size_t operator()(const PairKey& key) const {
    return static_cast<std::size_t>(key.first * 31 + key.second);
  }
};

struct PairEqual {
  bool operator()(const PairKey& a, const PairKey& b) const {
    return a.first == b.first && a.second == b.second;
  }
};

void test_custom_key() {
  ActionBeamSearch<State, Move, int> beam(State{}, 0, 4);
  assert(beam.step_with_key(
      [](const State&) {
        return std::array<Move, 3>{{{1, 0}, {5, 2}, {3, 1}}};
      },
      [](const State&, const Move& move) { return move.delta; },
      [](const State&, const Move& move) {
        return PairKey{move.id % 2, 7};
      },
      PairHash{}, PairEqual{},
      [](State& state, Move& move) { state.value += move.delta; }));
  assert(beam.size() == 2);
  assert(beam.states()[0].value == 5);
  assert(beam.states()[1].value == 3);
}

void test_bucket_diversity() {
  ActionBeamSearch<State, Move, int> beam(State{}, 0, 4);
  assert(beam.step_with_bucket_limit(
      [](const State&) {
        return std::array<Move, 6>{{
            {100, 0}, {99, 2}, {98, 4}, {20, 1}, {19, 3}, {18, 5}}};
      },
      [](const State&, const Move& move) { return move.delta; },
      [](const State&, const Move& move) { return move.id % 2; },
      2,
      [](State& state, Move& move) {
        state.value = move.delta;
        state.path.push_back(move.id);
      }));

  assert(beam.last_generated_count() == 6);
  assert(beam.last_unique_count() == 4);
  assert(beam.last_kept_count() == 4);
  assert(beam.states()[0].value == 100);
  assert(beam.states()[1].value == 99);
  assert(beam.states()[2].value == 20);
  assert(beam.states()[3].value == 19);

  bool threw = false;
  try {
    beam.step_with_bucket_limit(
        [](const State&) { return std::array<Move, 0>{}; },
        [](const State&, const Move&) { return 0; },
        [](const State&, const Move&) { return 0; },
        0,
        [](State&, Move&) {});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

struct Reference {
  State state;
  int score;
  std::size_t order;
};

std::uint64_t next_random(std::uint64_t& value) {
  value ^= value << 7;
  value ^= value >> 9;
  return value;
}

void test_random_oracle() {
  constexpr int width = 5;
  constexpr int branch = 17;
  for (int seed = 1; seed <= 50; ++seed) {
    ActionBeamSearch<State, Move, int> actual(State{}, 0, width);
    std::vector<Reference> expected{{State{}, 0, 0}};
    std::uint64_t random = static_cast<std::uint64_t>(seed);

    for (int turn = 0; turn < 8; ++turn) {
      std::vector<std::vector<Move>> actions(expected.size());
      for (std::size_t parent = 0; parent < expected.size(); ++parent) {
        actions[parent].reserve(branch);
        for (int i = 0; i < branch; ++i) {
          const int delta = static_cast<int>(next_random(random) % 31) - 15;
          actions[parent].push_back({delta, i});
        }
      }

      std::vector<Reference> candidates;
      std::size_t order = 0;
      for (std::size_t parent = 0; parent < expected.size(); ++parent) {
        for (const Move& move : actions[parent]) {
          Reference child = expected[parent];
          child.state.value += move.delta;
          child.state.path.push_back(move.id);
          child.score = child.state.value;
          child.order = order++;
          candidates.push_back(std::move(child));
        }
      }
      std::sort(candidates.begin(), candidates.end(),
                [](const Reference& a, const Reference& b) {
                  if (a.score != b.score) return a.score > b.score;
                  return a.order < b.order;
                });
      if (candidates.size() > width) candidates.resize(width);

      std::size_t expanded_parent = 0;
      assert(actual.step(
          [&](const State&) {
            return actions[expanded_parent++];
          },
          [](const State& state, const Move& move) {
            return state.value + move.delta;
          },
          [](State& state, Move& move) {
            state.value += move.delta;
            state.path.push_back(move.id);
          }));

      expected = std::move(candidates);
      assert(actual.size() == expected.size());
      for (std::size_t rank = 0; rank < expected.size(); ++rank) {
        assert(actual.scores()[rank] == expected[rank].score);
        assert(actual.states()[rank].value == expected[rank].state.value);
        assert(actual.states()[rank].path == expected[rank].state.path);
      }
      assert(actual.last_buffered_peak_count() <= 2 * width);
    }
  }
}

void test_reset_width_and_empty_step() {
  ActionBeamSearch<State, Move, int> beam(State{}, 0, 3);
  assert(beam.step(
      [](const State&) {
        return std::array<Move, 3>{{{1, 1}, {2, 2}, {3, 3}}};
      },
      [](const State& state, const Move& move) {
        return state.value + move.delta;
      },
      [](State& state, Move& move) { state.value += move.delta; }));
  beam.set_width(1);
  assert(beam.size() == 1 && beam.best().value == 3);
  const int old_depth = beam.depth();
  assert(!beam.step(
      [](const State&) { return std::array<Move, 0>{}; },
      [](const State&, const Move&) { return 0; },
      [](State&, Move&) {}));
  assert(beam.depth() == old_depth);
  assert(beam.best().value == 3);
  assert(beam.last_generated_count() == 0);
  assert(beam.last_kept_count() == 0);

  bool threw = false;
  try {
    beam.set_width(0);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  beam.reset(State{}, 0);
  assert(beam.depth() == 0 && beam.size() == 1);
  assert(beam.best().value == 0 && beam.best_score() == 0);
  beam.release_memory();
  assert(beam.best().value == 0);
}

}  // namespace

int main() {
  test_materializes_only_top_n();
  test_full_buffer_selection_has_same_result();
  test_observer_sees_candidates_before_cutoff();
  test_stable_ties();
  test_minimize_and_nan();
  test_keyed_selection();
  test_custom_key();
  test_bucket_diversity();
  test_random_oracle();
  test_reset_width_and_empty_step();
}
