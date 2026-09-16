#define main ahc015_submission_main
#include "../examples/search/ahc015_common_rollout.cpp"
#undef main
#include <cassert>

int main() {
  std::mt19937_64 engine(1515);
  CoalescedRollout<Board, int> shared;
  std::uint64_t full_steps = 0, shared_steps = 0;
  for (int input_seed = 0; input_seed < 2; ++input_seed) {
    const CandyCase input = make_case(input_seed);
    CandyRolloutProblem problem{input};
    Board board{};
    for (int turn = 0; turn < CELL_COUNT; ++turn) {
      place_by_rank(board, input.placement_rank[turn], input.flavor[turn]);
      const CandyRolloutProblem::State state{board, turn};
      std::vector<Board> initial;
      for (int action : problem.actions) initial.push_back(tilt_board(board, action));
      for (int sample = 0; sample < 3; ++sample) {
        const auto scenario = problem.generate_scenario(state, engine);
        const auto advance = [&](Board& future, int step) {
          const int future_turn = turn + step + 1;
          place_by_rank(future, scenario.rank[step], input.flavor[future_turn]);
          if (future_turn + 1 < CELL_COUNT) {
            future = tilt_board(future, rule_direction(input.flavor[future_turn], input.flavor[future_turn + 1]));
          }
        };
        const auto& scores = shared.evaluate(initial, scenario.length, advance, component_square_sum);
        for (int action : problem.actions) assert(scores[action] == problem.evaluate_action(state, action, scenario));
        full_steps += 4 * scenario.length;
        shared_steps += shared.last_transitions();
      }
      board = tilt_board(board, static_cast<int>(engine() % 4));
    }
  }
  assert(shared_steps < full_steps);
}
