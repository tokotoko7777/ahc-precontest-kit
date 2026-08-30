#include <algorithm>
#include <cassert>
#include <functional>
#include <numeric>
#include <queue>
#include <utility>
#include <vector>

// 大きい値から順に、現在の合計が最小のグループへ入れる。
// 全要素が0以上の時に、グループ合計を手軽に揃えるためのgreedy。
//
// 使い方:
// vector<long long> weight = {9, 8, 7, 6, 5, 4};
// auto groups = greedy_balanced_partition(weight, 3);
// // groupsは要素番号の一覧。この例では各グループの合計が13になる。
//
// 計算量は要素数をN、グループ数をKとして O(N log N + N log K)。
template <class Weight>
std::vector<std::vector<int>> greedy_balanced_partition(
    const std::vector<Weight>& weight,
    int group_count) {
  assert(group_count > 0);

  std::vector<int> order(weight.size());
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&](int left, int right) {
    if (weight[left] != weight[right]) return weight[left] > weight[right];
    return left < right;
  });

  using Group = std::pair<Weight, int>;  // 合計、グループ番号
  std::priority_queue<Group, std::vector<Group>, std::greater<Group>> lightest;
  std::vector<std::vector<int>> groups(group_count);
  for (int group = 0; group < group_count; ++group) {
    lightest.push({Weight{}, group});
  }

  for (int item : order) {
    auto [sum, group] = lightest.top();
    lightest.pop();
    groups[group].push_back(item);
    sum += weight[item];
    lightest.push({sum, group});
  }
  return groups;
}
