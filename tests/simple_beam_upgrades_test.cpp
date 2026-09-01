#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "../library/simple-beam-search.hpp"

struct TestState {
  int id = 0;
  int key = 0;
  double score = 0.0;
  bool terminal = false;
  std::vector<int> path;
};

bool same_state(const TestState& left, const TestState& right) {
  const bool same_score =
      (std::isnan(left.score) && std::isnan(right.score)) ||
      left.score == right.score;
  return left.id == right.id && left.key == right.key &&
         same_score && left.terminal == right.terminal &&
         left.path == right.path;
}

void test_terminal_observer_before_pruning() {
  SimpleBeamSearch<TestState, double> beam(TestState{}, 1);
  int observed_count = 0;
  int best_terminal_id = -1;
  double best_terminal_score = -1.0;

  assert(beam.step_and_observe(
      [](const TestState&) {
        return std::vector<TestState>{
            {10, 0, 0.0, true, {10}},
            {20, 0, 1.0, false, {20}}};
      },
      [](const TestState& state) { return state.score; },
      [&](auto& state, auto& rank_score) {
        static_assert(std::is_const_v<
                      std::remove_reference_t<decltype(state)>>);
        static_assert(std::is_const_v<
                      std::remove_reference_t<decltype(rank_score)>>);
        ++observed_count;
        assert(rank_score == state.score);
        if (state.terminal && best_terminal_score < 100.0) {
          best_terminal_score = 100.0;
          best_terminal_id = state.id;
        }
      }));

  assert(observed_count == 2);
  assert(best_terminal_id == 10);
  assert(beam.best().id == 20);
  assert(beam.last_generated_count() == 2);
  assert(beam.last_unique_count() == 2);
  assert(beam.last_kept_count() == 1);
}

void test_emitter_matches_container_api() {
  SimpleBeamSearch<TestState, double> by_container(TestState{}, 4);
  SimpleBeamSearch<TestState, double> by_emitter(TestState{}, 4);

  for (int turn = 0; turn < 5; ++turn) {
    const auto evaluate = [](const TestState& state) {
      return state.score;
    };
    assert(by_container.step(
        [turn](const TestState& parent) {
          std::vector<TestState> children;
          for (int action = 0; action < 3; ++action) {
            TestState child = parent;
            child.id = parent.id * 3 + action + 1;
            child.key = child.id % 5;
            child.score += static_cast<double>((turn + action) % 3);
            child.path.push_back(action);
            children.push_back(std::move(child));
          }
          return children;
        }, evaluate));
    assert(by_emitter.step_each(
        [turn](const TestState& parent, auto&& emit) {
          for (int action = 0; action < 3; ++action) {
            TestState child = parent;
            child.id = parent.id * 3 + action + 1;
            child.key = child.id % 5;
            child.score += static_cast<double>((turn + action) % 3);
            child.path.push_back(action);
            emit(std::move(child));
          }
        }, evaluate));

    assert(by_container.size() == by_emitter.size());
    for (std::size_t index = 0; index < by_container.size(); ++index) {
      assert(same_state(by_container.states()[index],
                        by_emitter.states()[index]));
    }
  }
}

void test_keyed_observer_order_and_counts() {
  SimpleBeamSearch<TestState, double> beam(TestState{}, 10);
  std::vector<int> observed_ids;
  std::vector<double> observed_scores;
  assert(beam.step_with_key_and_observe(
      [](const TestState&) {
        return std::vector<TestState>{
            {1, 7, 1.0, false, {}},
            {2, 7, 9.0, false, {}},
            {3, 8, 4.0, false, {}}};
      },
      [](const TestState& state) { return state.score; },
      [](const TestState& state) { return state.key; },
      [&](const TestState& state, const double& score) {
        observed_ids.push_back(state.id);
        observed_scores.push_back(score);
      }));

  assert(observed_ids == std::vector<int>({1, 2, 3}));
  assert(observed_scores == std::vector<double>({1.0, 9.0, 4.0}));
  assert(beam.last_generated_count() == 3);
  assert(beam.last_unique_count() == 2);
  assert(beam.last_kept_count() == 2);
  assert(beam.states()[0].id == 2);
  assert(beam.states()[1].id == 3);
}

void test_emitter_keyed_and_observer() {
  SimpleBeamSearch<TestState, double> beam(TestState{}, 2);
  int observed = 0;
  assert(beam.step_each_with_key_and_observe(
      [](const TestState&, auto&& emit) {
        emit(TestState{1, 1, 3.0, false, {}});
        emit(TestState{2, 1, 5.0, false, {}});
        emit(TestState{3, 2, 4.0, false, {}});
      },
      [](const TestState& state) { return state.score; },
      [](const TestState& state) { return state.key; },
      [&](const TestState&, const double&) { ++observed; }));
  assert(observed == 3);
  assert(beam.best().id == 2);
  assert(beam.last_generated_count() == 3);
  assert(beam.last_unique_count() == 2);
  assert(beam.last_kept_count() == 2);
}

void test_ties_nan_and_minimize() {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  SimpleBeamSearch<TestState, double> maximum(TestState{}, 3);
  assert(maximum.step_each(
      [nan](const TestState&, auto&& emit) {
        emit(TestState{1, 0, nan, false, {}});
        emit(TestState{2, 0, 5.0, false, {}});
        emit(TestState{3, 0, 5.0, false, {}});
        emit(TestState{4, 0, 1.0, false, {}});
      },
      [](const TestState& state) { return state.score; }));
  assert(maximum.states()[0].id == 2);
  assert(maximum.states()[1].id == 3);
  assert(maximum.states()[2].id == 4);

  SimpleBeamSearch<TestState, double> minimum(TestState{}, 3, false);
  assert(minimum.step_each(
      [nan](const TestState&, auto&& emit) {
        emit(TestState{1, 0, nan, false, {}});
        emit(TestState{2, 0, 2.0, false, {}});
        emit(TestState{3, 0, 2.0, false, {}});
        emit(TestState{4, 0, 7.0, false, {}});
      },
      [](const TestState& state) { return state.score; }));
  assert(minimum.states()[0].id == 2);
  assert(minimum.states()[1].id == 3);
  assert(minimum.states()[2].id == 4);
}

struct ConstructOnlyScore {
  int value;

  explicit ConstructOnlyScore(int initial_value) : value(initial_value) {}
  ConstructOnlyScore(const ConstructOnlyScore&) = delete;
  ConstructOnlyScore& operator=(const ConstructOnlyScore&) = delete;
  ConstructOnlyScore(ConstructOnlyScore&&) noexcept = default;
  ConstructOnlyScore& operator=(ConstructOnlyScore&&) = delete;
};

bool operator<(const ConstructOnlyScore& left,
               const ConstructOnlyScore& right) {
  return left.value < right.value;
}

void test_non_assignable_score_with_normal_step() {
  static_assert(std::is_move_constructible_v<ConstructOnlyScore>);
  static_assert(!std::is_move_assignable_v<ConstructOnlyScore>);

  SimpleBeamSearch<TestState, ConstructOnlyScore> beam(TestState{}, 2);
  assert(beam.step(
      [](const TestState&) {
        return std::vector<TestState>{
            {1, 0, 1.0, false, {}},
            {2, 0, 3.0, false, {}},
            {3, 0, 2.0, false, {}}};
      },
      [](const TestState& state) {
        return ConstructOnlyScore(static_cast<int>(state.score));
      }));
  assert(beam.states()[0].id == 2);
  assert(beam.states()[1].id == 3);
}

struct NonDefaultState {
  int value;

  explicit NonDefaultState(int initial_value) : value(initial_value) {}
  NonDefaultState(const NonDefaultState&) = delete;
  NonDefaultState& operator=(const NonDefaultState&) = delete;
  NonDefaultState(NonDefaultState&&) noexcept = default;
  NonDefaultState& operator=(NonDefaultState&&) = delete;
};

void test_set_width_with_non_default_state() {
  static_assert(std::is_move_constructible_v<NonDefaultState>);
  static_assert(!std::is_move_assignable_v<NonDefaultState>);
  SimpleBeamSearch<NonDefaultState, int> beam(NonDefaultState(0), 3);
  assert(beam.step_each(
      [](const NonDefaultState&, auto&& emit) {
        emit(NonDefaultState(1));
        emit(NonDefaultState(3));
        emit(NonDefaultState(2));
      },
      [](const NonDefaultState& state) { return state.value; }));
  assert(beam.size() == 3);
  beam.set_width(1);
  assert(beam.size() == 1);
  assert(beam.best().value == 3);
}

struct MoveOnlyState {
  std::unique_ptr<int> value;
};

void test_move_only_emitter_reset_and_release() {
  SimpleBeamSearch<MoveOnlyState, int> beam(
      MoveOnlyState{std::make_unique<int>(1)}, 2);
  assert(beam.step_each(
      [](const MoveOnlyState& parent, auto&& emit) {
        emit(MoveOnlyState{
            std::make_unique<int>(*parent.value + 1)});
        emit(MoveOnlyState{
            std::make_unique<int>(*parent.value + 3)});
      },
      [](const MoveOnlyState& state) { return *state.value; }));
  assert(*beam.best().value == 4);
  assert(beam.depth() == 1);

  beam.release_memory();
  assert(*beam.best().value == 4);
  assert(beam.depth() == 1);

  beam.reset(MoveOnlyState{std::make_unique<int>(10)});
  assert(beam.size() == 1);
  assert(*beam.best().value == 10);
  assert(beam.depth() == 0);
  assert(beam.last_generated_count() == 0);
  assert(beam.last_unique_count() == 0);
  assert(beam.last_kept_count() == 0);
}

struct CustomKey {
  int value;
};

struct AllCollisionHash {
  std::size_t operator()(const CustomKey&) const { return 0; }
};

struct CustomKeyEqual {
  bool operator()(const CustomKey& left, const CustomKey& right) const {
    return left.value == right.value;
  }
};

void test_custom_hash_equal() {
  SimpleBeamSearch<TestState, double> beam(TestState{}, 10);
  assert(beam.step_with_key(
      [](const TestState&) {
        return std::vector<TestState>{
            {1, 10, 1.0, false, {}},
            {2, 20, 2.0, false, {}},
            {3, 10, 3.0, false, {}}};
      },
      [](const TestState& state) { return state.score; },
      [](const TestState& state) { return CustomKey{state.key}; },
      AllCollisionHash{}, CustomKeyEqual{}));
  assert(beam.size() == 2);
  assert(beam.states()[0].id == 3);
  assert(beam.states()[1].id == 2);

  beam.reset(TestState{});
  int observed = 0;
  assert(beam.step_with_key_and_observe(
      [](const TestState&) {
        return std::vector<TestState>{
            {4, 30, 4.0, false, {}},
            {5, 30, 5.0, false, {}}};
      },
      [](const TestState& state) { return state.score; },
      [](const TestState& state) { return CustomKey{state.key}; },
      AllCollisionHash{}, CustomKeyEqual{},
      [&](const TestState&, const double&) { ++observed; }));
  assert(observed == 2);
  assert(beam.size() == 1);
  assert(beam.best().id == 5);
}

struct CopyConstructMoveAssignState {
  int id;
  int key;
  int score;

  CopyConstructMoveAssignState(int initial_id,
                               int initial_key,
                               int initial_score)
      : id(initial_id), key(initial_key), score(initial_score) {}
  CopyConstructMoveAssignState(const CopyConstructMoveAssignState&) = default;
  CopyConstructMoveAssignState& operator=(
      const CopyConstructMoveAssignState&) = delete;
  CopyConstructMoveAssignState(CopyConstructMoveAssignState&&) noexcept =
      default;
  CopyConstructMoveAssignState& operator=(
      CopyConstructMoveAssignState&&) noexcept = default;
};

void test_keyed_const_container_without_copy_assignment() {
  static_assert(
      std::is_copy_constructible_v<CopyConstructMoveAssignState>);
  static_assert(
      !std::is_copy_assignable_v<CopyConstructMoveAssignState>);
  static_assert(
      std::is_move_assignable_v<CopyConstructMoveAssignState>);

  const std::vector<CopyConstructMoveAssignState> children{
      {1, 7, 10}, {2, 7, 20}};
  SimpleBeamSearch<CopyConstructMoveAssignState, int> beam(
      CopyConstructMoveAssignState(0, 0, 0), 2);
  assert(beam.step_with_key(
      [&](const CopyConstructMoveAssignState&)
          -> const std::vector<CopyConstructMoveAssignState>& {
        return children;
      },
      [](const CopyConstructMoveAssignState& state) {
        return state.score;
      },
      [](const CopyConstructMoveAssignState& state) {
        return state.key;
      }));
  assert(children[0].id == 1);
  assert(children[1].id == 2);
  assert(beam.size() == 1);
  assert(beam.best().id == 2);
}

struct ConstContractKeyMaker {
  int mutable_calls = 0;
  int const_calls = 0;

  int operator()(TestState&) {
    ++mutable_calls;
    return -1;
  }

  int operator()(const TestState& state) {
    ++const_calls;
    return state.key;
  }
};

void test_make_key_uses_const_state_contract() {
  SimpleBeamSearch<TestState, double> beam(TestState{}, 3);
  ConstContractKeyMaker make_key;
  assert(beam.step_each_with_key(
      [](const TestState&, auto&& emit) {
        emit(TestState{1, 10, 1.0, false, {}});
        emit(TestState{2, 20, 2.0, false, {}});
      },
      [](const TestState& state) { return state.score; }, make_key));
  assert(make_key.mutable_calls == 0);
  assert(make_key.const_calls == 2);
  assert(beam.size() == 2);
}

struct MoveTrackedState {
  int value = 0;

  MoveTrackedState() = default;
  explicit MoveTrackedState(int initial_value) : value(initial_value) {}
  MoveTrackedState(const MoveTrackedState&) = default;
  MoveTrackedState& operator=(const MoveTrackedState&) = default;
  MoveTrackedState(MoveTrackedState&& other) noexcept : value(other.value) {
    other.value = -1;
  }
  MoveTrackedState& operator=(MoveTrackedState&& other) noexcept {
    value = other.value;
    other.value = -1;
    return *this;
  }
};

void test_old_container_copy_and_move_contract() {
  std::vector<MoveTrackedState> mutable_children;
  mutable_children.emplace_back(1);
  mutable_children.emplace_back(2);
  SimpleBeamSearch<MoveTrackedState, int> moved(MoveTrackedState{}, 2);
  assert(moved.step(
      [&](const MoveTrackedState&) -> std::vector<MoveTrackedState>& {
        return mutable_children;
      },
      [](const MoveTrackedState& state) { return state.value; }));
  assert(mutable_children[0].value == -1);
  assert(mutable_children[1].value == -1);

  const std::vector<MoveTrackedState> constant_children{
      MoveTrackedState(4), MoveTrackedState(7)};
  SimpleBeamSearch<MoveTrackedState, int> copied(MoveTrackedState{}, 2);
  assert(copied.step(
      [&](const MoveTrackedState&)
          -> const std::vector<MoveTrackedState>& {
        return constant_children;
      },
      [](const MoveTrackedState& state) { return state.value; }));
  assert(constant_children[0].value == 4);
  assert(constant_children[1].value == 7);
}

void test_empty_step_counts_and_state() {
  SimpleBeamSearch<TestState, double> beam(
      TestState{9, 0, 0.0, false, {}}, 2);
  assert(beam.step_each(
      [](const TestState&, auto&& emit) {
        emit(TestState{10, 0, 1.0, false, {}});
      },
      [](const TestState& state) { return state.score; }));
  const int depth = beam.depth();
  const int best_id = beam.best().id;
  assert(!beam.step_each(
      [](const TestState&, auto&&) {},
      [](const TestState& state) { return state.score; }));
  assert(beam.depth() == depth);
  assert(beam.best().id == best_id);
  assert(beam.last_generated_count() == 0);
  assert(beam.last_unique_count() == 0);
  assert(beam.last_kept_count() == 0);
}

struct DifferentialState {
  std::uint64_t id;
  int depth;
  int key;
  double score;
  bool terminal;
};

std::uint64_t differential_mix(std::uint64_t value) {
  value += 0x9e3779b97f4a7c15ULL;
  value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31U);
}

std::vector<DifferentialState> make_differential_children(
    const DifferentialState& parent,
    int seed) {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  if (parent.terminal || parent.depth >= 6) return {};

  // 最初の層は、key置換・同点・NaNを必ず全て通る固定ケース。
  if (parent.depth == 0) {
    return {
        {100, 1, 0, 1.0, false},
        {101, 1, 1, nan, false},
        {102, 1, 2, -2.0, false},
        {103, 1, 3, 5.0, false},
        {104, 1, 4, 5.0, false},
        {105, 1, 0, 5.0, false},
        {106, 1, 1, 0.0, false},
        {107, 1, 2, -2.0, false},
        {108, 1, 5, -2.0, false},
        {109, 1, 5, 2.0, false},
        {110, 1, 6, 2.0, false},
        {111, 1, 6, -2.0, false},
        {112, 1, 7, nan, false},
        {113, 1, 8, nan, true},
    };
  }

  const std::uint64_t base = differential_mix(
      parent.id ^ (static_cast<std::uint64_t>(seed) << 32U) ^
      static_cast<std::uint64_t>(parent.depth));
  const int child_count = 3 + static_cast<int>(base & 1ULL);
  std::vector<DifferentialState> children;
  children.reserve(static_cast<std::size_t>(child_count));
  for (int action = 0; action < child_count; ++action) {
    const int child_depth = parent.depth + 1;
    const std::uint64_t id =
        parent.id * 5ULL + static_cast<std::uint64_t>(action + 1);
    const std::uint64_t random_value = differential_mix(
        base ^ id ^ (static_cast<std::uint64_t>(action) << 48U));
    const int key = static_cast<int>(
        (parent.id % 3ULL + static_cast<std::uint64_t>(action % 2)) %
        5ULL);
    const bool terminal =
        child_depth >= 6 ||
        (action + 1 == child_count && child_depth % 2 == 1);
    const double score = terminal
                             ? nan
                             : static_cast<double>(
                                   static_cast<int>(random_value % 7ULL) - 3);
    children.push_back({id, child_depth, key, score, terminal});
  }
  return children;
}

bool differential_score_is_better(double left,
                                  double right,
                                  bool maximize) {
  const bool left_is_nan = std::isnan(left);
  const bool right_is_nan = std::isnan(right);
  if (left_is_nan != right_is_nan) return !left_is_nan;
  if (left_is_nan) return false;
  return maximize ? right < left : left < right;
}

bool same_differential_state(const DifferentialState& left,
                             const DifferentialState& right) {
  const bool same_score =
      (std::isnan(left.score) && std::isnan(right.score)) ||
      left.score == right.score;
  return left.id == right.id && left.depth == right.depth &&
         left.key == right.key && same_score &&
         left.terminal == right.terminal;
}

struct DifferentialCandidate {
  DifferentialState state;
  std::size_t order;
};

struct DifferentialStepResult {
  bool advanced = false;
  std::size_t generated = 0;
  std::size_t unique = 0;
  std::size_t kept = 0;
};

struct DifferentialCoverage {
  bool saw_finite_tie = false;
  bool saw_nan = false;
  bool saw_duplicate_key = false;
  bool saw_terminal_parent = false;
  bool saw_empty_step = false;
};

DifferentialStepResult differential_reference_step(
    std::vector<DifferentialState>& beam,
    int beam_width,
    bool maximize,
    bool keyed,
    int seed,
    DifferentialCoverage& coverage) {
  std::vector<DifferentialCandidate> candidates;
  std::vector<double> generated_scores;
  std::vector<int> generated_keys;
  DifferentialStepResult result;

  for (const DifferentialState& parent : beam) {
    if (parent.terminal) coverage.saw_terminal_parent = true;
    const std::vector<DifferentialState> children =
        make_differential_children(parent, seed);
    for (const DifferentialState& child : children) {
      const std::size_t order = result.generated++;
      if (std::isnan(child.score)) coverage.saw_nan = true;
      for (double previous_score : generated_scores) {
        if (!std::isnan(child.score) &&
            !std::isnan(previous_score) &&
            child.score == previous_score) {
          coverage.saw_finite_tie = true;
        }
      }
      generated_scores.push_back(child.score);
      if (std::find(generated_keys.begin(), generated_keys.end(),
                    child.key) != generated_keys.end()) {
        coverage.saw_duplicate_key = true;
      }
      generated_keys.push_back(child.key);

      if (!keyed) {
        candidates.push_back({child, order});
        continue;
      }

      auto found = std::find_if(
          candidates.begin(), candidates.end(),
          [&](const DifferentialCandidate& candidate) {
            return candidate.state.key == child.key;
          });
      if (found == candidates.end()) {
        candidates.push_back({child, order});
      } else if (differential_score_is_better(
                     child.score, found->state.score, maximize)) {
        *found = DifferentialCandidate{child, order};
      }
    }
  }

  result.unique = candidates.size();
  if (candidates.empty()) {
    coverage.saw_empty_step = true;
    return result;
  }

  // key置換で変わった生成順をまず復元し、その後のstable_sortで
  // 同点を生成順のまま残す。実装側のnth_elementとは独立した参照になる。
  std::stable_sort(
      candidates.begin(), candidates.end(),
      [](const DifferentialCandidate& left,
         const DifferentialCandidate& right) {
        return left.order < right.order;
      });
  std::stable_sort(
      candidates.begin(), candidates.end(),
      [&](const DifferentialCandidate& left,
          const DifferentialCandidate& right) {
        return differential_score_is_better(
            left.state.score, right.state.score, maximize);
      });

  result.kept = std::min(
      static_cast<std::size_t>(beam_width), candidates.size());
  std::vector<DifferentialState> next_beam;
  next_beam.reserve(result.kept);
  for (std::size_t index = 0; index < result.kept; ++index) {
    next_beam.push_back(candidates[index].state);
  }
  beam = std::move(next_beam);
  result.advanced = true;
  return result;
}

void assert_differential_states(
    const std::vector<DifferentialState>& actual,
    const std::vector<DifferentialState>& expected) {
  assert(actual.size() == expected.size());
  for (std::size_t index = 0; index < actual.size(); ++index) {
    assert(same_differential_state(actual[index], expected[index]));
  }
}

void run_differential_case(int seed,
                           bool use_emitter,
                           bool keyed,
                           bool maximize,
                           DifferentialCoverage& coverage) {
  const DifferentialState initial{1, 0, 0, 0.0, false};
  SimpleBeamSearch<DifferentialState, double> actual(initial, 4, maximize);
  std::vector<DifferentialState> expected{initial};
  int expected_depth = 0;
  bool stopped = false;
  bool saw_width_shrink = false;
  constexpr int widths[] = {16, 2, 7, 1, 5, 3, 4};

  for (int attempt = 0; attempt < 7; ++attempt) {
    const int width = widths[attempt];
    const std::size_t generated_before = actual.last_generated_count();
    const std::size_t unique_before = actual.last_unique_count();
    const std::size_t kept_before = actual.last_kept_count();
    if (expected.size() > static_cast<std::size_t>(width)) {
      saw_width_shrink = true;
    }
    actual.set_width(width);
    while (expected.size() > static_cast<std::size_t>(width)) {
      expected.pop_back();
    }
    assert(actual.width() == width);
    assert(actual.depth() == expected_depth);
    assert(actual.last_generated_count() == generated_before);
    assert(actual.last_unique_count() == unique_before);
    assert(actual.last_kept_count() == kept_before);
    assert_differential_states(actual.states(), expected);

    const DifferentialStepResult reference_result =
        differential_reference_step(expected, width, maximize, keyed,
                                    seed, coverage);
    const auto evaluate = [](const DifferentialState& state) {
      return state.score;
    };
    const auto make_key = [](const DifferentialState& state) {
      return state.key;
    };
    bool advanced = false;
    if (use_emitter) {
      const auto generate = [seed](const DifferentialState& parent,
                                   auto&& emit) {
        std::vector<DifferentialState> children =
            make_differential_children(parent, seed);
        for (DifferentialState& child : children) {
          emit(std::move(child));
        }
      };
      advanced = keyed
                     ? actual.step_each_with_key(generate, evaluate, make_key)
                     : actual.step_each(generate, evaluate);
    } else {
      const auto expand = [seed](const DifferentialState& parent) {
        return make_differential_children(parent, seed);
      };
      advanced = keyed
                     ? actual.step_with_key(expand, evaluate, make_key)
                     : actual.step(expand, evaluate);
    }

    assert(advanced == reference_result.advanced);
    assert(actual.last_generated_count() == reference_result.generated);
    assert(actual.last_unique_count() == reference_result.unique);
    assert(actual.last_kept_count() == reference_result.kept);
    if (reference_result.advanced) ++expected_depth;
    assert(actual.depth() == expected_depth);
    assert_differential_states(actual.states(), expected);

    if (!advanced) {
      stopped = true;
      break;
    }
  }
  assert(stopped);
  assert(saw_width_shrink);
}

void test_randomized_full_enumeration_oracle() {
  DifferentialCoverage coverage;
  for (int seed = 0; seed < 40; ++seed) {
    for (int emitter = 0; emitter < 2; ++emitter) {
      for (int keyed = 0; keyed < 2; ++keyed) {
        for (int maximize = 0; maximize < 2; ++maximize) {
          run_differential_case(seed, emitter != 0, keyed != 0,
                                maximize != 0, coverage);
        }
      }
    }
  }
  assert(coverage.saw_finite_tie);
  assert(coverage.saw_nan);
  assert(coverage.saw_duplicate_key);
  assert(coverage.saw_terminal_parent);
  assert(coverage.saw_empty_step);
}

int main() {
  test_terminal_observer_before_pruning();
  test_emitter_matches_container_api();
  test_keyed_observer_order_and_counts();
  test_emitter_keyed_and_observer();
  test_ties_nan_and_minimize();
  test_non_assignable_score_with_normal_step();
  test_set_width_with_non_default_state();
  test_move_only_emitter_reset_and_release();
  test_custom_hash_equal();
  test_keyed_const_container_without_copy_assignment();
  test_make_key_uses_const_state_contract();
  test_old_container_copy_and_move_contract();
  test_empty_step_counts_and_state();
  test_randomized_full_enumeration_oracle();
}
