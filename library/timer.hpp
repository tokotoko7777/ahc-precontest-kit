#include <chrono>

struct Timer {
  std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();

  void reset() { start = std::chrono::steady_clock::now(); }

  double elapsed_ms() const {
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(now - start).count();
  }

  bool is_over(double limit_ms) const { return elapsed_ms() >= limit_ms; }

  double progress(double limit_ms) const {
    if (limit_ms <= 0.0) return 1.0;
    const double value = elapsed_ms() / limit_ms;
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
  }
};

