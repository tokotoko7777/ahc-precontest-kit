#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>

// 良い候補を上位 K 個だけ残す。候補数が少ない場面向けの簡単な実装。
// 使い方:
// TopK<double, vector<int>> candidates(20);        // 大きいほど良い
// TopK<double, vector<int>> candidates(20, false); // 小さいほど良い
// candidates.add(score, state);
template <class Score, class State>
struct TopK {
  int limit;
  bool maximize;
  std::vector<std::pair<Score, State>> entries;

  explicit TopK(int limit, bool maximize = true)
      : limit(limit), maximize(maximize) {
    assert(limit > 0);
    entries.reserve(limit + 1);
  }

  void add(Score score, State state) {
    entries.emplace_back(std::move(score), std::move(state));
    std::stable_sort(entries.begin(), entries.end(), [&](const auto& a,
                                                         const auto& b) {
      return maximize ? b.first < a.first : a.first < b.first;
    });
    if (static_cast<int>(entries.size()) > limit) entries.pop_back();
  }

  int size() const { return static_cast<int>(entries.size()); }

  const Score& best_score() const {
    assert(!entries.empty());
    return entries.front().first;
  }

  const State& best_state() const {
    assert(!entries.empty());
    return entries.front().second;
  }
};
