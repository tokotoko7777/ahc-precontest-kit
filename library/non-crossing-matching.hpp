#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>

template <class Score>
struct NonCrossingMatchingResult {
  Score score;
  std::vector<std::pair<int, int>> pairs;
};

// 左番号も右番号も狭義単調増加になる組を選び、重みの合計を最大化する。
// 同じ左・右は高々1回しか使わず、線で結んだ時に組同士が交差しない。
// 計算量・メモリ量は O(L * R)。重みが負なら、その組を選ばなくてもよい。
//
// 使い方:
// vector<vector<long long>> weight(left_size,
//                                  vector<long long>(right_size));
// vector<vector<char>> allowed(left_size, vector<char>(right_size, true));
// allowed[a][b] = false;  // この組は選べない
// auto result = maximum_weight_non_crossing_matching(weight, allowed);
// for (auto [left, right] : result.pairs) { ... }
template <class Score>
NonCrossingMatchingResult<Score> maximum_weight_non_crossing_matching(
    const std::vector<std::vector<Score>>& weight,
    const std::vector<std::vector<char>>& allowed) {
  const int left_size = static_cast<int>(weight.size());
  assert(static_cast<int>(allowed.size()) == left_size);
  const int right_size =
      left_size == 0 ? 0 : static_cast<int>(weight[0].size());
  for (int left = 0; left < left_size; ++left) {
    assert(static_cast<int>(weight[left].size()) == right_size);
    assert(static_cast<int>(allowed[left].size()) == right_size);
  }

  std::vector<std::vector<Score>> best(
      left_size + 1, std::vector<Score>(right_size + 1, Score{}));
  // 1: leftを使わない、2: rightを使わない、3: この組を選ぶ。
  std::vector<std::vector<char>> choice(
      left_size + 1, std::vector<char>(right_size + 1, 0));

  for (int left = 1; left <= left_size; ++left) {
    for (int right = 1; right <= right_size; ++right) {
      best[left][right] = best[left - 1][right];
      choice[left][right] = 1;
      if (best[left][right] < best[left][right - 1]) {
        best[left][right] = best[left][right - 1];
        choice[left][right] = 2;
      }
      if (allowed[left - 1][right - 1]) {
        const Score with_pair =
            best[left - 1][right - 1] + weight[left - 1][right - 1];
        if (best[left][right] < with_pair) {
          best[left][right] = with_pair;
          choice[left][right] = 3;
        }
      }
    }
  }

  std::vector<std::pair<int, int>> pairs;
  int left = left_size;
  int right = right_size;
  while (left > 0 && right > 0) {
    if (choice[left][right] == 1) {
      --left;
    } else if (choice[left][right] == 2) {
      --right;
    } else {
      assert(choice[left][right] == 3);
      pairs.push_back({left - 1, right - 1});
      --left;
      --right;
    }
  }
  std::reverse(pairs.begin(), pairs.end());
  return {best[left_size][right_size], std::move(pairs)};
}

// すべての組を選択可能にする短い版。
template <class Score>
NonCrossingMatchingResult<Score> maximum_weight_non_crossing_matching(
    const std::vector<std::vector<Score>>& weight) {
  const int left_size = static_cast<int>(weight.size());
  const int right_size =
      left_size == 0 ? 0 : static_cast<int>(weight[0].size());
  const std::vector<std::vector<char>> allowed(
      left_size, std::vector<char>(right_size, true));
  return maximum_weight_non_crossing_matching(weight, allowed);
}
