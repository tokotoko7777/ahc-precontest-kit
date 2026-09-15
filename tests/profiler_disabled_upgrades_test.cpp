#include <cassert>
#include <sstream>
#include <type_traits>
#include "library/scope-profiler.hpp"

int main() {
  static_assert(std::is_empty<ScopeProfiler>::value, "disabled profiler stores nothing");
  static_assert(std::is_empty<ScopeProfiler::Guard>::value, "disabled guard stores nothing");
  ScopeProfiler p("unused");
  { auto guard = p.measure(); }
  assert(p.calls() == 0 && p.elapsed_ms() == 0);
  std::ostringstream output;
  p.report(output);
  assert(output.str().empty());
}
