#include <cassert>
#include <chrono>
#include <type_traits>
#include <utility>

// 初期解を何個も作り、一番良いものを返す。
// generate() が State、evaluate(state) が Score を返すようにする。
//
// 使い方:
// State best = multi_start<State>(100, generate, evaluate);
// State best = time_based_multi_start<State>(500.0, generate, evaluate);
template <class State, class Generate, class Evaluate>
State multi_start(
    int trials,
    Generate generate,
    Evaluate evaluate,
    bool maximize = true) {
  assert(trials > 0);

  State best = generate();
  using Score = std::decay_t<decltype(evaluate(best))>;
  Score best_score = evaluate(best);

  for (int trial = 1; trial < trials; ++trial) {
    State candidate = generate();
    const Score score = evaluate(candidate);
    const bool better = maximize ? best_score < score : score < best_score;
    if (better) {
      best = std::move(candidate);
      best_score = score;
    }
  }
  return best;
}

// limit_ms ミリ秒まで初期解を作る。最低でも1個は作る。
template <class State, class Generate, class Evaluate>
State time_based_multi_start(
    double limit_ms,
    Generate generate,
    Evaluate evaluate,
    bool maximize = true) {
  assert(limit_ms > 0.0);
  const auto start = std::chrono::steady_clock::now();

  State best = generate();
  using Score = std::decay_t<decltype(evaluate(best))>;
  Score best_score = evaluate(best);

  while (true) {
    const auto now = std::chrono::steady_clock::now();
    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(now - start).count();
    if (elapsed_ms >= limit_ms) break;

    State candidate = generate();
    const Score score = evaluate(candidate);
    const bool better = maximize ? best_score < score : score < best_score;
    if (better) {
      best = std::move(candidate);
      best_score = score;
    }
  }
  return best;
}
