#define main ahc021_submission_main
#include "examples/search/ahc021_tree_beam.cpp"
#undef main
#include <cassert>

void check_frontier(const PyramidProblem& problem, const PyramidProblem::State& state) {
  for (int vertex = 0; vertex < CELL_COUNT; ++vertex) {
    const bool expected = !state.fixed[vertex] && problem.parents_are_fixed(state, vertex);
    assert(((state.frontier[problem.row[vertex]] >> problem.column[vertex]) & 1U) == expected);
  }
}

void check_full_dijkstra(const PyramidProblem& problem, const PyramidProblem::State& state,
                         const std::vector<PyramidProblem::Move>& moves) {
  std::array<int, CELL_COUNT> distance, previous;
  distance.fill(std::numeric_limits<int>::max() / 4);
  previous.fill(-1);
  using Entry = std::pair<int, int>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
  const int start = state.position_of_value[state.next_value];
  distance[start] = 0;
  queue.push({0, start});
  while (!queue.empty()) {
    const auto [cost, vertex] = queue.top(); queue.pop();
    if (distance[vertex] != cost) continue;
    for (int next : problem.adjacent[vertex]) {
      if (state.fixed[next]) continue;
      const int candidate = cost + problem.edge_cost(state, vertex, next);
      if (candidate < distance[next]) {
        distance[next] = candidate; previous[next] = vertex;
        queue.push({candidate, next});
      }
    }
  }
  int expected_count = 0;
  for (int vertex = 0; vertex < CELL_COUNT; ++vertex)
    expected_count += !state.fixed[vertex] && problem.parents_are_fixed(state, vertex) &&
                      distance[vertex] < std::numeric_limits<int>::max() / 4;
  assert(moves.size() == static_cast<std::size_t>(expected_count));
  for (const auto& move : moves) {
    assert(move.rank_cost == distance[move.path.back()]);
    std::vector<std::uint16_t> path;
    for (int vertex = move.path.back(); vertex != -1; vertex = previous[vertex]) path.push_back(vertex);
    std::reverse(path.begin(), path.end());
    assert(path == move.path); // 得点だけでなく同距離の経路選択まで一致。
  }
}

int main() {
  std::mt19937_64 random(3129);
  for (int seed = 0; seed < 3; ++seed) {
    std::array<int, CELL_COUNT> input{};
    std::iota(input.begin(), input.end(), 0);
    std::shuffle(input.begin(), input.end(), random);
    PyramidProblem problem;
    auto state = problem.initial_state(input);
    for (int turn = 0; turn < CELL_COUNT; ++turn) {
      check_frontier(problem, state);
      const auto moves = problem.generate_moves(state);
      check_full_dijkstra(problem, state, moves);
      auto move = moves[random() % moves.size()];
      const auto before = state;
      problem.apply_move(state, move);
      check_frontier(problem, state);
      problem.revert_move(state, move);
      assert(state.value == before.value && state.position_of_value == before.position_of_value);
      assert(state.fixed == before.fixed && state.frontier == before.frontier);
      assert(state.hash == before.hash && state.next_value == before.next_value);
      assert(state.operations == before.operations && state.rank_cost == before.rank_cost);
      problem.apply_move(state, move);
    }
    problem.validate(state);
    assert(problem.generate_moves(state).empty());
  }
}
