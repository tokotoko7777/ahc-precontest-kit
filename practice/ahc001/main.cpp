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
};

// library/axis-aligned-rectangle.hpp
template <class Coordinate>
struct AxisAlignedRectangle {
  Coordinate left;
  Coordinate bottom;
  Coordinate right;
  Coordinate top;

  bool is_valid() const { return left < right && bottom < top; }
  Coordinate width() const { return right - left; }
  Coordinate height() const { return top - bottom; }
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

struct SplitCandidate {
  double error;
  int axis;
  int split_count;
  int cut;
};

double satisfaction(long long desired, long long actual) {
  const double ratio =
      static_cast<double>(min(desired, actual)) / max(desired, actual);
  return 1.0 - (1.0 - ratio) * (1.0 - ratio);
}

// A quick approximation used while comparing many recursive partitions.
long long quick_best_area(int width, int height, long long desired) {
  if (width > height) swap(width, height);
  long long best = 1;

  auto try_width = [&](int w) {
    if (w < 1 || w > width) return;
    const long long floor_height = min<long long>(height, desired / w);
    if (floor_height >= 1) {
      const long long area = 1LL * w * floor_height;
      if (abs(area - desired) < abs(best - desired)) best = area;
    }
    const long long ceil_height = min<long long>(height, (desired + w - 1) / w);
    if (ceil_height >= 1) {
      const long long area = 1LL * w * ceil_height;
      if (abs(area - desired) < abs(best - desired)) best = area;
    }
  };

  try_width(1);
  try_width(width);
  try_width(static_cast<int>(sqrt(static_cast<double>(desired))));
  try_width(static_cast<int>(desired / max(1, height)));
  try_width(static_cast<int>((desired + height - 1) / max(1, height)));
  for (int sample = 1; sample <= 24; ++sample) {
    try_width(1 + static_cast<long long>(width - 1) * sample / 24);
  }
  return best;
}

long long exact_best_area(int width, int height, long long desired,
                          int& best_width, int& best_height) {
  bool swapped = false;
  if (width > height) {
    swap(width, height);
    swapped = true;
  }

  long long best_area = 1;
  int answer_width = 1;
  int answer_height = 1;
  for (int w = 1; w <= width; ++w) {
    const long long floor_height = min<long long>(height, desired / w);
    const long long ceil_height = min<long long>(height, (desired + w - 1) / w);
    for (long long h : {floor_height, ceil_height}) {
      if (h < 1) continue;
      const long long area = 1LL * w * h;
      if (abs(area - desired) < abs(best_area - desired)) {
        best_area = area;
        answer_width = w;
        answer_height = static_cast<int>(h);
      }
    }
  }

  if (swapped) swap(answer_width, answer_height);
  best_width = answer_width;
  best_height = answer_height;
  return best_area;
}

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  int n;
  cin >> n;
  vector<Request> requests(n);
  uint64_t input_hash = 1469598103934665603ULL;
  for (auto& request : requests) {
    cin >> request.x >> request.y >> request.desired_area;
    input_hash ^= static_cast<uint64_t>(request.x + 10001 * request.y);
    input_hash *= 1099511628211ULL;
    input_hash ^= static_cast<uint64_t>(request.desired_area);
    input_hash *= 1099511628211ULL;
  }

  Random random(input_hash);
  BatchedTimer timer(4750.0, 1);
  vector<AxisAlignedRectangle<int>> best_regions(n);
  double best_proxy_score = -1.0;
  int builds = 0;

  while (!timer.is_over()) {
    vector<AxisAlignedRectangle<int>> regions(n);

    function<void(vector<int>&, AxisAlignedRectangle<int>)> divide =
        [&](vector<int>& indices, AxisAlignedRectangle<int> space) {
          if (indices.size() == 1) {
            regions[indices[0]] = space;
            return;
          }

          vector<int> by_x = indices;
          vector<int> by_y = indices;
          sort(by_x.begin(), by_x.end(), [&](int a, int b) {
            return requests[a].x < requests[b].x;
          });
          sort(by_y.begin(), by_y.end(), [&](int a, int b) {
            return requests[a].y < requests[b].y;
          });

          long long total_desired = 0;
          for (int index : indices) total_desired += requests[index].desired_area;

          vector<SplitCandidate> candidates;
          candidates.reserve(indices.size() * 2);
          for (int axis = 0; axis < 2; ++axis) {
            const vector<int>& order = axis == 0 ? by_x : by_y;
            const int low = axis == 0 ? space.left : space.bottom;
            const int high = axis == 0 ? space.right : space.top;
            const int other_length = axis == 0 ? space.height() : space.width();
            long long left_desired = 0;

            for (int count = 1; count < static_cast<int>(order.size()); ++count) {
              left_desired += requests[order[count - 1]].desired_area;
              const int previous_coordinate =
                  axis == 0 ? requests[order[count - 1]].x
                            : requests[order[count - 1]].y;
              const int next_coordinate =
                  axis == 0 ? requests[order[count]].x
                            : requests[order[count]].y;
              if (previous_coordinate == next_coordinate) continue;

              const int minimum_cut = previous_coordinate + 1;
              const int maximum_cut = next_coordinate;
              const long double fraction =
                  static_cast<long double>(left_desired) / total_desired;
              int cut = static_cast<int>(llround(low + (high - low) * fraction));
              cut = clamp(cut, minimum_cut, maximum_cut);

              if (builds > 0 && minimum_cut < maximum_cut &&
                  random.next_int(0, 4) == 0) {
                const int pull = random.next_int(0, 2) == 0
                                     ? minimum_cut
                                     : maximum_cut;
                cut = (3 * cut + pull) / 4;
                cut = clamp(cut, minimum_cut, maximum_cut);
              }

              const long long parent_area = space.area();
              const long long left_area = 1LL * (cut - low) * other_length;
              const long double target_area = parent_area * fraction;
              const double allocation_error =
                  abs(static_cast<long double>(left_area) - target_area) /
                  max<long double>(1.0L, parent_area);
              const double depth_error =
                  0.002 * abs(2 * count - static_cast<int>(order.size())) /
                  order.size();
              candidates.push_back(
                  {allocation_error + depth_error, axis, count, cut});
            }
          }

          assert(!candidates.empty());
          const int keep = min<int>(10, candidates.size());
          nth_element(candidates.begin(), candidates.begin() + keep - 1,
                      candidates.end(), [](const auto& a, const auto& b) {
                        return a.error < b.error;
                      });
          sort(candidates.begin(), candidates.begin() + keep,
               [](const auto& a, const auto& b) { return a.error < b.error; });

          int chosen_rank = 0;
          if (builds > 0) {
            const int roll = random.next_int(0, 100);
            if (roll >= 58) chosen_rank = min(1, keep - 1);
            if (roll >= 80) chosen_rank = min(2, keep - 1);
            if (roll >= 91) chosen_rank = min(4, keep - 1);
            if (roll >= 97) chosen_rank = random.next_int(0, keep);
          }
          const SplitCandidate chosen = candidates[chosen_rank];
          const vector<int>& order = chosen.axis == 0 ? by_x : by_y;
          vector<int> first(order.begin(), order.begin() + chosen.split_count);
          vector<int> second(order.begin() + chosen.split_count, order.end());

          auto first_space = space;
          auto second_space = space;
          if (chosen.axis == 0) {
            first_space.right = chosen.cut;
            second_space.left = chosen.cut;
          } else {
            first_space.top = chosen.cut;
            second_space.bottom = chosen.cut;
          }
          divide(first, first_space);
          divide(second, second_space);
        };

    vector<int> all(n);
    iota(all.begin(), all.end(), 0);
    divide(all, {0, 0, 10000, 10000});
    ++builds;

    double proxy_score = 0.0;
    for (int i = 0; i < n; ++i) {
      const long long area = quick_best_area(
          max(1, regions[i].width()), max(1, regions[i].height()),
          requests[i].desired_area);
      proxy_score += satisfaction(requests[i].desired_area, area);
    }
    if (proxy_score > best_proxy_score) {
      best_proxy_score = proxy_score;
      best_regions = regions;
    }
  }

  vector<AxisAlignedRectangle<int>> answer(n);
  for (int i = 0; i < n; ++i) {
    int width = 1;
    int height = 1;
    exact_best_area(best_regions[i].width(), best_regions[i].height(),
                    requests[i].desired_area, width, height);

    const int left = clamp(requests[i].x - width / 2,
                           best_regions[i].left,
                           best_regions[i].right - width);
    const int bottom = clamp(requests[i].y - height / 2,
                             best_regions[i].bottom,
                             best_regions[i].top - height);
    answer[i] = {left, bottom, left + width, bottom + height};
  }

  for (int i = 0; i < n; ++i) {
    assert(answer[i].is_valid());
    assert(answer[i].contains(requests[i].x, requests[i].y));
    for (int j = 0; j < i; ++j) assert(!answer[i].overlaps(answer[j]));
    cout << answer[i].left << ' ' << answer[i].bottom << ' '
         << answer[i].right << ' ' << answer[i].top << '\n';
  }
}
