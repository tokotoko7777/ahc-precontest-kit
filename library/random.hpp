// AHC LIBRARY: Random
// Copy this whole file above main().

#ifndef AHC_PRECONTEST_KIT_LIBRARY_RANDOM_HPP
#define AHC_PRECONTEST_KIT_LIBRARY_RANDOM_HPP

#include <cassert>
#include <cstdint>
#include <iterator>
#include <utility>

namespace ahc {

// SplitMix64-based generator. The same seed produces the same sequence.
class Random {
 public:
  explicit Random(std::uint64_t seed = 0x243f6a8885a308d3ULL)
      : state_(seed) {}

  std::uint64_t next_u64() {
    std::uint64_t value = (state_ += 0x9e3779b97f4a7c15ULL);
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
  }

  std::uint32_t next_u32() {
    return static_cast<std::uint32_t>(next_u64() >> 32);
  }

  // Returns a value in [0, upper_exclusive) without modulo bias.
  std::uint64_t uniform(std::uint64_t upper_exclusive) {
    assert(upper_exclusive > 0);
    const std::uint64_t threshold = -upper_exclusive % upper_exclusive;
    while (true) {
      const std::uint64_t value = next_u64();
      if (value >= threshold) return value % upper_exclusive;
    }
  }

  // Returns an int in [lower, upper_exclusive).
  int uniform_int(int lower, int upper_exclusive) {
    assert(lower < upper_exclusive);
    const auto width = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(upper_exclusive) - lower);
    const auto value = static_cast<std::int64_t>(lower) +
                       static_cast<std::int64_t>(uniform(width));
    return static_cast<int>(value);
  }

  // Returns a double in [0, 1) using 53 random bits.
  double uniform01() {
    return static_cast<double>(next_u64() >> 11) * 0x1.0p-53;
  }

  template <class RandomIt>
  void shuffle(RandomIt first, RandomIt last) {
    using Difference = typename std::iterator_traits<RandomIt>::difference_type;
    const Difference size = last - first;
    for (Difference i = size; i > 1; --i) {
      const Difference j = static_cast<Difference>(
          uniform(static_cast<std::uint64_t>(i)));
      using std::swap;
      swap(first[i - 1], first[j]);
    }
  }

 private:
  std::uint64_t state_;
};

}  // namespace ahc

#endif  // AHC_PRECONTEST_KIT_LIBRARY_RANDOM_HPP

