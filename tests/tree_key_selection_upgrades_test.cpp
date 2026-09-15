#include <array>
#include <cassert>
#include "library/tree-beam-search.hpp"

struct CountedKey {
  int value;
  bool operator==(const CountedKey& other) const { return value == other.value; }
};
static int hash_calls = 0;
struct FinalProblem {
  struct State { int value = 0, depth = 0; };
  using Move = int;
  using Score = int;
  std::array<int, 3> generate_moves(const State&) const { return {1, 2, 3}; }
  void apply_move(State& state, int action) { state.value += action; ++state.depth; }
  void revert_move(State& state, int action) { state.value -= action; --state.depth; }
  int evaluate(const State& state) const { return state.value; }
};
namespace std {
template <> struct hash<CountedKey> {
  size_t operator()(const CountedKey& key) const noexcept {
    ++hash_calls;
    return static_cast<size_t>(key.value);
  }
};
}

int main() {
  // 同じSTL・同じkey列で、find→emplaceの二重検索よりhash呼び出しを減らせること。
  std::unordered_map<CountedKey, int> reference;
  reference.reserve(80 * 4);
  hash_calls = 0;
  for (int action = 0; action < 200; ++action) {
    CountedKey key{action % 77};
    const auto found = reference.find(key);
    if (found == reference.end()) reference.emplace(key, action);
    else found->second = action;
  }
  const int baseline_calls = hash_calls;
  TreeBeamSearch<int, int, int> beam(0, 0, 80);
  hash_calls = 0;
  assert(beam.step_with_key(
      [](int) { std::vector<int> actions(200); std::iota(actions.begin(), actions.end(), 0); return actions; },
      [](int& state, int action) { state = action; },
      [](int& state, int) { state = 0; },
      [](int state) { return state; },
      [](int state) { return CountedKey{state % 77}; }));
  assert(hash_calls < baseline_calls);
  assert(beam.size() == 77 && beam.state == 0 && beam.current_node == 0);
  assert(beam.last_generated_count() == 200 && beam.last_unique_count() == 77);
  for (int rank = 0; rank < beam.size(); ++rank) {
    assert(beam.restore(rank) == std::vector<int>{199 - rank});
  }
  FinalProblem problem;
  TreeBeamRunner<FinalProblem> runner(problem, {}, 0, 3);
  assert(runner.run(1) == 1);
  int best_rank = -1, best_value = 100, visited = 0;
  runner.for_each_state([&](int rank, const FinalProblem::State& state) {
    assert(state.depth == 1);
    ++visited;
    // この架空の問題では、探索順位と違って完成後はvalueが小さい方が良い。
    if (state.value < best_value) { best_value = state.value; best_rank = rank; }
  });
  assert(visited == 3 && best_rank == 2);
  assert(runner.best_score() == 3 && runner.restore(0) == std::vector<int>{3});
  assert(runner.restore(best_rank) == std::vector<int>{1});
  // 巡回後もrootへ完全に戻り、同じRunnerで探索を継続できる。
  assert(runner.step_and_observe([](int, int, const FinalProblem::State& state, int) {
    assert(state.depth == 2);
  }));
  assert(runner.best_score() == 6);
}
