#include <cassert>
#include <cmath>
#include <random>
#include "library/forward-backward-dp.hpp"

template <class Real>
void test() {
  auto transitions = [](std::size_t t, int from, int action, auto emit) {
    emit(from, Real(0.25), Real(-1));
    emit((from + action + 1) % 4, Real(0.5), Real(from + int(t)) / 3);
    emit(-1, Real(0.25), Real(action * 2));
  };
  auto full = [&](const std::vector<int>& actions) {
    std::vector<Real> p{1, 0, 0, 0};
    Real score = 0;
    for (std::size_t t = 0; t < actions.size(); ++t) {
      std::vector<Real> next(4);
      for (int from = 0; from < 4; ++from) {
        transitions(t, from, actions[t], [&](int to, Real prob, Real reward) {
          score += p[from] * prob * reward;
          if (to >= 0) next[to] += p[from] * prob;
        });
      }
      p.swap(next);
    }
    for (int i = 0; i < 4; ++i) score += p[i] * Real(i + 2);
    return score;
  };
  auto equal = [](Real a, Real b) { assert(std::abs(a - b) < 1e-9L); };
  ForwardBackwardDP<int, Real> dp(4);
  std::vector<int> actions{1, 0, 3, 2, 1, 1, 0};
  const std::vector<Real> initial{1, 0, 0, 0}, terminal{2, 3, 4, 5};
  bool threw = false;
  try { dp.score(); } catch (const std::logic_error&) { threw = true; }
  assert(threw);
  dp.build(initial, actions, transitions, terminal);
  std::mt19937 rng(9);
  for (int trial = 0; trial < 100; ++trial) {
    equal(dp.score(), full(actions));
    const auto before = dp.score();
    for (std::size_t position = 0; position < actions.size(); ++position) {
      for (int action = 0; action < 4; ++action) {
        auto changed = actions;
        changed[position] = action;
        equal(dp.score_if_changed(position, action, transitions), full(changed));
        assert(dp.actions() == actions && dp.score() == before);
      }
    }
    const auto position = rng() % actions.size();
    const int action = rng() % 4;
    actions[position] = action;
    dp.commit_change(position, action, transitions);
    assert(dp.actions() == actions);
  }
  threw = false;
  try { dp.score_if_changed(actions.size(), 0, transitions); } catch (const std::out_of_range&) { threw = true; }
  assert(threw);
  dp.build(initial, {}, transitions, terminal);
  equal(dp.score(), 2);
  dp.build(initial, {}, transitions);
  equal(dp.score(), 0);
  threw = false;
  try { dp.build({}, {}, transitions); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  dp.build(initial, actions, transitions, terminal);
  auto broken = [](auto, auto, auto, auto) { throw std::runtime_error("callback failed"); };
  try { dp.commit_change(0, 0, broken); } catch (const std::runtime_error&) {}
  threw = false;
  try { dp.score(); } catch (const std::logic_error&) { threw = true; }
  assert(threw);
  dp.build(initial, actions, transitions, terminal);
  equal(dp.score(), full(actions));
}

int main() {
  test<double>();
  test<long double>();
  bool threw = false;
  try { ForwardBackwardDP<int> dp(0); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}
