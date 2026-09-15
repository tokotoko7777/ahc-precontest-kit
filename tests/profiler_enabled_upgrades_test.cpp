#define AHC_ENABLE_PROFILING
#include <cassert>
#include <sstream>
#include <type_traits>
#include "library/scope-profiler.hpp"

int main() {
  static_assert(!std::is_copy_constructible<ScopeProfiler::Guard>::value, "guard cannot be copied");
  ScopeProfiler a("evaluation"), b("rebuild");
  {
    auto outer = a.measure();
    { auto inner = b.measure(); }
    assert(a.calls() == 0 && b.calls() == 1);
  }
  try { auto guard = a.measure(); throw 1; } catch (int) {}
  assert(a.calls() == 2 && b.calls() == 1);
  assert(a.elapsed_ms() >= 0 && b.elapsed_ms() >= 0);
  std::ostringstream output;
  a.report(output);
  assert(output.str().find("evaluation: calls=2 total_ms=") == 0);
}
