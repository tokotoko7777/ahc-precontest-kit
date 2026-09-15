#include <bits/stdc++.h>
using namespace std;

// 提出時は次の1行を、このhppの全文へ置き換える。
#include "../../library/time-based-simulated-annealing.hpp"

// Pre-contest public solver source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/examples/search/ahc006_sa.cpp
// Official problem: https://atcoder.jp/contests/ahc006/tasks/ahc006_a

// ============================================================================
// ここから問題ごとに書く部分。定数、入力型、Problem、出力を含む。
// ============================================================================
constexpr int ORDER_COUNT = 1000;
constexpr int CHOSEN_COUNT = 50;
constexpr int ROUTE_SIZE = 2 * CHOSEN_COUNT + 2;
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

struct DeliveryProblem {
  using Route = array<int, ROUTE_SIZE>;

  // TODO(AHC006): 現在解と、近傍評価に必要なcacheをStateへ置く。
  struct State {
    Route route{};
    array<bool, ORDER_COUNT> selected{};
    int cost = 0;
  };

  // TODO(AHC006): 近傍1回分を書く。
  // SAでは候補を同時に大量保持しないため、固定長配列を丸ごと持たせても
  // vectorの確保は起きない。まず安全な形で書き、必要なら局所deltaへ縮める。
  struct Move {
    Route next_route{};
    int next_cost = 0;
    int removed_order = -1;
    int inserted_order = -1;
  };

  // Runnerでは大きいほど良い値にするため、scoreは距離の符号を反転する。
  using Score = int;

  array<Order, ORDER_COUNT> orders{};

  void read_input() {
    for (Order& order : orders) {
      cin >> order.pickup_x >> order.pickup_y
          >> order.delivery_x >> order.delivery_y;
    }
  }

  pair<int, int> point(int event) const {
    if (event == DEPOT_EVENT) return {400, 400};
    const Order& order = orders[event / 2];
    if (event % 2 == 0) return {order.pickup_x, order.pickup_y};
    return {order.delivery_x, order.delivery_y};
  }

  int distance(int first, int second) const {
    const auto [x1, y1] = point(first);
    const auto [x2, y2] = point(second);
    return abs(x1 - x2) + abs(y1 - y2);
  }

  int route_cost(const Route& route) const {
    int cost = 0;
    for (int i = 1; i < ROUTE_SIZE; ++i) {
      cost += distance(route[i - 1], route[i]);
    }
    return cost;
  }

  int insertion_delta(
      const vector<int>& route, int position, int event) const {
    return distance(route[position - 1], event) +
           distance(event, route[position]) -
           distance(route[position - 1], route[position]);
  }

  // pickupを先、deliveryを後に置ける全位置から最良の組を選ぶ。
  void insert_order_best(vector<int>& route, int order_id) const {
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
  }

  bool is_valid(const Route& route) const {
    if (route.front() != DEPOT_EVENT || route.back() != DEPOT_EVENT) {
      return false;
    }
    bitset<ORDER_COUNT> picked_up;
    bitset<ORDER_COUNT> delivered;
    int used = 0;
    for (int position = 1; position + 1 < ROUTE_SIZE; ++position) {
      const int event = route[position];
      if (event < 0 || event >= DEPOT_EVENT) return false;
      const int order = event / 2;
      if (event % 2 == 0) {
        if (picked_up[order]) return false;
        picked_up[order] = true;
        ++used;
      } else {
        if (!picked_up[order] || delivered[order]) return false;
        delivered[order] = true;
      }
    }
    return used == CHOSEN_COUNT && picked_up == delivered;
  }

  // TODO(AHC006): 必ず合法な初期解を作る。
  State make_initial_state() const {
    vector<int> order_ids(ORDER_COUNT);
    iota(order_ids.begin(), order_ids.end(), 0);
    sort(order_ids.begin(), order_ids.end(), [&](int first, int second) {
      const auto single_cost = [&](int order_id) {
        const int pickup = 2 * order_id;
        const int delivery = pickup + 1;
        return distance(DEPOT_EVENT, pickup) + distance(pickup, delivery) +
               distance(delivery, DEPOT_EVENT);
      };
      const int left = single_cost(first);
      const int right = single_cost(second);
      return left != right ? left < right : first < second;
    });

    State state;
    vector<int> route{DEPOT_EVENT, DEPOT_EVENT};
    for (int i = 0; i < CHOSEN_COUNT; ++i) {
      state.selected[order_ids[i]] = true;
      insert_order_best(route, order_ids[i]);
    }
    copy(route.begin(), route.end(), state.route.begin());
    state.cost = route_cost(state.route);
    assert(is_valid(state.route));
    return state;
  }

  static int random_int(mt19937_64& engine, int left, int right) {
    return uniform_int_distribution<int>(left, right - 1)(engine);
  }

  // TODO(AHC006): pickup-before-deliveryを壊さない近傍を1個作る。
  optional<Move> propose_move(
      const State& state, mt19937_64& engine, double progress) const {
    Move move;
    move.next_route = state.route;
    const int neighborhood = random_int(engine, 0, 100);

    if (neighborhood < 40) {
      // 1イベントを、対応するイベントとの前後関係を守って移動する。
      const int from = random_int(engine, 1, ROUTE_SIZE - 1);
      const int event = move.next_route[from];
      for (int i = from; i + 1 < ROUTE_SIZE; ++i) {
        move.next_route[i] = move.next_route[i + 1];
      }
      int counterpart_position = 1;
      while (move.next_route[counterpart_position] != (event ^ 1)) {
        ++counterpart_position;
      }
      const int to = event % 2 == 0
                         ? random_int(engine, 1, counterpart_position + 1)
                         : random_int(
                               engine, counterpart_position + 1,
                               ROUTE_SIZE - 1);
      for (int i = ROUTE_SIZE - 1; i > to; --i) {
        move.next_route[i] = move.next_route[i - 1];
      }
      move.next_route[to] = event;
    } else if (neighborhood < 70) {
      const int first = random_int(engine, 1, ROUTE_SIZE - 1);
      const int second = random_int(engine, 1, ROUTE_SIZE - 1);
      if (first == second) return nullopt;
      swap(move.next_route[first], move.next_route[second]);
    } else if (neighborhood < 95) {
      const int left = random_int(engine, 1, ROUTE_SIZE - 2);
      // 前半は広め、終盤は狭い区間を試す。
      const int maximum_length =
          max(2, static_cast<int>(12.0 - 8.0 * progress));
      const int right_limit = min(ROUTE_SIZE - 2, left + maximum_length);
      if (left >= right_limit) return nullopt;
      const int right = random_int(engine, left + 1, right_limit + 1);
      reverse(move.next_route.begin() + left,
              move.next_route.begin() + right + 1);
    } else {
      // 選択注文を1件外し、未選択注文を最良の2位置へ挿入する。
      const int position = random_int(engine, 1, ROUTE_SIZE - 1);
      move.removed_order = state.route[position] / 2;
      do {
        move.inserted_order = random_int(engine, 0, ORDER_COUNT);
      } while (state.selected[move.inserted_order]);

      vector<int> route;
      route.reserve(ROUTE_SIZE);
      for (int event : state.route) {
        if (event != DEPOT_EVENT && event / 2 == move.removed_order) continue;
        route.push_back(event);
      }
      insert_order_best(route, move.inserted_order);
      copy(route.begin(), route.end(), move.next_route.begin());
    }

    if (!is_valid(move.next_route)) return nullopt;
    move.next_cost = route_cost(move.next_route);
    return move;
  }

  // TODO(AHC006): 正なら改善となる差分を返す。Stateは変更しない。
  optional<Score> evaluate_move(const State& state, const Move& move,
                                double /* threshold */) const {
    // TODO: 差分は既にO(1)で分かるので、閾値を使わず正確な改善量を返す。
    return state.cost - move.next_cost;
  }

  // TODO(AHC006): 採用された近傍だけをStateへ反映する。
  void apply_move(State& state, const Move& move) const {
    state.route = move.next_route;
    state.cost = move.next_cost;
    if (move.removed_order != -1) {
      state.selected[move.removed_order] = false;
      state.selected[move.inserted_order] = true;
    }
  }
};

void print_answer(
    const DeliveryProblem& problem, const DeliveryProblem::State& answer) {
  array<bool, ORDER_COUNT> already_output{};
  vector<int> chosen_orders;
  for (int position = 1; position + 1 < ROUTE_SIZE; ++position) {
    const int order = answer.route[position] / 2;
    if (!already_output[order]) {
      already_output[order] = true;
      chosen_orders.push_back(order);
    }
  }

  cout << chosen_orders.size();
  for (int order : chosen_orders) cout << ' ' << order + 1;
  cout << '\n';

  cout << ROUTE_SIZE;
  for (int event : answer.route) {
    const auto [x, y] = problem.point(event);
    cout << ' ' << x << ' ' << y;
  }
  cout << '\n';
}

// ============================================================================
// ここから下は探索の呼び出し。時計・温度・採否・best保存はRunnerが担当する。
// ============================================================================
int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  DeliveryProblem problem;
  problem.read_input();
  DeliveryProblem::State initial = problem.make_initial_state();

  TimeBasedAnnealingRunner<DeliveryProblem> runner(
      problem, initial, -initial.cost,
      SEARCH_EXAMPLE_TIME_LIMIT_MS,
      120.0, 1.0,  // TODO(AHC006): 開始温度、終了温度。
      20211115, 64);
  runner.run();

  const DeliveryProblem::State& answer = runner.best_state();
  assert(problem.is_valid(answer.route));
  assert(problem.route_cost(answer.route) == answer.cost);
  print_answer(problem, answer);
}
