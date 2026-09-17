#include <array>
#include <cassert>
#include "library/action-beam-search.hpp"

struct ReusedState {
  static int constructed, assigned, grew;
  std::vector<int> path;
  ReusedState() = default;
  ReusedState(const ReusedState& other) : path(other.path) { ++constructed; }
  ReusedState(ReusedState&&) = default;
  ReusedState& operator=(const ReusedState& other) {
    ++assigned;
    if (path.capacity() < other.path.size()) ++grew;
    path = other.path;
    return *this;
  }
};
int ReusedState::constructed = 0;
int ReusedState::assigned = 0;
int ReusedState::grew = 0;

struct NoAssignState {
  const int marker = 712;
  std::vector<int> path;
};
static_assert(!std::is_copy_assignable_v<NoAssignState>);

template <class State>
void verify_paths() {
  ActionBeamSearch<State, int, int> beam(State{}, 0, 4);
  auto expand = [](const State&) { return std::array<int, 4>{{0, 1, 2, 3}}; };
  auto evaluate = [](const State&, int a) { return a; };
  auto apply = [](State& s, int a) { s.path.push_back(a); };
  for (int depth = 0; depth < 40; ++depth) {
    assert(beam.step(expand, evaluate, apply));
    for (const auto& s : beam.states()) assert(s.path.size() == std::size_t(depth + 1));
    assert(beam.best().path == std::vector<int>(depth + 1, 3));
    if (depth == 9) beam.set_width(2);
    if (depth == 19) beam.set_width(7);
    if (depth == 29) beam.release_memory();
  }
  beam.reset(State{}, 0);
  assert(beam.step(expand, evaluate, apply));
  assert(beam.best().path == std::vector<int>{3});
}

void verify_capacity_reuse() {
  ReusedState::constructed = ReusedState::assigned = ReusedState::grew = 0;
  ActionBeamSearch<ReusedState, int, int> beam({}, 0, 4);
  auto expand = [](const ReusedState&) { return std::array<int, 4>{{0, 1, 2, 3}}; };
  auto evaluate = [](const ReusedState&, int a) { return a; };
  auto apply = [](ReusedState& s, int a) {
    s.path.reserve(100);  // warm both State buffers, then never allocate again
    s.path.push_back(a);
  };
  assert(beam.step(expand, evaluate, apply));
  assert(beam.step(expand, evaluate, apply));
  const int constructed = ReusedState::constructed;
  const int assigned = ReusedState::assigned;
  const int grew = ReusedState::grew;
  for (int i = 0; i < 20; ++i) assert(beam.step(expand, evaluate, apply));
  assert(ReusedState::constructed == constructed);
  assert(ReusedState::assigned == assigned + 80);
  assert(ReusedState::grew == grew);
}

int main() {
  verify_paths<ReusedState>();
  verify_paths<NoAssignState>();
  verify_capacity_reuse();
}
