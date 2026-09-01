#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

#include "library/simulated-annealing.hpp"
#include "library/time-based-simulated-annealing.hpp"

bool almost_equal(double left, double right, double tolerance = 1e-12) {
  const double scale = std::max({1.0, std::abs(left), std::abs(right)});
  return std::abs(left - right) <= tolerance * scale;
}

template <class Function>
void expect_invalid_argument(Function function) {
  bool thrown = false;
  try {
    function();
  } catch (const std::invalid_argument&) {
    thrown = true;
  }
  assert(thrown);
}

double reference_random_01(std::mt19937_64& engine) {
  constexpr double inverse = 1.0 / 9007199254740992.0;
  return static_cast<double>(engine() >> 11) * inverse;
}

bool reference_accept(
    std::mt19937_64& engine,
    double improvement,
    double inverse_temperature) {
  if (std::isnan(improvement)) return false;
  if (improvement >= 0.0) return true;
  const double probability = std::exp(improvement * inverse_temperature);
  return reference_random_01(engine) < probability;
}

void test_progress_based_temperature_settings() {
  SimulatedAnnealing annealing(100.0, 1.0, 1);
  assert(almost_equal(annealing.temperature(0.5), 10.0));

  // public fieldを直接変えても、同じ進捗率でキャッシュが更新される。
  annealing.set_progress(0.5);
  annealing.start_temperature = 400.0;
  annealing.end_temperature = 4.0;
  annealing.set_progress(0.5);
  assert(almost_equal(annealing.current_temperature(), 40.0));

  annealing.start_temperature = 900.0;
  annealing.end_temperature = 9.0;
  const SimulatedAnnealing& const_annealing = annealing;
  assert(almost_equal(const_annealing.temperature(0.5), 90.0));
  assert(almost_equal(const_annealing.current_temperature(), 90.0));

  annealing.set_temperatures(100.0, 1.0);
  annealing.use_linear_schedule();
  assert(almost_equal(annealing.temperature(0.5), 50.5));
  assert(almost_equal(annealing.current_temperature(), 50.5));

  annealing.set_cooling_power(2.0);
  assert(almost_equal(annealing.temperature(0.5), 75.25));
  assert(almost_equal(annealing.current_temperature(), 75.25));
  assert(almost_equal(annealing.temperature(-1.0), 100.0));
  assert(almost_equal(annealing.temperature(2.0), 1.0));

  annealing.use_geometric_schedule();
  assert(almost_equal(annealing.temperature(0.5), std::sqrt(1000.0)));
  annealing.set_cooling_power(1.0);
  assert(almost_equal(annealing.temperature(0.5), 10.0));

  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  expect_invalid_argument([&] { (void)annealing.temperature(nan); });
  expect_invalid_argument([&] { annealing.set_progress(nan); });
  expect_invalid_argument([&] {
    (void)annealing.acceptance_probability(nan, nan);
  });
  expect_invalid_argument([&] { annealing.set_cooling_power(0.0); });
  expect_invalid_argument([&] { annealing.set_cooling_power(-1.0); });
  expect_invalid_argument([&] { annealing.set_cooling_power(infinity); });
  expect_invalid_argument([&] { annealing.set_cooling_power(nan); });

  annealing.start_temperature = 0.0;
  expect_invalid_argument([&] { (void)const_annealing.temperature(0.5); });
  annealing.set_temperatures(100.0, 1.0);

  const double smallest = std::numeric_limits<double>::denorm_min();
  SimulatedAnnealing tiny_linear(smallest, smallest, 9);
  tiny_linear.use_linear_schedule();
  assert(tiny_linear.temperature(0.5) == smallest);
  assert(almost_equal(
      tiny_linear.acceptance_probability(-smallest), std::exp(-1.0)));
  assert(almost_equal(
      tiny_linear.acceptance_probability(-smallest, 0.5), std::exp(-1.0)));
  std::mt19937_64 tiny_reference(9);
  const bool tiny_expected =
      reference_random_01(tiny_reference) < std::exp(-1.0);
  assert(tiny_linear.accept(-smallest) == tiny_expected);
  assert(tiny_linear.engine == tiny_reference);
}

void test_probability_helpers() {
  SimulatedAnnealing annealing(10.0, 10.0, 2);
  assert(almost_equal(
      annealing.acceptance_probability(-10.0), std::exp(-1.0)));
  assert(annealing.acceptance_probability(0.0) == 1.0);
  assert(annealing.acceptance_probability(1.0) == 1.0);
  assert(annealing.acceptance_probability(
      std::numeric_limits<double>::quiet_NaN()) == 0.0);

  SimulatedAnnealing scheduled(100.0, 1.0, 3);
  assert(almost_equal(
      scheduled.acceptance_probability(-10.0, 0.5), std::exp(-1.0)));

  const double desired_probability = 0.8;
  const double temperature = SimulatedAnnealing::temperature_for_acceptance(
      7.0, desired_probability);
  assert(almost_equal(std::exp(-7.0 / temperature), desired_probability));
  const double timed_temperature =
      TimeBasedSimulatedAnnealing::temperature_for_acceptance(
          7.0, desired_probability);
  assert(almost_equal(timed_temperature, temperature));

  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  expect_invalid_argument([&] {
    (void)SimulatedAnnealing::temperature_for_acceptance(0.0, 0.5);
  });
  expect_invalid_argument([&] {
    (void)SimulatedAnnealing::temperature_for_acceptance(-1.0, 0.5);
  });
  expect_invalid_argument([&] {
    (void)SimulatedAnnealing::temperature_for_acceptance(nan, 0.5);
  });
  expect_invalid_argument([&] {
    (void)SimulatedAnnealing::temperature_for_acceptance(infinity, 0.5);
  });
  expect_invalid_argument([&] {
    (void)SimulatedAnnealing::temperature_for_acceptance(1.0, 0.0);
  });
  expect_invalid_argument([&] {
    (void)SimulatedAnnealing::temperature_for_acceptance(1.0, 1.0);
  });
  expect_invalid_argument([&] {
    (void)SimulatedAnnealing::temperature_for_acceptance(1.0, nan);
  });
  expect_invalid_argument([&] {
    (void)TimeBasedSimulatedAnnealing::temperature_for_acceptance(0.0, 0.5);
  });
  expect_invalid_argument([&] {
    (void)TimeBasedSimulatedAnnealing::temperature_for_acceptance(-1.0, 0.5);
  });
  expect_invalid_argument([&] {
    (void)TimeBasedSimulatedAnnealing::temperature_for_acceptance(nan, 0.5);
  });
  expect_invalid_argument([&] {
    (void)TimeBasedSimulatedAnnealing::temperature_for_acceptance(
        infinity, 0.5);
  });
  expect_invalid_argument([&] {
    (void)TimeBasedSimulatedAnnealing::temperature_for_acceptance(1.0, 0.0);
  });
  expect_invalid_argument([&] {
    (void)TimeBasedSimulatedAnnealing::temperature_for_acceptance(1.0, 1.0);
  });
  expect_invalid_argument([&] {
    (void)TimeBasedSimulatedAnnealing::temperature_for_acceptance(1.0, nan);
  });
}

void test_progress_based_decision_compatibility() {
  constexpr std::uint64_t seed = 123456789;
  SimulatedAnnealing annealing(1.0, 1.0, seed);
  std::mt19937_64 reference_engine(seed);
  const std::vector<double> improvements = {
      1.0,
      0.0,
      -0.0,
      -0.1,
      -1.0,
      -36.0,
      -37.0,
      -100.0,
      -1000.0,
      -std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::quiet_NaN(),
      -5.0,
  };

  for (int repeat = 0; repeat < 100; ++repeat) {
    for (double improvement : improvements) {
      const bool expected = reference_accept(reference_engine, improvement, 1.0);
      assert(annealing.accept(improvement) == expected);
      assert(annealing.engine == reference_engine);
    }
  }
}

void test_time_based_temperature_settings() {
  TimeBasedSimulatedAnnealing annealing(
      100000.0, 100.0, 1.0, 4, 8);
  assert(almost_equal(annealing.temperature(0.5), 10.0));

  // 時刻に依存させず、同じcached progressでの同期を確認する。
  annealing.cached_progress_value = 0.5;
  annealing.set_temperatures(100.0, 1.0);
  assert(almost_equal(annealing.cached_temperature(), 10.0));
  annealing.start_temperature = 400.0;
  annealing.end_temperature = 4.0;
  const TimeBasedSimulatedAnnealing& const_annealing = annealing;
  assert(almost_equal(const_annealing.cached_temperature(), 40.0));
  assert(almost_equal(const_annealing.temperature(0.5), 40.0));

  annealing.set_temperatures(100.0, 1.0);
  annealing.use_linear_schedule();
  assert(almost_equal(annealing.temperature(0.5), 50.5));
  assert(almost_equal(annealing.cached_temperature(), 50.5));
  annealing.set_cooling_power(2.0);
  assert(almost_equal(annealing.temperature(0.5), 75.25));
  assert(almost_equal(annealing.cached_temperature(), 75.25));
  annealing.use_geometric_schedule();
  assert(almost_equal(annealing.temperature(0.5), std::sqrt(1000.0)));
  annealing.set_cooling_power(1.0);
  assert(almost_equal(annealing.temperature(0.5), 10.0));

  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  expect_invalid_argument([&] { (void)annealing.temperature(nan); });
  expect_invalid_argument([&] { annealing.set_cooling_power(0.0); });
  expect_invalid_argument([&] { annealing.set_cooling_power(infinity); });
  expect_invalid_argument([&] { annealing.set_cooling_power(nan); });

  annealing.end_temperature = 0.0;
  expect_invalid_argument([&] { (void)const_annealing.cached_temperature(); });
  annealing.set_temperatures(100.0, 1.0);

  const double smallest = std::numeric_limits<double>::denorm_min();
  TimeBasedSimulatedAnnealing tiny_linear(
      100000.0, smallest, smallest, 10, 1);
  tiny_linear.use_linear_schedule();
  assert(tiny_linear.temperature(0.5) == smallest);
  assert(almost_equal(
      tiny_linear.acceptance_probability(-smallest), std::exp(-1.0)));
  std::mt19937_64 tiny_reference(10);
  const bool tiny_expected =
      reference_random_01(tiny_reference) < std::exp(-1.0);
  assert(tiny_linear.accept(-smallest) == tiny_expected);
  assert(tiny_linear.engine == tiny_reference);
}

void test_time_based_probability_and_decisions() {
  constexpr std::uint64_t seed = 987654321;
  TimeBasedSimulatedAnnealing annealing(
      100000.0, 1.0, 1.0, seed, 64);
  assert(almost_equal(
      annealing.acceptance_probability(-1.0), std::exp(-1.0)));
  assert(annealing.acceptance_probability(0.0) == 1.0);
  assert(annealing.acceptance_probability(
      std::numeric_limits<double>::quiet_NaN()) == 0.0);

  std::mt19937_64 reference_engine(seed);
  const std::vector<double> improvements = {
      1.0,
      0.0,
      -0.1,
      -1.0,
      -36.0,
      -37.0,
      -100.0,
      -1000.0,
      -std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::quiet_NaN(),
      -5.0,
  };
  for (int repeat = 0; repeat < 100; ++repeat) {
    for (double improvement : improvements) {
      const bool expected = reference_accept(reference_engine, improvement, 1.0);
      assert(annealing.accept(improvement) == expected);
      assert(annealing.engine == reference_engine);
    }
  }
}

void test_time_checks() {
  using namespace std::chrono;
  TimeBasedSimulatedAnnealing annealing(
      100000.0, 100.0, 1.0, 5, 4);
  assert(!annealing.is_over());
  annealing.start -= seconds(200);

  // 最初の時計確認後は、指定回数まで古い判定を使う。
  for (int i = 0; i < 3; ++i) assert(!annealing.is_over());
  assert(annealing.is_over());
  assert(annealing.cached_progress() == 1.0);
  assert(almost_equal(annealing.cached_temperature(), 1.0));

  annealing.reset();
  assert(!annealing.is_over_now());
  annealing.start -= seconds(200);
  assert(annealing.is_over_now());

  annealing.reset();
  annealing.set_check_interval(2);
  assert(!annealing.is_over());
  annealing.start -= seconds(200);
  assert(!annealing.is_over());
  assert(annealing.is_over());

  expect_invalid_argument([&] { annealing.set_check_interval(0); });
  expect_invalid_argument([&] { annealing.set_check_interval(-1); });
}

int main() {
  test_progress_based_temperature_settings();
  test_probability_helpers();
  test_progress_based_decision_compatibility();
  test_time_based_temperature_settings();
  test_time_based_probability_and_decisions();
  test_time_checks();
}
