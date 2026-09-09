// Introduction to Heuristics Contest A: AtCoder Contest Scheduling
// https://atcoder.jp/contests/intro-heuristics/tasks/intro_heuristics_a
//
// 「問題ごとに書く部分」と「汎用ビームサーチ」を分けた完全な解答例。
// リポジトリ内ではheaderをincludeする。提出時はheader全文をこの位置へ貼る。

#include <array>
#include <iostream>
#include <numeric>
#include <vector>

#include "../../library/action-beam-search.hpp"

// ========= ここだけ問題に合わせて書く =========

struct ContestSchedulingProblem {
  struct State {
    int day = 0;
    std::array<int, 26> last_day{};
    long long score = 0;
    std::array<unsigned char, 365> answer{};
  };

  using Action = int;
  using Score = long long;

  int days = 0;
  std::array<int, 26> decay{};
  std::vector<std::array<int, 26>> satisfaction;
  std::array<int, 26> contests{};

  bool read_input() {
    if (!(std::cin >> days)) return false;
    for (int& value : decay) std::cin >> value;
    satisfaction.resize(days);
    for (auto& row : satisfaction) {
      for (int& value : row) std::cin >> value;
    }
    std::iota(contests.begin(), contests.end(), 0);
    return true;
  }

  State initial_state() const { return State{}; }

  Score initial_score() const { return 0; }

  int max_turns() const { return days; }

  // 現在のStateから試すActionを返す。
  const std::array<int, 26>& generate_actions(const State&) const {
    return contests;
  }

  // Action適用後の順位を、Stateをコピーせずに計算する。
  Score evaluate_action(const State& state, const Action& contest) const {
    const int next_day = state.day + 1;
    Score next_score = state.score + satisfaction[state.day][contest];
    for (int type = 0; type < 26; ++type) {
      const int next_last = type == contest ? next_day
                                             : state.last_day[type];
      next_score -= static_cast<long long>(decay[type]) *
                    (next_day - next_last);
    }

    const int remaining = days - next_day;
    Score rank_score = next_score;
    for (int type = 0; type < 26; ++type) {
      const int next_last = type == contest ? next_day
                                             : state.last_day[type];
      rank_score += static_cast<long long>(decay[type]) *
                    next_last * remaining;
    }
    return rank_score;
  }

  // 選ばれたActionだけがここへ来る。Stateは既に親からコピー済み。
  void apply_action(State& state, Action& contest) const {
    state.answer[state.day] = static_cast<unsigned char>(contest);
    state.last_day[contest] = state.day + 1;
    state.score += satisfaction[state.day][contest];
    for (int type = 0; type < 26; ++type) {
      state.score -= static_cast<long long>(decay[type]) *
                     (state.day + 1 - state.last_day[type]);
    }
    ++state.day;
  }

  void print_answer(const State& answer) const {
    for (int day = 0; day < days; ++day) {
      std::cout << static_cast<int>(answer.answer[day]) + 1 << '\n';
    }
  }
};

// ========= 問題ごとに書く部分はここまで =========

int main() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  ContestSchedulingProblem problem;
  if (!problem.read_input()) return 0;

  constexpr int BEAM_WIDTH = 500;
  ActionBeamRunner<ContestSchedulingProblem> beam(
      problem,
      problem.initial_state(),
      problem.initial_score(),
      BEAM_WIDTH);
  beam.run(problem.max_turns());
  problem.print_answer(beam.best());
}
