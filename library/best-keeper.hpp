#include <utility>

// 今までで一番良い解を保存する。
// 使い方:
// BestKeeper<long long, vector<int>> best(score, answer);       // 最大化
// BestKeeper<double, vector<int>> best(cost, answer, false);    // 最小化
template <class Score, class State>
struct BestKeeper {
  Score best_score;
  State best_state;
  bool maximize;

  BestKeeper(Score initial_score, State initial_state, bool maximize = true)
      : best_score(std::move(initial_score)),
        best_state(std::move(initial_state)),
        maximize(maximize) {}

  bool update(const Score& score, const State& state) {
    const bool better = maximize ? best_score < score : score < best_score;
    if (!better) return false;
    best_score = score;
    best_state = state;
    return true;
  }
};
