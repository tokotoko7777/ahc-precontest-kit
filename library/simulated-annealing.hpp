// AHC LIBRARY: Simulated Annealing
// Copy this whole file above main().

#ifndef AHC_PRECONTEST_KIT_LIBRARY_SIMULATED_ANNEALING_HPP
#define AHC_PRECONTEST_KIT_LIBRARY_SIMULATED_ANNEALING_HPP

#include <cassert>
#include <cmath>
#include <cstdint>

namespace ahc {

// Handles geometric cooling and transition acceptance only.
//
// improvement must be positive when the candidate is better:
//   maximize: candidate_score - current_score
//   minimize: current_cost - candidate_cost
//
// progress is clamped to [0, 1]. It can come from elapsed time or iterations.
class SimulatedAnnealing {
 public:
  SimulatedAnnealing(
      double start_temperature,
      double end_temperature,
      std::uint64_t seed = 0x243f6a8885a308d3ULL)
      : start_temperature_(start_temperature),
        end_temperature_(end_temperature),
        random_state_(seed) {
    assert(start_temperature_ > 0.0);
    assert(end_temperature_ > 0.0);
  }

  double temperature(double progress) const {
    progress = clamp_progress(progress);
    return start_temperature_ *
           std::pow(end_temperature_ / start_temperature_, progress);
  }

  bool accept(double improvement, double progress) {
    if (improvement >= 0.0) return true;
    const double probability =
        std::exp(improvement / temperature(progress));
    return uniform01() < probability;
  }

 private:
  static double clamp_progress(double progress) {
    if (progress <= 0.0) return 0.0;
    if (progress >= 1.0) return 1.0;
    return progress;
  }

  std::uint64_t next_u64() {
    std::uint64_t value = (random_state_ += 0x9e3779b97f4a7c15ULL);
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
  }

  double uniform01() {
    return static_cast<double>(next_u64() >> 11) * 0x1.0p-53;
  }

  double start_temperature_;
  double end_temperature_;
  std::uint64_t random_state_;
};

}  // namespace ahc

#endif  // AHC_PRECONTEST_KIT_LIBRARY_SIMULATED_ANNEALING_HPP

