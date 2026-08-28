#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>

#include "library/ahc/random.hpp"
#include "library/ahc/timer.hpp"

int main() {
  ahc::Random first(42);
  ahc::Random second(42);
  for (int i = 0; i < 1000; ++i) {
    assert(first.next_u64() == second.next_u64());
  }

  ahc::Random random(123);
  for (int i = 0; i < 1000; ++i) {
    const int value = random.uniform_int(-7, 13);
    assert(-7 <= value && value < 13);
    const double unit = random.uniform01();
    assert(0.0 <= unit && unit < 1.0);
  }

  std::array<int, 8> values{0, 1, 2, 3, 4, 5, 6, 7};
  random.shuffle(values.begin(), values.end());
  std::sort(values.begin(), values.end());
  for (int i = 0; i < 8; ++i) assert(values[i] == i);

  ahc::Timer timer;
  assert(timer.elapsed_ms() >= 0.0);
  assert(0.0 <= timer.progress(1000.0));
  assert(timer.progress(0.0) == 1.0);
}

