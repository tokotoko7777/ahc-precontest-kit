#include <algorithm>
#include <cassert>
#include <cmath>
#include <string>
#include <vector>

#include "library/best-keeper.hpp"
#include "library/chmin-chmax.hpp"
#include "library/coordinate-compression.hpp"
#include "library/cumulative-sum-2d.hpp"
#include "library/cumulative-sum.hpp"
#include "library/dsu.hpp"
#include "library/random.hpp"
#include "library/rollback-array.hpp"
#include "library/rollback-dsu.hpp"
#include "library/schedule.hpp"
#include "library/simulated-annealing.hpp"
#include "library/stamp-array.hpp"
#include "library/time-based-simulated-annealing.hpp"
#include "library/timer.hpp"
#include "library/top-k.hpp"

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
    const long long large = random.next_int(0LL, 1LL << 40);
    assert(0 <= large && large < (1LL << 40));
    const double unit = random.next_real();
    assert(0.0 <= unit && unit < 1.0);
  }

  std::vector<int> values{0, 1, 2, 3, 4, 5, 6, 7};
  random.shuffle(values);
  std::sort(values.begin(), values.end());
  for (int i = 0; i < 8; ++i) assert(values[i] == i);
  assert(0 <= random.choice(values) && random.choice(values) < 8);
  assert(random.weighted_index(std::vector<int>{0, 0, 10}) == 2);

  Timer timer;
  assert(timer.elapsed_ms() >= 0.0);
  assert(0.0 <= timer.remaining_ms(1000.0));
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

  TimeBasedSimulatedAnnealing timed(1000.0, 100.0, 1.0, 4321);
  assert(!timed.is_over());
  assert(std::abs(timed.temperature(0.0) - 100.0) < 1e-12);
  assert(std::abs(timed.temperature(1.0) - 1.0) < 1e-12);
  assert(timed.accept(1LL));

  int small = 10;
  assert(chmin(small, 7));
  assert(small == 7);
  assert(!chmin(small, 8));
  assert(chmax(small, 12));
  assert(small == 12);

  assert(linear_schedule(100, 0, 0.25) == 75);
  assert(power_schedule(100, 0, 0.5, 2.0) == 75);
  assert(std::abs(geometric_schedule(100.0, 1.0, 0.5) - 10.0) <
         1e-12);

  BestKeeper<int, std::string> best(10, "ten");
  assert(!best.update(9, "nine"));
  assert(best.update(12, "twelve"));
  assert(best.best_score == 12 && best.best_state == "twelve");

  BestKeeper<double, int> minimum(10.0, 10, false);
  assert(minimum.update(5.0, 5));
  assert(!minimum.update(8.0, 8));

  TopK<int, std::string> top(2);
  top.add(10, "ten");
  top.add(30, "thirty");
  top.add(20, "twenty");
  assert(top.size() == 2);
  assert(top.best_score() == 30);
  assert(top.entries[1].first == 20);

  CumulativeSum<long long> cumulative({3, 1, 4, 1, 5});
  assert(cumulative.query(1, 4) == 6);

  CumulativeSum2D<int> cumulative_2d({{1, 2, 3}, {4, 5, 6}});
  assert(cumulative_2d.query(0, 1, 2, 3) == 16);

  std::vector<long long> coordinates{100, 20, 100, 50};
  CoordinateCompression<long long> compression(coordinates);
  assert(compression.size() == 3);
  assert(compression.index(50) == 1);
  assert(compression.value(2) == 100);
  assert(compression.compress(coordinates) ==
         std::vector<int>({2, 0, 2, 1}));

  Dsu dsu(5);
  assert(dsu.unite(0, 1));
  assert(dsu.unite(1, 2));
  assert(dsu.same(0, 2));
  assert(dsu.size(1) == 3);

  RollbackDsu rollback_dsu(5);
  rollback_dsu.unite(0, 1);
  const int dsu_snapshot = rollback_dsu.snapshot();
  rollback_dsu.unite(1, 2);
  assert(rollback_dsu.same(0, 2));
  rollback_dsu.rollback(dsu_snapshot);
  assert(!rollback_dsu.same(0, 2));

  RollbackArray<std::string> rollback_array({"a", "b", "c"});
  const int array_snapshot = rollback_array.snapshot();
  rollback_array.set(1, "changed");
  rollback_array.set(2, "also changed");
  assert(rollback_array[1] == "changed");
  rollback_array.rollback(array_snapshot);
  assert(rollback_array[1] == "b" && rollback_array[2] == "c");

  StampArray<int> stamped(5, -1);
  assert(stamped.get(2) == -1);
  stamped[2] = 100;
  assert(stamped.get(2) == 100);
  stamped.clear();
  assert(stamped.get(2) == -1);
}
