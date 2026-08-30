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

// library/axis-aligned-rectangle.hpp
template <class Coordinate>
struct AxisAlignedRectangle {
  Coordinate left;
  Coordinate bottom;
  Coordinate right;
  Coordinate top;

  bool is_valid() const { return left < right && bottom < top; }

  Coordinate width() const {
    assert(is_valid());
    return right - left;
  }

  Coordinate height() const {
    assert(is_valid());
    return top - bottom;
  }

  long long area() const { return 1LL * width() * height(); }

  bool contains(Coordinate x, Coordinate y) const {
    return left <= x && x < right && bottom <= y && y < top;
  }

  bool overlaps(const AxisAlignedRectangle& other) const {
    return max(left, other.left) < min(right, other.right) &&
           max(bottom, other.bottom) < min(top, other.top);
  }
};

struct Request {
  int x;
  int y;
  long long desired_area;
};

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  int n;
  cin >> n;
  vector<Request> requests(n);
  vector<AxisAlignedRectangle<int>> rectangles(n);

  for (int i = 0; i < n; ++i) {
    cin >> requests[i].x >> requests[i].y >> requests[i].desired_area;
    rectangles[i] = {
        requests[i].x,
        requests[i].y,
        requests[i].x + 1,
        requests[i].y + 1};
  }

  auto satisfaction = [&](int index,
                          const AxisAlignedRectangle<int>& rectangle) {
    const double actual = static_cast<double>(rectangle.area());
    const double desired = static_cast<double>(requests[index].desired_area);
    const double ratio = min(actual, desired) / max(actual, desired);
    return 1.0 - (1.0 - ratio) * (1.0 - ratio);
  };

  auto can_place = [&](int index,
                       const AxisAlignedRectangle<int>& candidate) {
    if (!candidate.is_valid()) return false;
    if (candidate.left < 0 || candidate.bottom < 0 ||
        candidate.right > 10000 || candidate.top > 10000) {
      return false;
    }
    if (!candidate.contains(requests[index].x, requests[index].y)) {
      return false;
    }
    for (int other = 0; other < n; ++other) {
      if (other != index && candidate.overlaps(rectangles[other])) {
        return false;
      }
    }
    return true;
  };

  Random random(20210306);
  BatchedTimer timer(4700.0, 64);

  while (!timer.is_over()) {
    const int index = random.next_int(0, n);
    const auto current = rectangles[index];
    const long long current_area = current.area();
    const long long desired_area = requests[index].desired_area;
    if (current_area >= desired_area) continue;

    AxisAlignedRectangle<int> best_candidate = current;
    double best_gain = 0.0;
    const double current_satisfaction = satisfaction(index, current);
    const int first_direction = random.next_int(0, 4);

    for (int offset = 0; offset < 4; ++offset) {
      const int direction = (first_direction + offset) % 4;
      const int other_length =
          direction % 2 == 0 ? current.height() : current.width();
      const long long remaining = desired_area - current_area;
      const int floor_step =
          max(1LL, remaining / other_length);
      const int ceil_step =
          max(1LL, (remaining + other_length - 1) / other_length);

      array<int, 6> steps{
          1,
          min(4, ceil_step),
          min(16, ceil_step),
          min(64, ceil_step),
          floor_step,
          ceil_step};
      sort(steps.begin(), steps.end());

      for (int step_index = 0; step_index < 6; ++step_index) {
        if (step_index > 0 && steps[step_index] == steps[step_index - 1]) {
          continue;
        }

        AxisAlignedRectangle<int> candidate = current;
        const int step = steps[step_index];
        if (direction == 0) candidate.left -= step;
        if (direction == 1) candidate.bottom -= step;
        if (direction == 2) candidate.right += step;
        if (direction == 3) candidate.top += step;
        if (!can_place(index, candidate)) continue;

        const double gain =
            satisfaction(index, candidate) - current_satisfaction;
        if (gain > best_gain) {
          best_gain = gain;
          best_candidate = candidate;
        }
      }
    }

    if (best_gain > 0.0) rectangles[index] = best_candidate;
  }

  for (int i = 0; i < n; ++i) {
    assert(rectangles[i].is_valid());
    assert(rectangles[i].contains(requests[i].x, requests[i].y));
    for (int j = 0; j < i; ++j) {
      assert(!rectangles[i].overlaps(rectangles[j]));
    }
    cout << rectangles[i].left << ' ' << rectangles[i].bottom << ' '
         << rectangles[i].right << ' ' << rectangles[i].top << '\n';
  }
}
