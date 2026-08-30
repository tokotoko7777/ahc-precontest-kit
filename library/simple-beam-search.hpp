#include <algorithm>
#include <cassert>
#include <type_traits>
#include <utility>
#include <vector>

// 状態を丸ごとコピーする、分かりやすさ優先のビームサーチ。
// State の中に操作履歴も入れておくと、返り値から答えを復元できる。
//
// 使い方:
// auto answer = simple_beam_search(
//     initial_state, turns, 100,
//     [](const State& state) { return state.next_states(); },
//     [](const State& state) { return state.score; });
template <class State, class Expand, class Evaluate>
State simple_beam_search(
    State initial_state,
    int turns,
    int beam_width,
    Expand expand,
    Evaluate evaluate,
    bool maximize = true) {
  assert(turns >= 0);
  assert(beam_width > 0);

  using Score = std::decay_t<decltype(evaluate(initial_state))>;
  struct Candidate {
    Score score;
    State state;
    int order;
  };

  std::vector<State> beam;
  beam.push_back(std::move(initial_state));
  std::vector<Candidate> candidates;

  for (int turn = 0; turn < turns; ++turn) {
    candidates.clear();
    int order = 0;

    for (const State& state : beam) {
      auto next_states = expand(state);
      for (State& next_state : next_states) {
        const Score score = evaluate(next_state);
        candidates.push_back({score, std::move(next_state), order++});
      }
    }

    if (candidates.empty()) break;

    const auto is_better = [&](const Candidate& a, const Candidate& b) {
      if (maximize) {
        if (b.score < a.score) return true;
        if (a.score < b.score) return false;
      } else {
        if (a.score < b.score) return true;
        if (b.score < a.score) return false;
      }
      return a.order < b.order;
    };
    const int kept =
        std::min(beam_width, static_cast<int>(candidates.size()));
    std::partial_sort(candidates.begin(), candidates.begin() + kept,
                      candidates.end(), is_better);
    candidates.resize(kept);

    beam.clear();
    beam.reserve(candidates.size());
    for (Candidate& candidate : candidates) {
      beam.push_back(std::move(candidate.state));
    }
  }

  return std::move(beam.front());
}
