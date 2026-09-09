// Introduction to Heuristics Contest A: AtCoder Contest Scheduling
// https://atcoder.jp/contests/intro-heuristics/tasks/intro_heuristics_a
//
// 全26候補のStateを作らず、contest番号と差分評価だけを先に選ぶ例。
// リポジトリ内ではheaderをincludeする。提出時はheader全文をこの位置へ貼る。

#include <array>
#include <iostream>
#include <numeric>
#include <vector>

#include "../../library/action-beam-search.hpp"

struct ScheduleState {
  int day = 0;
  std::array<int, 26> last_day{};
  long long score = 0;
  std::array<unsigned char, 365> answer{};
};

int main() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  int days;
  if (!(std::cin >> days)) return 0;

  std::array<int, 26> decay{};
  for (int& value : decay) std::cin >> value;

  std::vector<std::array<int, 26>> satisfaction(days);
  for (auto& row : satisfaction) {
    for (int& value : row) std::cin >> value;
  }

  std::array<int, 26> contests{};
  std::iota(contests.begin(), contests.end(), 0);

  constexpr int BEAM_WIDTH = 500;
  ActionBeamSearch<ScheduleState, int, long long> beam(
      ScheduleState{}, 0, BEAM_WIDTH);

  for (int day = 0; day < days; ++day) {
    const bool advanced = beam.step(
        [&](const ScheduleState&) -> const std::array<int, 26>& {
          return contests;
        },
        [&](const ScheduleState& state, const int& contest) {
          // action適用後のrank_scoreを、Stateをコピーせず計算する。
          long long next_score = state.score + satisfaction[day][contest];
          for (int type = 0; type < 26; ++type) {
            const int next_last = type == contest ? day + 1
                                                   : state.last_day[type];
            next_score -= static_cast<long long>(decay[type]) *
                          (day + 1 - next_last);
          }

          const int remaining = days - (day + 1);
          long long rank_score = next_score;
          for (int type = 0; type < 26; ++type) {
            const int next_last = type == contest ? day + 1
                                                   : state.last_day[type];
            rank_score += static_cast<long long>(decay[type]) *
                          next_last * remaining;
          }
          return rank_score;
        },
        [&](ScheduleState& state, int& contest) {
          state.answer[day] = static_cast<unsigned char>(contest);
          state.last_day[contest] = day + 1;
          state.score += satisfaction[day][contest];
          for (int type = 0; type < 26; ++type) {
            state.score -= static_cast<long long>(decay[type]) *
                           (day + 1 - state.last_day[type]);
          }
          state.day = day + 1;
        });
    if (!advanced) break;
  }

  const ScheduleState& answer = beam.best();
  for (int day = 0; day < days; ++day) {
    std::cout << static_cast<int>(answer.answer[day]) + 1 << '\n';
  }
}
