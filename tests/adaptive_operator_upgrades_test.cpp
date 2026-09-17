#include <cassert>
#include <iostream>
#include <limits>
#include "library/adaptive-operator-selector.hpp"

bool close(double a, double b) { return std::abs(a - b) < 1e-12; }
template <class F> void invalid(F f) {
  bool threw = false;
  try { f(); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}
int main() {
  AdaptiveOperatorOptions options;
  options.update_interval = 4;
  options.learning_rate = 1;
  options.exploration = 0.2;
  AdaptiveOperatorSelector selector(2, options);
  assert(close(selector.probability(0), 0.5));
  selector.record(0, 1); selector.record(1, 0);
  selector.record(0, 1);
  assert(close(selector.probability(0), 0.5)); // 区間終了までは重みを固定。
  selector.record(1, 0);
  assert(close(selector.probability(0), 0.9) && close(selector.probability(1), 0.1));
  // 成功回数の和でなく、試行回数あたりの平均で学習する。
  selector.record(0, 1); selector.record(0, 0); selector.record(0, 0.5); selector.record(1, 0.5);
  assert(close(selector.probability(0), 0.5));
  for (int j = 0; j < 4; ++j) selector.record(j % 2, 0);
  assert(close(selector.probability(0), 0.5)); // 全報酬0・重み0でも等確率。
  selector.record(0, 1); selector.update();
  assert(close(selector.probability(0), 0.9)); // 部分区間を反映。
  selector.update();
  assert(close(selector.probability(0), 0.9)); // 空区間は変化なし。
  AdaptiveOperatorSelector untried(2, options);
  for (int j = 0; j < 4; ++j) untried.record(0, 0);
  assert(close(untried.probability(1), 0.9)); // 未試行は失敗扱いしない。
  std::mt19937_64 rng(1);
  int selected[2] = {};
  for (int j = 0; j < 100000; ++j) ++selected[selector.select(rng)];
  assert(selected[0] > 89000 && selected[0] < 91000);
  assert(selected[1] > 9000); // explorationによる下限。
  options.learning_rate = 0.25;
  AdaptiveOperatorSelector smooth(2, options);
  for (int j = 0; j < 4; ++j) smooth.record(j % 2, j % 2);
  assert(close(smooth.probability(0), 0.1 + 0.8 * 0.75 / 1.75));
  options.adaptive = false;
  AdaptiveOperatorSelector fixed(3, options);
  for (int j = 0; j < 1000; ++j) fixed.record(j % 3, j % 3 == 0 ? 1 : 0);
  fixed.update();
  for (int id = 0; id < 3; ++id) assert(close(fixed.probability(id), 1.0 / 3));
  AdaptiveOperatorSelector one(1);
  for (int j = 0; j < 1000; ++j) { assert(one.select(rng) == 0); one.record(0, 0); }
  assert(one.probability(0) == 1);
  // 同一乱数・同一報酬なら選択列も完全一致。
  AdaptiveOperatorSelector a(5), b(5);
  std::mt19937_64 ra(999), rb(999);
  for (int j = 0; j < 10000; ++j) {
    const int x = a.select(ra), y = b.select(rb);
    assert(x == y);
    a.record(x, x == 2 ? 1 : 0); b.record(y, y == 2 ? 1 : 0);
  }
  assert(a.probability(2) > 0.9);
  invalid([] { AdaptiveOperatorSelector bad(0); });
  invalid([&] { fixed.record(-1, 0); });
  invalid([&] { fixed.record(3, 0); });
  for (double reward : {-0.1, 1.1, std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::quiet_NaN()}) {
    invalid([&] { fixed.record(0, reward); });
  }
  options.update_interval = 0;
  invalid([&] { AdaptiveOperatorSelector bad(1, options); });
  options.update_interval = 1;
  for (double e : {0.0, -1.0, 1.1, std::numeric_limits<double>::quiet_NaN()}) {
    options.exploration = e;
    invalid([&] { AdaptiveOperatorSelector bad(1, options); });
  }
  options.exploration = 0.1;
  options.learning_rate = 1.1;
  invalid([&] { AdaptiveOperatorSelector bad(1, options); });
  std::cout << "adaptive operator selector tests passed\n";
}
