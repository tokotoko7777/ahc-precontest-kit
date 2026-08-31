#include <bits/stdc++.h>
using namespace std;

#include "../../library/time-based-simulated-annealing.hpp"

// AtCoder Heuristic Contest 006 A - Food Delivery
// https://atcoder.jp/contests/ahc006/tasks/ahc006_a
//
// ローカルでは上の include でライブラリを試せる。
// AtCoderへ提出するときは、includeの行をheaderの中身で置き換えればよい。

constexpr int ORDER_COUNT = 1000;
constexpr int CHOSEN_COUNT = 50;
constexpr int DEPOT_EVENT = 2 * ORDER_COUNT;

#ifndef SEARCH_EXAMPLE_TIME_LIMIT_MS
#define SEARCH_EXAMPLE_TIME_LIMIT_MS 1850.0
#endif

struct Order {
  int pickup_x;
  int pickup_y;
  int delivery_x;
  int delivery_y;
};

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  array<Order, ORDER_COUNT> orders;
  for (Order& order : orders) {
    if (!(cin >> order.pickup_x >> order.pickup_y
              >> order.delivery_x >> order.delivery_y)) {
      return 0;
    }
  }

  // event = 2 * 注文番号     : pickup
  // event = 2 * 注文番号 + 1 : delivery
  // event = DEPOT_EVENT      : 事務所 (400, 400)
  auto point = [&](int event) -> pair<int, int> {
    if (event == DEPOT_EVENT) return {400, 400};
    const Order& order = orders[event / 2];
    if (event % 2 == 0) return {order.pickup_x, order.pickup_y};
    return {order.delivery_x, order.delivery_y};
  };

  auto distance = [&](int first, int second) {
    const auto [x1, y1] = point(first);
    const auto [x2, y2] = point(second);
    return abs(x1 - x2) + abs(y1 - y2);
  };

  auto route_cost = [&](const vector<int>& route) {
    int cost = 0;
    for (int i = 1; i < static_cast<int>(route.size()); ++i) {
      cost += distance(route[i - 1], route[i]);
    }
    return cost;
  };

  // route[position] の直前へeventを挿入したときの距離増加。
  auto insertion_delta = [&](const vector<int>& route,
                             int position,
                             int event) {
    return distance(route[position - 1], event) +
           distance(event, route[position]) -
           distance(route[position - 1], route[position]);
  };

  // pickupを先、deliveryを後に置ける全位置を試し、最良位置へ挿入する。
  auto insert_order_best = [&](vector<int>& route, int order_id) {
    const int pickup = 2 * order_id;
    const int delivery = pickup + 1;
    int best_delta = numeric_limits<int>::max();
    int best_pickup_position = -1;
    int best_delivery_position = -1;

    for (int pickup_position = 1;
         pickup_position < static_cast<int>(route.size());
         ++pickup_position) {
      vector<int> with_pickup = route;
      const int pickup_delta =
          insertion_delta(route, pickup_position, pickup);
      with_pickup.insert(with_pickup.begin() + pickup_position, pickup);

      for (int delivery_position = pickup_position + 1;
           delivery_position < static_cast<int>(with_pickup.size());
           ++delivery_position) {
        const int delta = pickup_delta +
            insertion_delta(with_pickup, delivery_position, delivery);
        if (delta < best_delta) {
          best_delta = delta;
          best_pickup_position = pickup_position;
          best_delivery_position = delivery_position;
        }
      }
    }

    route.insert(route.begin() + best_pickup_position, pickup);
    route.insert(route.begin() + best_delivery_position, delivery);
  };

  // 各注文を単独で運ぶ距離が短い順に50件選ぶ。
  vector<int> order_ids(ORDER_COUNT);
  iota(order_ids.begin(), order_ids.end(), 0);
  sort(order_ids.begin(), order_ids.end(), [&](int first, int second) {
    auto single_order_cost = [&](int order_id) {
      const int pickup = 2 * order_id;
      const int delivery = pickup + 1;
      return distance(DEPOT_EVENT, pickup) + distance(pickup, delivery) +
             distance(delivery, DEPOT_EVENT);
    };
    const int first_cost = single_order_cost(first);
    const int second_cost = single_order_cost(second);
    if (first_cost != second_cost) return first_cost < second_cost;
    return first < second;
  });

  array<bool, ORDER_COUNT> selected{};
  vector<int> current_route{DEPOT_EVENT, DEPOT_EVENT};
  for (int i = 0; i < CHOSEN_COUNT; ++i) {
    selected[order_ids[i]] = true;
    insert_order_best(current_route, order_ids[i]);
  }

  // pickupがdeliveryより前にあり、ちょうど50注文を1回ずつ運ぶか調べる。
  auto is_valid = [](const vector<int>& route) {
    if (route.size() != 2 * CHOSEN_COUNT + 2) return false;
    if (route.front() != DEPOT_EVENT || route.back() != DEPOT_EVENT) {
      return false;
    }

    array<bool, ORDER_COUNT> picked_up{};
    array<int, ORDER_COUNT> event_count{};
    for (int position = 1;
         position + 1 < static_cast<int>(route.size());
         ++position) {
      const int event = route[position];
      if (event < 0 || DEPOT_EVENT <= event) return false;
      const int order_id = event / 2;
      ++event_count[order_id];
      if (event % 2 == 0) {
        // 同じpickupが2回現れる候補も不正。
        if (picked_up[order_id]) return false;
        picked_up[order_id] = true;
      } else if (!picked_up[order_id]) {
        return false;
      }
    }

    int used_orders = 0;
    for (int count : event_count) {
      if (count == 0) continue;
      if (count != 2) return false;
      ++used_orders;
    }
    return used_orders == CHOSEN_COUNT;
  };

  assert(is_valid(current_route));
  int current_cost = route_cost(current_route);
  const int initial_cost = current_cost;
  int best_cost = current_cost;
  vector<int> best_route = current_route;

  mt19937_64 random(20211114);
  auto random_int = [&](int left, int right) {
    // [left, right) から一つ選ぶ。
    return uniform_int_distribution<int>(left, right - 1)(random);
  };

  // 最小化問題なので、acceptへは current_cost - new_cost を渡す。
  // 64回に一度だけ時計と温度を更新し、軽い近傍の速度低下を抑える。
  TimeBasedSimulatedAnnealing annealing(
      SEARCH_EXAMPLE_TIME_LIMIT_MS, 120.0, 1.0, 20211115, 64);

  while (!annealing.is_over()) {
    vector<int> candidate = current_route;
    int removed_order = -1;
    int inserted_order = -1;
    const int neighborhood = random_int(0, 100);

    if (neighborhood < 40) {
      // 近傍1: pickupまたはdeliveryを、先行制約を守れる位置へ移す。
      const int from = random_int(1, static_cast<int>(candidate.size()) - 1);
      const int event = candidate[from];
      candidate.erase(candidate.begin() + from);

      const int counterpart = event ^ 1;
      const int counterpart_position = static_cast<int>(
          find(candidate.begin(), candidate.end(), counterpart) -
          candidate.begin());

      int to;
      if (event % 2 == 0) {
        to = random_int(1, counterpart_position + 1);
      } else {
        to = random_int(
            counterpart_position + 1, static_cast<int>(candidate.size()));
      }
      candidate.insert(candidate.begin() + to, event);
    } else if (neighborhood < 70) {
      // 近傍2: 2イベントを交換。不正になった候補は後で捨てる。
      const int first =
          random_int(1, static_cast<int>(candidate.size()) - 1);
      const int second =
          random_int(1, static_cast<int>(candidate.size()) - 1);
      swap(candidate[first], candidate[second]);
    } else if (neighborhood < 95) {
      // 近傍3: 短い区間を反転。不正になった候補は後で捨てる。
      int left = random_int(1, static_cast<int>(candidate.size()) - 2);
      const int maximum_length = min(12,
          static_cast<int>(candidate.size()) - 2 - left);
      if (maximum_length <= 0) continue;
      const int right = left + random_int(1, maximum_length + 1);
      reverse(candidate.begin() + left, candidate.begin() + right + 1);
    } else {
      // 近傍4: 選択中の1注文を、未選択の1注文と入れ替える。
      const int position =
          random_int(1, static_cast<int>(candidate.size()) - 1);
      removed_order = candidate[position] / 2;
      do {
        inserted_order = random_int(0, ORDER_COUNT);
      } while (selected[inserted_order]);

      candidate.erase(
          remove(candidate.begin(), candidate.end(), 2 * removed_order),
          candidate.end());
      candidate.erase(
          remove(candidate.begin(), candidate.end(), 2 * removed_order + 1),
          candidate.end());
      insert_order_best(candidate, inserted_order);
    }

    if (!is_valid(candidate)) continue;

    const int candidate_cost = route_cost(candidate);
    const int improvement = current_cost - candidate_cost;
    if (!annealing.accept(improvement)) continue;

    current_route = move(candidate);
    current_cost = candidate_cost;
    if (removed_order != -1) {
      selected[removed_order] = false;
      selected[inserted_order] = true;
    }

    if (current_cost < best_cost) {
      best_cost = current_cost;
      best_route = current_route;
    }
  }

  assert(is_valid(best_route));
  assert(route_cost(best_route) == best_cost);

#ifdef SEARCH_EXAMPLE_PRINT_STATS
  cerr << "initial_cost=" << initial_cost
       << " best_cost=" << best_cost << '\n';
#else
  (void)initial_cost;
#endif

  // 1行目: 選んだ50注文。2行目: 事務所を含む102頂点の経路。
  vector<int> chosen_orders;
  array<bool, ORDER_COUNT> already_output{};
  for (int position = 1;
       position + 1 < static_cast<int>(best_route.size());
       ++position) {
    const int order_id = best_route[position] / 2;
    if (!already_output[order_id]) {
      already_output[order_id] = true;
      chosen_orders.push_back(order_id);
    }
  }

  cout << chosen_orders.size();
  for (int order_id : chosen_orders) cout << ' ' << order_id + 1;
  cout << '\n';

  cout << best_route.size();
  for (int event : best_route) {
    const auto [x, y] = point(event);
    cout << ' ' << x << ' ' << y;
  }
  cout << '\n';
}
