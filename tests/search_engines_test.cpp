#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "library/cost-tree-beam-search.hpp"
#include "library/simple-beam-search.hpp"
#include "library/simulated-annealing.hpp"
#include "library/time-based-simulated-annealing.hpp"
#include "library/tree-beam-search.hpp"

template <class Function>
void expect_invalid_argument(Function function) {
  bool thrown = false;
  try {
    function();
  } catch (const std::invalid_argument&) {
    thrown = true;
  }
  assert(thrown);
}

void test_simulated_annealing() {
  SimulatedAnnealing sa(100.0, 1.0, 123);
  sa.set_progress(0.5);
  assert(std::abs(sa.current_temperature() - 10.0) < 1e-12);
  assert(sa.accept(0));
  assert(sa.accept(1));
  assert(!sa.accept(std::numeric_limits<double>::quiet_NaN()));

  SimulatedAnnealing extreme(1e308, 1e-308, 0);
  assert(std::abs(extreme.temperature(0.5) - 1.0) < 1e-12);
  extreme.set_temperatures(16.0, 1.0);
  extreme.set_progress(0.5);
  assert(std::abs(extreme.current_temperature() - 4.0) < 1e-12);

  // 改善手で乱数を消費しないことを、同じseedの2個で確認する。
  SimulatedAnnealing first(10.0, 0.1, 456);
  SimulatedAnnealing second(10.0, 0.1, 456);
  first.set_progress(0.25);
  second.set_progress(0.25);
  for (int i = 0; i < 100; ++i) {
    assert(first.accept(1.0));
    assert(first.accept(-2.0) == second.accept(-2.0));
  }

  // 互換APIと、進捗を先に設定する高速APIは同じ判定列になる。
  SimulatedAnnealing direct(20.0, 0.2, 789);
  SimulatedAnnealing cached(20.0, 0.2, 789);
  for (int i = 0; i < 100; ++i) {
    const double progress = i / 99.0;
    cached.set_progress(progress);
    assert(direct.accept(-1.5, progress) == cached.accept(-1.5));
  }

  expect_invalid_argument([] { SimulatedAnnealing invalid(0.0, 1.0); });
  expect_invalid_argument([] {
    SimulatedAnnealing invalid(1.0, 0.0);
  });
  expect_invalid_argument([&] {
    sa.set_progress(std::numeric_limits<double>::quiet_NaN());
  });

  TimeBasedSimulatedAnnealing timed(100000.0, 100.0, 1.0, 1, 8);
  assert(!timed.is_over());
  assert(0.0 <= timed.cached_progress() && timed.cached_progress() <= 1.0);
  assert(timed.accept(0));
  assert(!timed.accept(std::numeric_limits<double>::quiet_NaN()));
  timed.reset();
  assert(timed.cached_progress() == 0.0);
  assert(timed.cached_temperature() == 100.0);
  assert(1.0 <= timed.current_temperature() &&
         timed.current_temperature() <= 100.0);
  TimeBasedSimulatedAnnealing safe_default(100000.0, 10.0, 1.0, 2);
  assert(safe_default.check_interval == 1);
  expect_invalid_argument([] {
    TimeBasedSimulatedAnnealing invalid(100.0, 10.0, 1.0, 0, 0);
  });
}

struct CopyState {
  int value = 0;
  double score = 0.0;
  int key = 0;
  std::vector<int> answer;
};

void test_simple_beam_search() {
  const auto expand = [](const CopyState& state) {
    std::vector<CopyState> next;
    for (int move : {-1, 1}) {
      CopyState child = state;
      child.value += move;
      child.score = child.value;
      child.answer.push_back(move);
      next.push_back(std::move(child));
    }
    return next;
  };
  const auto evaluate = [](const CopyState& state) { return state.score; };

  CopyState answer = simple_beam_search(CopyState{}, 3, 2, expand, evaluate);
  assert(answer.value == 3);
  assert(answer.answer == std::vector<int>({1, 1, 1}));

  SimpleBeamSearch<CopyState, double> minimum(CopyState{}, 2, false);
  assert(minimum.step(
      [](const CopyState&) {
        return std::vector<CopyState>{{1, 7.0, 0, {}},
                                      {2, 2.0, 0, {}},
                                      {3, 5.0, 0, {}}};
      }, evaluate));
  assert(minimum.best().score == 2.0);
  assert(minimum.states().size() == 2);

  SimpleBeamSearch<CopyState, double> keyed(CopyState{}, 10);
  assert(keyed.step_with_key(
      [](const CopyState&) {
        return std::vector<CopyState>{{1, 1.0, 5, {}},
                                      {2, 9.0, 5, {}},
                                      {3, 4.0, 8, {}}};
      }, evaluate,
      [](const CopyState& state) { return state.key; }));
  assert(keyed.states().size() == 2);
  assert(keyed.best().value == 2);

  SimpleBeamSearch<CopyState, double> nan_beam(CopyState{}, 2);
  const double nan = std::numeric_limits<double>::quiet_NaN();
  assert(nan_beam.step(
      [nan](const CopyState&) {
        return std::vector<CopyState>{{1, nan, 0, {}},
                                      {2, 3.0, 0, {}},
                                      {3, 1.0, 0, {}}};
      }, evaluate));
  assert(nan_beam.states()[0].score == 3.0);
  assert(nan_beam.states()[1].score == 1.0);
  const int old_depth = nan_beam.depth();
  assert(!nan_beam.step(
      [](const CopyState&) { return std::vector<CopyState>{}; }, evaluate));
  assert(nan_beam.depth() == old_depth);

  struct MoveOnlyState {
    std::unique_ptr<int> value;
  };
  SimpleBeamSearch<MoveOnlyState, int> move_only(
      MoveOnlyState{std::make_unique<int>(0)}, 1);
  assert(move_only.step(
      [](const MoveOnlyState& state) {
        std::vector<MoveOnlyState> children;
        children.push_back(
            MoveOnlyState{std::make_unique<int>(*state.value + 1)});
        return children;
      },
      [](const MoveOnlyState& state) { return *state.value; }));
  assert(*move_only.best().value == 1);

  std::vector<CopyState> reusable_children;
  reusable_children.reserve(4);
  SimpleBeamSearch<CopyState, double> reused(CopyState{}, 2);
  assert(reused.step(
      [&](const CopyState& state) -> std::vector<CopyState>& {
        reusable_children.clear();
        reusable_children.push_back(
            CopyState{state.value + 1, 1.0, 0, {}});
        reusable_children.push_back(
            CopyState{state.value + 2, 2.0, 0, {}});
        return reusable_children;
      },
      evaluate));
  assert(reused.best().value == 2);

  const std::vector<CopyState> constant_children{
      {7, 7.0, 0, {}}, {3, 3.0, 0, {}}};
  SimpleBeamSearch<CopyState, double> copied_const(CopyState{}, 1);
  assert(copied_const.step(
      [&](const CopyState&) -> const std::vector<CopyState>& {
        return constant_children;
      },
      evaluate));
  assert(copied_const.best().value == 7);
  assert(constant_children[0].value == 7);

  expect_invalid_argument([] { SimpleBeamSearch<CopyState, int> bad({}, 0); });
  expect_invalid_argument([&] {
    static_cast<void>(simple_beam_search(CopyState{}, -1, 1, expand, evaluate));
  });
}

struct UndoMove {
  int difference;
  int digit;

  bool operator==(const UndoMove& other) const {
    return difference == other.difference && digit == other.digit;
  }
};

struct UndoState {
  long long value = 0;
  long long code = 0;
  int depth = 0;
};

void apply_undo_move(UndoState& state, const UndoMove& move) {
  state.value += move.difference;
  state.code = state.code * 5 + move.digit;
  ++state.depth;
}

void revert_undo_move(UndoState& state, const UndoMove& move) {
  --state.depth;
  state.code = (state.code - move.digit) / 5;
  state.value -= move.difference;
}

long long undo_score(const UndoState& state) {
  return -std::llabs(state.value - 7) * 1000000LL + state.code;
}

void test_tree_beam_search() {
  constexpr int width = 7;
  TreeBeamSearch<UndoState, UndoMove, long long> beam(
      UndoState{}, undo_score(UndoState{}), width);
  beam.reserve_nodes(1000);
  beam.reserve_candidates(100);

  struct Reference {
    UndoState state;
    std::vector<UndoMove> moves;
    long long score;
  };
  std::vector<Reference> reference{{UndoState{}, {}, undo_score({})}};
  const std::vector<UndoMove> actions{{-1, 1}, {1, 2}, {3, 3}};

  for (int turn = 0; turn < 7; ++turn) {
    std::vector<Reference> candidates;
    for (const Reference& parent : reference) {
      for (const UndoMove& move : actions) {
        Reference child = parent;
        apply_undo_move(child.state, move);
        child.moves.push_back(move);
        child.score = undo_score(child.state);
        candidates.push_back(std::move(child));
      }
    }
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const Reference& a, const Reference& b) {
                       return a.score > b.score;
                     });
    if (candidates.size() > width) candidates.resize(width);
    reference = std::move(candidates);

    assert(beam.step(
        [&](const UndoState&) -> const std::vector<UndoMove>& {
          return actions;
        }, apply_undo_move,
        revert_undo_move, undo_score));
    assert(beam.state.value == 0 && beam.state.code == 0 &&
           beam.state.depth == 0);
    assert(beam.beam.size() == reference.size());
    for (int rank = 0; rank < static_cast<int>(reference.size()); ++rank) {
      assert(beam.nodes[beam.beam[rank]].score == reference[rank].score);
    }
    std::vector<long long> visited(reference.size());
    beam.for_each_state(
        [&](int rank, const UndoState& state) {
          visited[rank] = undo_score(state);
        },
        apply_undo_move,
        revert_undo_move);
    for (int rank = 0; rank < static_cast<int>(reference.size()); ++rank) {
      assert(visited[rank] == reference[rank].score);
    }
    assert(beam.restore() == reference.front().moves);
  }

  // 外から別nodeへ動かしても、次のstep開始時にrootへ戻る。
  beam.move_to(beam.beam.back(), apply_undo_move, revert_undo_move);
  assert(beam.state.depth == beam.depth());
  assert(beam.step(
      [&](const UndoState&) { return actions; }, apply_undo_move,
      revert_undo_move, undo_score));
  assert(beam.current_node == 0 && beam.state.depth == 0);

  struct LineState {
    int value = 0;
  };
  const auto line_apply = [](LineState& state, int move) { state.value += move; };
  const auto line_revert = [](LineState& state, int move) { state.value -= move; };
  TreeBeamSearch<LineState, int, int> keyed(LineState{}, 0, 10);
  for (int turn = 0; turn < 2; ++turn) {
    assert(keyed.step_with_key(
        [](const LineState&) { return std::vector<int>{-1, 1}; },
        line_apply, line_revert,
        [](const LineState& state) { return state.value; },
        [](const LineState& state) { return state.value; }));
  }
  assert(keyed.beam.size() == 3);

  // 木のDFS順がbeam順と違っても、同点は親rank→行動順で安定する。
  TreeBeamSearch<LineState, int, int> tied(LineState{}, 0, 2);
  for (int turn = 0; turn < 2; ++turn) {
    assert(tied.step(
        [](const LineState&) { return std::vector<int>{0, 1}; },
        line_apply, line_revert, [](const LineState&) { return 0; }));
  }
  assert(tied.restore(0) == std::vector<int>({0, 0}));
  assert(tied.restore(1) == std::vector<int>({0, 1}));
  tied.set_beam_width(1);
  assert(tied.beam.size() == 1);

  TreeBeamSearch<LineState, int, int> minimum(LineState{}, 0, 2, false);
  for (int turn = 0; turn < 2; ++turn) {
    assert(minimum.step(
        [](const LineState&) { return std::vector<int>{1, 2}; },
        line_apply, line_revert,
        [](const LineState& state) { return std::abs(5 - state.value); }));
  }
  assert(minimum.best_score() == 1);
  assert(minimum.restore() == std::vector<int>({2, 2}));

  TreeBeamSearch<LineState, int, double> nan_beam(LineState{}, 0.0, 1);
  assert(nan_beam.step(
      [](const LineState&) { return std::vector<int>{1, 2}; }, line_apply,
      line_revert,
      [](const LineState& state) {
        return state.value == 1
                   ? std::numeric_limits<double>::quiet_NaN()
                   : static_cast<double>(state.value);
      }));
  assert(nan_beam.best_score() == 2.0);

  const int old_depth = minimum.depth();
  assert(!minimum.step(
      [](const LineState&) { return std::vector<int>{}; }, line_apply,
      line_revert, [](const LineState& state) { return state.value; }));
  assert(minimum.depth() == old_depth);
  expect_invalid_argument([] {
    TreeBeamSearch<LineState, int, int> invalid(LineState{}, 0, 0);
  });
}

struct CostMove {
  int difference;
  int advance;

  bool operator==(const CostMove& other) const {
    return difference == other.difference && advance == other.advance;
  }
};

struct CostState {
  int value = 0;
  int generation = 0;
};

void apply_cost_move(CostState& state, const CostMove& move) {
  state.value += move.difference;
  state.generation += move.advance;
}

void revert_cost_move(CostState& state, const CostMove& move) {
  state.value -= move.difference;
  state.generation -= move.advance;
}

void test_cost_tree_beam_search() {
  constexpr int width = 3;
  constexpr int max_generation = 8;
  const std::vector<CostMove> actions{{1, 1}, {4, 2}, {-1, 3}};
  const auto expand = [&](const CostState&)
      -> const std::vector<CostMove>& { return actions; };
  const auto evaluate = [](const CostState& state) { return state.value; };
  const auto get_advance = [](const CostMove& move) { return move.advance; };

  CostTreeBeamSearch<CostState, CostMove, int> beam(
      CostState{}, 0, width, max_generation);
  struct Reference {
    CostState state;
    std::vector<CostMove> moves;
  };
  std::vector<Reference> active{{CostState{}, {}}};
  std::vector<std::vector<Reference>> future(max_generation + 1);
  int generation = 0;

  while (true) {
    std::vector<int> touched;
    for (const Reference& parent : active) {
      for (const CostMove& move : actions) {
        if (generation + move.advance > max_generation) continue;
        Reference child = parent;
        apply_cost_move(child.state, move);
        child.moves.push_back(move);
        touched.push_back(child.state.generation);
        future[child.state.generation].push_back(std::move(child));
      }
    }
    std::sort(touched.begin(), touched.end());
    touched.erase(std::unique(touched.begin(), touched.end()), touched.end());
    for (int next : touched) {
      std::stable_sort(future[next].begin(), future[next].end(),
                       [](const Reference& a, const Reference& b) {
                         return a.state.value > b.state.value;
                       });
      if (future[next].size() > width) future[next].resize(width);
    }

    int next_generation = generation + 1;
    while (next_generation <= max_generation &&
           future[next_generation].empty()) {
      ++next_generation;
    }
    const bool expected_advanced = next_generation <= max_generation;
    const bool advanced = beam.step(expand, apply_cost_move, revert_cost_move,
                                    evaluate, get_advance);
    assert(advanced == expected_advanced);
    if (!advanced) break;

    generation = next_generation;
    active = std::move(future[generation]);
    future[generation].clear();
    assert(beam.generation() == generation);
    if (beam.size() != static_cast<int>(active.size())) {
      std::fprintf(stderr,
                   "cost beam mismatch at generation %d: actual=%d expected=%zu\n",
                   generation, beam.size(), active.size());
    }
    assert(beam.size() == static_cast<int>(active.size()));
    assert(beam.best_score() == active.front().state.value);
    std::vector<int> visited(active.size());
    beam.for_each_state(
        [&](int rank, const CostState& state) {
          visited[rank] = state.value;
        },
        apply_cost_move,
        revert_cost_move);
    for (int rank = 0; rank < static_cast<int>(active.size()); ++rank) {
      assert(visited[rank] == active[rank].state.value);
      if (beam.restore(rank) != active[rank].moves) {
        std::fprintf(stderr,
                     "cost beam path mismatch at generation %d rank %d\n",
                     generation, rank);
        std::fprintf(stderr, "actual:");
        for (const CostMove& move : beam.restore(rank)) {
          std::fprintf(stderr, " (%d,%d)", move.difference, move.advance);
        }
        std::fprintf(stderr, "\nexpected:");
        for (const CostMove& move : active[rank].moves) {
          std::fprintf(stderr, " (%d,%d)", move.difference, move.advance);
        }
        std::fprintf(stderr, "\n");
      }
      assert(beam.restore(rank) == active[rank].moves);
    }
  }

  CostState restored;
  for (const CostMove& move : beam.restore()) apply_cost_move(restored, move);
  assert(restored.generation == beam.generation());
  assert(restored.value == beam.best_score());

  CostTreeBeamSearch<CostState, CostMove, int, int> keyed(
      CostState{}, 0, 10, 2);
  assert(keyed.step_with_key(
      [](const CostState&) {
        return std::vector<CostMove>{{1, 1}, {1, 1}, {2, 1}};
      },
      apply_cost_move, revert_cost_move, evaluate, get_advance,
      [](const CostState& state) { return state.value; }));
  assert(keyed.size() == 2);
  assert(keyed.best_score() == 2);

  // 先にgeneration 2へ予約した同一keyの候補を、generation 1から
  // 後着した高得点候補で正しく置き換える。
  CostTreeBeamSearch<CostState, CostMove, int, int> late_key(
      CostState{}, 0, 10, 2);
  const auto late_expand = [](const CostState& state) {
    if (state.generation == 0) {
      return std::vector<CostMove>{{5, 2}, {2, 1}};
    }
    return std::vector<CostMove>{{8, 1}};
  };
  const auto same_key = [](const CostState&) { return 0; };
  assert(late_key.step_with_key(
      late_expand, apply_cost_move, revert_cost_move, evaluate, get_advance,
      same_key));
  assert(late_key.generation() == 1 && late_key.best_score() == 2);
  assert(late_key.step_with_key(
      late_expand, apply_cost_move, revert_cost_move, evaluate, get_advance,
      same_key));
  assert(late_key.generation() == 2 && late_key.size() == 1);
  assert(late_key.best_score() == 10);
  assert(late_key.restore() ==
         std::vector<CostMove>({{2, 1}, {8, 1}}));

  CostTreeBeamSearch<CostState, CostMove, int> minimum(
      CostState{}, 0, 1, 1, false);
  assert(minimum.step(
      [](const CostState&) {
        return std::vector<CostMove>{{1, 1}, {3, 1}};
      },
      apply_cost_move, revert_cost_move, evaluate, get_advance));
  assert(minimum.best_score() == 1);

  CostTreeBeamSearch<CostState, CostMove, double> nan_beam(
      CostState{}, 0.0, 1, 1);
  assert(nan_beam.step(
      [](const CostState&) {
        return std::vector<CostMove>{{1, 1}, {2, 1}};
      },
      apply_cost_move, revert_cost_move,
      [](const CostState& state) {
        return state.value == 1
                   ? std::numeric_limits<double>::quiet_NaN()
                   : static_cast<double>(state.value);
      },
      get_advance));
  assert(nan_beam.best_score() == 2.0);

  CostTreeBeamSearch<CostState, CostMove, int> zero_generation(
      CostState{}, 0, 1, 0);
  assert(zero_generation.run(
             expand, apply_cost_move, revert_cost_move, evaluate,
             get_advance) == 0);
  assert(zero_generation.restore().empty());

  CostTreeBeamSearch<CostState, CostMove, int, int> mixed_mode(
      CostState{}, 0, 1, 2);
  assert(mixed_mode.step(
      expand, apply_cost_move, revert_cost_move, evaluate, get_advance));
  expect_invalid_argument([&] {
    mixed_mode.step_with_key(
        expand, apply_cost_move, revert_cost_move, evaluate, get_advance,
        [](const CostState& state) { return state.value; });
  });

  expect_invalid_argument([] {
    CostTreeBeamSearch<CostState, CostMove, int> invalid({}, 0, 0, 10);
  });
  expect_invalid_argument([] {
    CostTreeBeamSearch<CostState, CostMove, int> invalid({}, 0, 1, -1);
  });
  expect_invalid_argument([&] {
    CostTreeBeamSearch<CostState, CostMove, int> invalid_move({}, 0, 1, 1);
    invalid_move.step(
        [](const CostState&) { return std::vector<CostMove>{{1, 0}}; },
        apply_cost_move, revert_cost_move, evaluate, get_advance);
  });

  // 有効候補を一度bufferへ積んだ後に不正advanceを見つけても、catch後に
  // 同じbeamを再利用でき、古い候補や壊れたDFS状態が残らない。
  CostTreeBeamSearch<CostState, CostMove, int> reusable_after_error(
      {}, 0, 2, 2);
  expect_invalid_argument([&] {
    reusable_after_error.step(
        [](const CostState&) {
          return std::vector<CostMove>{{5, 1}, {9, 0}};
        },
        apply_cost_move, revert_cost_move, evaluate, get_advance);
  });
  assert(reusable_after_error.generation() == 0);
  assert(reusable_after_error.restore().empty());
  assert(reusable_after_error.step(
      [](const CostState&) {
        return std::vector<CostMove>{{3, 1}, {1, 2}};
      },
      apply_cost_move, revert_cost_move, evaluate, get_advance));
  assert(reusable_after_error.generation() == 1);
  assert(reusable_after_error.best_score() == 3);
  assert(reusable_after_error.restore() ==
         std::vector<CostMove>({{3, 1}}));
}

int main() {
  test_simulated_annealing();
  test_simple_beam_search();
  test_tree_beam_search();
  test_cost_tree_beam_search();
}
