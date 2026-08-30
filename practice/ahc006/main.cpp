#include <bits/stdc++.h>
using namespace std;

// library/batched-timer.hpp
struct BatchedTimer {
  double time_limit_ms;
  int check_interval;
  int calls_until_check = 0;
  bool over = false;
  double last_elapsed_ms = 0.0;
  chrono::steady_clock::time_point start;

  BatchedTimer(double time_limit_ms, int check_interval)
      : time_limit_ms(time_limit_ms),
        check_interval(check_interval),
        start(chrono::steady_clock::now()) {
    assert(time_limit_ms > 0.0);
    assert(check_interval > 0);
  }

  void reset() {
    calls_until_check = 0;
    over = false;
    last_elapsed_ms = 0.0;
    start = chrono::steady_clock::now();
  }

  double elapsed_ms() const {
    const auto now = chrono::steady_clock::now();
    return chrono::duration<double, milli>(now - start).count();
  }

  bool is_over() {
    if (over) return true;
    if (calls_until_check > 0) {
      --calls_until_check;
      return false;
    }
    calls_until_check = check_interval - 1;
    last_elapsed_ms = elapsed_ms();
    over = last_elapsed_ms >= time_limit_ms;
    return over;
  }

  double cached_progress() const {
    return clamp(last_elapsed_ms / time_limit_ms, 0.0, 1.0);
  }
};

// library/random.hpp
struct Random {
  mt19937_64 engine;

  explicit Random(uint64_t seed = 0) : engine(seed) {}

  uint64_t next_u64() { return engine(); }

  template <class Int>
  Int next_int(Int left, Int right) {
    assert(left < right);
    uniform_int_distribution<Int> distribution(left, right - 1);
    return distribution(engine);
  }

  template <class Real = double>
  Real next_real(Real left = Real(0), Real right = Real(1)) {
    assert(left < right);
    uniform_real_distribution<Real> distribution(left, right);
    return distribution(engine);
  }

  double next_double() { return next_real<double>(); }

  template <class T>
  void shuffle(vector<T>& values) {
    std::shuffle(values.begin(), values.end(), engine);
  }

  template <class T>
  T& choice(vector<T>& values) {
    assert(!values.empty());
    return values[next_int<size_t>(0, values.size())];
  }

  template <class T>
  const T& choice(const vector<T>& values) {
    assert(!values.empty());
    return values[next_int<size_t>(0, values.size())];
  }

  template <class Weight>
  int weighted_index(const vector<Weight>& weights) {
    assert(!weights.empty());
    long double total = 0.0L;
    for (const Weight& weight : weights) {
      assert(weight >= Weight(0));
      total += static_cast<long double>(weight);
    }
    assert(total > 0.0L);

    const long double target = next_real<long double>(0.0L, total);
    long double sum = 0.0L;
    for (int i = 0; i < static_cast<int>(weights.size()); ++i) {
      sum += static_cast<long double>(weights[i]);
      if (target < sum) return i;
    }
    return static_cast<int>(weights.size()) - 1;
  }
};

// library/simulated-annealing.hpp
struct SimulatedAnnealing {
  double start_temperature;
  double end_temperature;
  mt19937_64 engine;

  SimulatedAnnealing(
      double start_temperature,
      double end_temperature,
      uint64_t seed = 0)
      : start_temperature(start_temperature),
        end_temperature(end_temperature),
        engine(seed) {
    assert(start_temperature > 0.0);
    assert(end_temperature > 0.0);
  }

  double temperature(double progress) const {
    progress = clamp(progress, 0.0, 1.0);
    return start_temperature *
           pow(end_temperature / start_temperature, progress);
  }

  template <class Score>
  bool accept(Score improvement, double progress) {
    const double value = static_cast<double>(improvement);
    if (value >= 0.0) return true;
    const double probability = exp(value / temperature(progress));
    uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(engine) < probability;
  }
};

// library/best-keeper.hpp
template <class Score, class State>
struct BestKeeper {
  Score best_score;
  State best_state;
  bool maximize;

  BestKeeper(Score initial_score, State initial_state, bool maximize = true)
      : best_score(move(initial_score)),
        best_state(move(initial_state)),
        maximize(maximize) {}

  bool update(const Score& score, const State& state) {
    const bool better = maximize ? best_score < score : score < best_score;
    if (!better) return false;
    best_score = score;
    best_state = state;
    return true;
  }
};

// library/route-utils.hpp
template <class Route, class Distance>
auto route_length(const Route& route, Distance distance) {
  using Cost = decay_t<decltype(distance(route[0], route[0]))>;
  Cost total{};
  for (int i = 1; i < static_cast<int>(route.size()); ++i) {
    total += distance(route[i - 1], route[i]);
  }
  return total;
}

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

constexpr int ORDER_COUNT = 1000;
constexpr int CHOSEN_COUNT = 50;
constexpr int DEPOT = ORDER_COUNT * 2;

struct Order {
  int restaurant_x;
  int restaurant_y;
  int destination_x;
  int destination_y;
};

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  array<Order, ORDER_COUNT> orders;
  for (Order& order : orders) {
    cin >> order.restaurant_x >> order.restaurant_y
        >> order.destination_x >> order.destination_y;
  }

  auto point = [&](int event) {
    if (event == DEPOT) return pair{400, 400};
    const Order& order = orders[event / 2];
    if (event % 2 == 0) {
      return pair{order.restaurant_x, order.restaurant_y};
    }
    return pair{order.destination_x, order.destination_y};
  };

  auto distance = [&](int first, int second) {
    const auto [x1, y1] = point(first);
    const auto [x2, y2] = point(second);
    return abs(x1 - x2) + abs(y1 - y2);
  };

  // pickupを先、deliveryを後に置く最良の2位置へ注文を挿入する。
  auto insert_order_best = [&](vector<int>& route, int order) {
    const int pickup = 2 * order;
    const int delivery = pickup + 1;
    int best_delta = numeric_limits<int>::max();
    int best_pickup_position = -1;
    int best_delivery_position = -1;

    for (int pickup_position = 1;
         pickup_position < static_cast<int>(route.size());
         ++pickup_position) {
      vector<int> with_pickup = route;
      const int pickup_delta = route_insertion_delta(
          route, pickup_position, pickup, distance);
      with_pickup.insert(
          with_pickup.begin() + pickup_position, pickup);

      for (int delivery_position = pickup_position + 1;
           delivery_position < static_cast<int>(with_pickup.size());
           ++delivery_position) {
        const int delta = pickup_delta + route_insertion_delta(
            with_pickup, delivery_position, delivery, distance);
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

  // 単独で担当した時に短い注文を初期の50件にする。
  vector<int> order_ids(ORDER_COUNT);
  iota(order_ids.begin(), order_ids.end(), 0);
  sort(order_ids.begin(), order_ids.end(), [&](int a, int b) {
    const int cost_a = distance(DEPOT, 2 * a) +
                       distance(2 * a, 2 * a + 1) +
                       distance(2 * a + 1, DEPOT);
    const int cost_b = distance(DEPOT, 2 * b) +
                       distance(2 * b, 2 * b + 1) +
                       distance(2 * b + 1, DEPOT);
    return cost_a < cost_b;
  });

  array<bool, ORDER_COUNT> selected{};
  vector<int> current_route{DEPOT, DEPOT};
  for (int i = 0; i < CHOSEN_COUNT; ++i) {
    selected[order_ids[i]] = true;
    insert_order_best(current_route, order_ids[i]);
  }

  auto is_valid = [](const vector<int>& route) {
    if (route.size() != 2 * CHOSEN_COUNT + 2) return false;
    if (route.front() != DEPOT || route.back() != DEPOT) return false;

    array<int, ORDER_COUNT> pickup_position;
    pickup_position.fill(-1);
    array<int, ORDER_COUNT> event_count{};

    for (int position = 1;
         position + 1 < static_cast<int>(route.size());
         ++position) {
      const int event = route[position];
      if (event < 0 || DEPOT <= event) return false;
      const int order = event / 2;
      ++event_count[order];
      if (event % 2 == 0) {
        pickup_position[order] = position;
      } else if (pickup_position[order] == -1) {
        return false;
      }
    }

    int selected_count = 0;
    for (int count : event_count) {
      if (count == 0) continue;
      if (count != 2) return false;
      ++selected_count;
    }
    return selected_count == CHOSEN_COUNT;
  };

  assert(is_valid(current_route));
  int current_cost = route_length(current_route, distance);
  BestKeeper<int, vector<int>> best(current_cost, current_route, false);

  Random random(20211114);
  SimulatedAnnealing annealing(120.0, 1.0, 20211115);
  BatchedTimer timer(1850.0, 64);

  while (!timer.is_over()) {
    vector<int> candidate = current_route;
    int removed_order = -1;
    int inserted_order = -1;
    const int neighborhood = random.next_int(0, 100);

    if (neighborhood < 35) {
      // pickupまたはdeliveryを、先行制約を守れる別位置へ移す。
      const int from = random.next_int(
          1, static_cast<int>(candidate.size()) - 1);
      const int event = candidate[from];
      candidate.erase(candidate.begin() + from);
      const int counterpart = event ^ 1;
      const int counterpart_position = static_cast<int>(
          find(candidate.begin(), candidate.end(), counterpart) -
          candidate.begin());

      int to;
      if (event % 2 == 0) {
        to = random.next_int(1, counterpart_position + 1);
      } else {
        to = random.next_int(
            counterpart_position + 1,
            static_cast<int>(candidate.size()));
      }
      candidate.insert(candidate.begin() + to, event);
    } else if (neighborhood < 65) {
      // 2イベントを交換。壊れた先行制約は後で弾く。
      const int first = random.next_int(
          1, static_cast<int>(candidate.size()) - 1);
      const int second = random.next_int(
          1, static_cast<int>(candidate.size()) - 1);
      swap(candidate[first], candidate[second]);
    } else if (neighborhood < 90) {
      // 区間を反転。pickupとdeliveryの順が保たれた候補だけ使う。
      int left = random.next_int(
          1, static_cast<int>(candidate.size()) - 2);
      int right = random.next_int(
          left + 1, static_cast<int>(candidate.size()) - 1);
      reverse(candidate.begin() + left, candidate.begin() + right + 1);
    } else {
      // 選択中の1注文を外し、未選択注文を最良位置へ挿入する。
      const int position = random.next_int(
          1, static_cast<int>(candidate.size()) - 1);
      removed_order = candidate[position] / 2;
      do {
        inserted_order = random.next_int(0, ORDER_COUNT);
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

    const int candidate_cost = route_length(candidate, distance);
    const int improvement = current_cost - candidate_cost;
    if (!annealing.accept(improvement, timer.cached_progress())) continue;

    current_route = move(candidate);
    current_cost = candidate_cost;
    if (removed_order != -1) {
      selected[removed_order] = false;
      selected[inserted_order] = true;
    }
    best.update(current_cost, current_route);
  }

  const vector<int>& answer = best.best_state;
  assert(is_valid(answer));
  assert(route_length(answer, distance) == best.best_score);

  vector<int> chosen_orders;
  array<bool, ORDER_COUNT> already_output{};
  for (int position = 1;
       position + 1 < static_cast<int>(answer.size());
       ++position) {
    const int order = answer[position] / 2;
    if (!already_output[order]) {
      already_output[order] = true;
      chosen_orders.push_back(order);
    }
  }

  cout << chosen_orders.size();
  for (int order : chosen_orders) cout << ' ' << order + 1;
  cout << '\n';

  cout << answer.size();
  for (int event : answer) {
    const auto [x, y] = point(event);
    cout << ' ' << x << ' ' << y;
  }
  cout << '\n';
}
