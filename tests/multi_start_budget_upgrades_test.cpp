#include "library/multi-start.hpp"
#include <cassert>
#include <memory>

int main() {
  auto score = [](int x) { return x; };
  auto never = [] { return false; };
  int calls = 0;
  auto generate = [&](int trial, auto&) -> std::optional<int> {
    ++calls;
    if (trial == 1) return std::nullopt;
    return trial + 5;
  };
  assert(budgeted_multi_start(0, 0, generate, score, never) == 0 && calls == 0);
  assert(budgeted_multi_start(3, 10, generate, score, [] { return true; }) == 3 && calls == 0);
  assert(budgeted_multi_start(0, 4, generate, score, never) == 8 && calls == 4);
  assert(budgeted_multi_start(100, 4, generate, score, never, false) == 5);
  // 時計を実時間にせず、探索内の経過量を使って中断と最後の完成解を検証。
  int ticks = 0, evaluated = 0, attempts = 0;
  auto stop = [&] { return ticks >= 5; };
  auto cooperative = [&](int trial, auto& deadline) -> std::optional<int> {
    ++attempts;
    for (int i = 0; i < 3; ++i) {
      if (deadline()) return std::nullopt;
      ++ticks;
    }
    return 10 + trial;
  };
  auto eval = [&](int x) { ++evaluated; return x; };
  assert(budgeted_multi_start(0, 100, cooperative, eval, stop) == 10);
  assert(ticks == 5 && attempts == 2 && evaluated == 2); // fallback + 完成1回のみ。
  // 処理中に締切を越えて戻っても完成解は捨てない。時間保証ではない。
  ticks = 0;
  auto late = [&](int, auto&) -> std::optional<int> { ticks = 6; return 9; };
  assert(budgeted_multi_start(1, 3, late, score, stop) == 9);
  using Pointer = std::unique_ptr<int>;
  auto pointer_score = [](const Pointer& p) { return *p; };
  auto tied = [](int, auto&) -> std::optional<Pointer> { return std::make_unique<int>(7); };
  auto first = std::make_unique<int>(7);
  int* identity = first.get();
  auto best = budgeted_multi_start(std::move(first), 3, tied, pointer_score, never);
  assert(best.get() == identity); // 同点は先着。move-only Stateでも動く。
  auto smaller = [](int, auto&) -> std::optional<Pointer> { return std::make_unique<int>(2); };
  best = budgeted_multi_start(std::move(best), 1, smaller, pointer_score, never, false);
  assert(*best == 2);
  // 以前のAPIも残す。
  assert(multi_start<int>(2, [] { return 3; }, score) == 3);
}
