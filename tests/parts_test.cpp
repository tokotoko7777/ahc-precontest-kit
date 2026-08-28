#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

#include "library/random.hpp"
#include "library/simulated-annealing.hpp"
#include "library/timer.hpp"

int main() {
  Random first(42);
  Random second(42);
  for (int i = 0; i < 1000; ++i) {
    assert(first.next_u64() == second.next_u64());
  }

  Random random(123);
  for (int i = 0; i < 1000; ++i) {
    const int value = random.next_int(-7, 13);
    assert(-7 <= value && value < 13);
    const double unit = random.next_double();
    assert(0.0 <= unit && unit < 1.0);
  }

  std::vector<int> values{0, 1, 2, 3, 4, 5, 6, 7};
  random.shuffle(values);
  std::sort(values.begin(), values.end());
  for (int i = 0; i < 8; ++i) assert(values[i] == i);

  Timer timer;
  assert(timer.elapsed_ms() >= 0.0);
  assert(0.0 <= timer.progress(1000.0));
  assert(timer.progress(0.0) == 1.0);

  SimulatedAnnealing annealing(100.0, 1.0, 987654321);
  assert(std::abs(annealing.temperature(0.0) - 100.0) < 1e-12);
  assert(std::abs(annealing.temperature(1.0) - 1.0) < 1e-12);
  assert(annealing.accept(0.0, 0.5));
  assert(annealing.accept(1.0, 0.5));

  SimulatedAnnealing annealing_first(10.0, 0.1, 1234);
  SimulatedAnnealing annealing_second(10.0, 0.1, 1234);
  for (int i = 0; i < 1000; ++i) {
    assert(annealing_first.accept(-1.0, 0.5) ==
           annealing_second.accept(-1.0, 0.5));
  }
}
