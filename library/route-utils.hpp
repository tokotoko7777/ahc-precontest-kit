#include <cassert>
#include <type_traits>

// 経路の長さと、挿入・削除・区間反転の差分を計算する。
// Route は vector<int>、vector<pair<int, int>> など自由に選べる。
// distance(a, b) は2点間の距離を返す関数にする。
//
// 使い方:
// auto distance = [](Point a, Point b) { ... };
// long long cost = route_length(route, distance);
// long long delta = route_insertion_delta(route, position, point, distance);

template <class Route, class Distance>
auto route_length(const Route& route, Distance distance) {
  using Cost = std::decay_t<decltype(distance(route[0], route[0]))>;
  Cost total{};
  for (int i = 1; i < static_cast<int>(route.size()); ++i) {
    total += distance(route[i - 1], route[i]);
  }
  return total;
}

// position の直前へ point を挿入した時の「新しい距離 - 古い距離」。
template <class Route, class Point, class Distance>
auto route_insertion_delta(
    const Route& route,
    int position,
    const Point& point,
    Distance distance) {
  assert(0 < position && position < static_cast<int>(route.size()));
  return distance(route[position - 1], point) +
         distance(point, route[position]) -
         distance(route[position - 1], route[position]);
}

// position の点を削除した時の「新しい距離 - 古い距離」。
template <class Route, class Distance>
auto route_removal_delta(
    const Route& route,
    int position,
    Distance distance) {
  assert(0 < position && position + 1 < static_cast<int>(route.size()));
  return distance(route[position - 1], route[position + 1]) -
         distance(route[position - 1], route[position]) -
         distance(route[position], route[position + 1]);
}

// [left, right] をreverseした時の「新しい距離 - 古い距離」。
// Manhattan距離やEuclid距離のように distance(a,b)==distance(b,a) の時だけ使える。
template <class Route, class Distance>
auto route_reverse_delta(
    const Route& route,
    int left,
    int right,
    Distance distance) {
  assert(0 < left && left <= right);
  assert(right + 1 < static_cast<int>(route.size()));
  return distance(route[left - 1], route[right]) +
         distance(route[left], route[right + 1]) -
         distance(route[left - 1], route[left]) -
         distance(route[right], route[right + 1]);
}
