#include <cassert>
#include <cstdint>
#include <random>
#include "library/action-beam-search.hpp"

struct IntegerState { int rank = 0; std::vector<int> path; };

template <class Score>
void check(bool maximize, int width, int pattern, bool batched) {
  struct Expected { IntegerState state; Score score; std::size_t order; };
  ActionBeamSearch<IntegerState, int, Score> beam({}, Score(0), width, maximize);
  beam.set_batched_selection(batched);
  std::vector<Expected> reference{{{}, Score(0), 0}};
  std::mt19937_64 random(127);
  for (int depth = 0; depth < 5; ++depth) {
    std::vector<std::vector<Score>> values(reference.size());
    std::vector<Expected> expected;
    for (std::size_t p = 0; p < reference.size(); ++p) {
      for (int a = 0; a < 23; ++a) {
        Score value = Score(random() % 128);
        if (pattern == 1) value = Score(7);
        if (pattern == 2) value = (a & 1) ? std::numeric_limits<Score>::max() : std::numeric_limits<Score>::min();
        if (pattern == 3) value = Score(int(random() % 1000) - 500);
        if (pattern == 4) value = Score(p * 23 + a);
        values[p].push_back(value);
        auto state = reference[p].state;
        state.path.push_back(a);
        expected.push_back({std::move(state), value, expected.size()});
      }
    }
    std::sort(expected.begin(), expected.end(), [=](const Expected& a, const Expected& b) {
      if (a.score != b.score) return maximize ? a.score > b.score : a.score < b.score;
      return a.order < b.order;
    });
    if (expected.size() > std::size_t(width)) expected.resize(width);
    int rank = 0;
    assert(beam.step(
        [](const IntegerState&) { std::vector<int> actions(23); std::iota(actions.begin(), actions.end(), 0); return actions; },
        [&](const IntegerState& s, int a) { return values[s.rank][a]; },
        [&](IntegerState& s, int a) { s.path.push_back(a); s.rank = rank++; }));
    assert(beam.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
      expected[i].state.rank = int(i);
      assert(beam.states()[i].path == expected[i].state.path);
      assert(beam.scores()[i] == expected[i].score);
    }
    reference = std::move(expected);
  }
}

template <class Score>
void check_type() {
  for (bool maximize : {false, true}) for (int width : {1, 31, 600, 1200}) {
    for (int pattern = 0; pattern < 5; ++pattern) for (bool batched : {false, true}) {
      check<Score>(maximize, width, pattern, batched);
    }
  }
}

int main() {
  check_type<std::int8_t>();
  check_type<std::uint16_t>();
  check_type<std::int32_t>();
  check_type<std::int64_t>();
  check_type<std::uint64_t>();
}
