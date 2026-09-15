#include <array>
#include <cassert>
#include <memory>
#include <random>
#include "library/action-beam-search.hpp"

struct SmallAction { int id; };
struct LargeAction { int id; std::array<int, 64> padding{}; };
struct ConstAction { const int id; }; // trivialでも代入不可。ID選抜で使えること。
struct OwnedAction {
  int id;
  std::unique_ptr<int> payload;
  explicit OwnedAction(int id_value) : id(id_value), payload(new int(id_value)) {}
  OwnedAction(OwnedAction&&) = default;
  OwnedAction& operator=(OwnedAction&&) = default;
};
struct SelectionState { int id = 0; std::vector<int> path; };
struct Expected { SelectionState state; double score; std::size_t order; };
struct CustomScore {
  int value;
  CustomScore() = delete;
  explicit CustomScore(int score) : value(score) {}
  bool operator<(const CustomScore& other) const { return value < other.value; }
  bool operator>(const CustomScore& other) const { return value > other.value; }
};

void test_custom_score() {
  // 軽量な独自Scoreも可。候補の縮小にデフォルト構築を要求しない。
  ActionBeamSearch<int, SmallAction, CustomScore> beam(0, CustomScore(0), 2);
  assert(beam.step(
      [](int) { return std::array<SmallAction, 5>{{{0}, {1}, {2}, {3}, {4}}}; },
      [](int, SmallAction action) { return CustomScore(action.id); },
      [](int& state, SmallAction action) { state = action.id; }));
  assert(beam.states() == std::vector<int>({4, 3}));
  assert(beam.scores()[0].value == 4 && beam.scores()[1].value == 3);
}

bool score_better(double a, double b, bool maximize) {
  if (std::isnan(a) != std::isnan(b)) return !std::isnan(a);
  if (std::isnan(a)) return false;
  return maximize ? a > b : a < b;
}

template <class Action>
void compare_with_full_sort(bool maximize, int width, bool threshold_mode, int pattern) {
  ActionBeamSearch<SelectionState, Action, double> beam({}, 0, width, maximize);
  std::vector<Expected> reference{{{}, 0, 0}};
  std::mt19937 random(817 + pattern);
  for (int depth = 0; depth < 5; ++depth) {
    std::vector<std::vector<double>> values(reference.size());
    for (auto& row : values) {
      for (int i = 0; i < 71; ++i) {
        double value = static_cast<int>(random() % 13) - 6;
        if (pattern == 1) value = i;
        if (pattern == 2) value = -i;
        if (pattern == 3) value = 0;
        if (pattern == 4 && i % 9 == 0) value = std::numeric_limits<double>::quiet_NaN();
        if (pattern == 4 && i % 11 == 0) value = std::numeric_limits<double>::infinity();
        if (pattern == 5) value = std::numeric_limits<double>::quiet_NaN();
        row.push_back(value);
      }
    }
    std::vector<Expected> next;
    for (std::size_t parent = 0; parent < reference.size(); ++parent) {
      for (int id = 0; id < 71; ++id) {
        auto state = reference[parent].state;
        state.path.push_back(id);
        next.push_back({std::move(state), values[parent][id], next.size()});
      }
    }
    std::sort(next.begin(), next.end(), [=](const Expected& a, const Expected& b) {
      if (score_better(a.score, b.score, maximize)) return true;
      if (score_better(b.score, a.score, maximize)) return false;
      return a.order < b.order;
    });
    if (next.size() > static_cast<std::size_t>(width)) next.resize(width);
    for (std::size_t rank = 0; rank < next.size(); ++rank) next[rank].state.id = static_cast<int>(rank);

    auto expand = [](const SelectionState&) {
      std::vector<Action> actions;
      for (int i = 0; i < 71; ++i) actions.push_back(Action{i});
      return actions;
    };
    auto evaluate = [&](const SelectionState& state, const Action& action) { return values[state.id][action.id]; };
    int applied = 0;
    auto apply = [&](SelectionState& state, Action& action) {
      state.path.push_back(action.id);
      state.id = applied++;
    };
    if (threshold_mode) {
      auto bounded = [&](const SelectionState& state, const Action& action, const double* threshold)
          -> std::optional<double> {
        double score = evaluate(state, action);
        if (threshold && !score_better(score, *threshold, maximize)) return std::nullopt;
        return score;
      };
      assert(beam.step_with_threshold(expand, bounded, apply));
    } else {
      assert(beam.step(expand, evaluate, apply));
    }
    assert(beam.size() == next.size());
    for (std::size_t rank = 0; rank < next.size(); ++rank) {
      assert(beam.states()[rank].path == next[rank].state.path);
      assert(beam.scores()[rank] == next[rank].score ||
             (std::isnan(beam.scores()[rank]) && std::isnan(next[rank].score)));
    }
    assert(beam.last_buffered_peak_count() <= 2 * static_cast<std::size_t>(width));
    reference = std::move(next);
  }
}

int main() {
  test_custom_score();
  for (bool maximize : {false, true}) for (bool threshold : {false, true}) {
    for (int width : {1, 2, 17, 80}) for (int pattern = 0; pattern < 6; ++pattern) {
      compare_with_full_sort<SmallAction>(maximize, width, threshold, pattern);
      compare_with_full_sort<LargeAction>(maximize, width, threshold, pattern);
      compare_with_full_sort<ConstAction>(maximize, width, threshold, pattern);
      compare_with_full_sort<OwnedAction>(maximize, width, threshold, pattern);
    }
  }
}
