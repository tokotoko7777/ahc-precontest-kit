#include <algorithm>
#include <cassert>
#include <cstdint>
#include <random>
#include <vector>

struct Random {
  std::mt19937_64 engine;

  explicit Random(std::uint64_t seed = 0) : engine(seed) {}

  std::uint64_t next_u64() { return engine(); }

  // [left, right) の整数を返す。
  int next_int(int left, int right) {
    assert(left < right);
    std::uniform_int_distribution<int> distribution(left, right - 1);
    return distribution(engine);
  }

  // [0, 1) の小数を返す。
  double next_double() {
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(engine);
  }

  template <class T>
  void shuffle(std::vector<T>& values) {
    std::shuffle(values.begin(), values.end(), engine);
  }
};
