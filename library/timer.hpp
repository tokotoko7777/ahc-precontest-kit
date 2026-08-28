// AHC LIBRARY: Timer
// Copy this whole file above main().

#ifndef AHC_PRECONTEST_KIT_LIBRARY_TIMER_HPP
#define AHC_PRECONTEST_KIT_LIBRARY_TIMER_HPP

#include <chrono>

namespace ahc {

class Timer {
 public:
  using Clock = std::chrono::steady_clock;

  Timer() : started_at_(Clock::now()) {}

  void reset() { started_at_ = Clock::now(); }

  double elapsed_ms() const {
    return std::chrono::duration<double, std::milli>(Clock::now() - started_at_)
        .count();
  }

  double elapsed_sec() const { return elapsed_ms() * 0.001; }

  bool reached(double limit_ms) const { return elapsed_ms() >= limit_ms; }

  double progress(double limit_ms) const {
    if (limit_ms <= 0.0) return 1.0;
    const double value = elapsed_ms() / limit_ms;
    if (value <= 0.0) return 0.0;
    if (value >= 1.0) return 1.0;
    return value;
  }

 private:
  Clock::time_point started_at_;
};

}  // namespace ahc

#endif  // AHC_PRECONTEST_KIT_LIBRARY_TIMER_HPP

