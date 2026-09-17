#include <cassert>
#include <iostream>
#include <vector>
#include "library/large-neighborhood-search.hpp"

template <class T> struct SequenceProblem {
  using Score = T;
  struct State { T score; State(T value) : score(value) {} };
  std::vector<T> values;
  std::vector<long double> thresholds;
  std::size_t position = 0;
  bool fail = false;
  void destroy(const State&, State& candidate, std::mt19937_64&, double) {
    candidate.score = values.at(position++);
  }
  std::optional<T> repair(State& candidate, std::mt19937_64&, double,
                           long double threshold) {
    thresholds.push_back(threshold);
    if (fail) return std::nullopt;
    return candidate.score;
  }
};

int main() {
  LnsOptions options;
  options.time_limit_ms = 100000;
  options.iteration_limit = 3;
  SequenceProblem<int> hill{{10, 9, 11}, {}};
  LargeNeighborhoodSearch<SequenceProblem<int>> h(hill, {10}, 10, options);
  assert(h.step() && h.last_outcome() == LnsOutcome::Accepted);
  assert(h.step() && h.last_outcome() == LnsOutcome::Rejected);
  assert(h.step() && h.last_outcome() == LnsOutcome::ImprovedBest);
  assert(h.current_score() == 11 && h.best_score() == 11);
  assert(h.accepted() == 2 && h.improved() == 1 && h.iterations() == 3);
  assert(!h.step());
  assert(h.last_outcome() == LnsOutcome::ImprovedBest); // 終了時には書き換えない。

  options.acceptance = LnsAcceptance::RecordToRecord;
  options.maximize = false;
  options.start_margin = options.end_margin = 2;
  SequenceProblem<int> rrt{{102, 104, 99}, {}};
  LargeNeighborhoodSearch<SequenceProblem<int>> r(rrt, {100}, 100, options);
  assert(r.step() && r.current_score() == 102 && r.best_score() == 100);
  assert(r.last_outcome() == LnsOutcome::Accepted);
  assert(r.step() && r.current_score() == 102); // current+2でなくbest+2を使う。
  r.run();
  assert(r.best_score() == 99 && r.accepted() == 2);
  assert(rrt.thresholds == std::vector<long double>({102, 102, 102}));

  options.maximize = true;
  SequenceProblem<int> max_rrt{{98, 96, 101}, {}};
  LargeNeighborhoodSearch<SequenceProblem<int>> mr(max_rrt, {100}, 100, options);
  mr.run();
  assert(mr.best_score() == 101 && mr.accepted() == 2);

  options.early_cutoff = false;
  SequenceProblem<int> failure{{1000, 1000, 1000}, {}};
  failure.fail = true;
  LargeNeighborhoodSearch<SequenceProblem<int>> f(failure, {20}, 20, options);
  f.run();
  assert(f.current_state().score == 20 && f.best_state().score == 20);
  assert(f.rejected_repairs() == 3 && f.accepted() == 0);
  assert(f.last_outcome() == LnsOutcome::Rejected);
  assert(std::isinf(failure.thresholds[0]) && failure.thresholds[0] < 0);

  options.early_cutoff = true;
  options.acceptance = LnsAcceptance::HillClimbing;
  const unsigned long long big = std::numeric_limits<unsigned long long>::max();
  SequenceProblem<unsigned long long> integer{{big - 1, big, big - 2}, {}};
  LargeNeighborhoodSearch<SequenceProblem<unsigned long long>> i(integer, {big - 1}, big - 1, options);
  i.run();
  assert(i.best_score() == big && i.accepted() == 2);

  options.acceptance = LnsAcceptance::RecordToRecord;
  options.start_margin = options.end_margin = 0.5;
  SequenceProblem<unsigned long long> fraction{{big - 1, big, big - 1}, {}};
  LargeNeighborhoodSearch<SequenceProblem<unsigned long long>> fr(fraction, {big}, big, options);
  fr.run();
  assert(fr.accepted() == 1 && fr.current_score() == big);

  SequenceProblem<double> nan{{std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(), 1.5}, {}};
  LargeNeighborhoodSearch<SequenceProblem<double>> d(nan, {1.0}, 1.0, options);
  d.run();
  assert(d.best_score() == 1.5 && d.rejected_repairs() == 2);

  options.acceptance = LnsAcceptance::SimulatedAnnealing;
  options.iteration_limit = 100;
  SequenceProblem<int> first, second;
  for (int j = 0; j < 100; ++j) first.values.push_back(20 + j % 9);
  second.values = first.values;
  LargeNeighborhoodSearch<SequenceProblem<int>> s1(first, {20}, 20, options);
  options.early_cutoff = false;
  LargeNeighborhoodSearch<SequenceProblem<int>> s2(second, {20}, 20, options);
  s1.run(); s2.run();
  assert(s1.best_score() == s2.best_score() && s1.accepted() == s2.accepted());
  s1.restart_from_best();
  assert(s1.current_score() == s1.best_score() && s1.iterations() == 100);

  options.iteration_limit = 0;
  SequenceProblem<int> empty;
  LargeNeighborhoodSearch<SequenceProblem<int>> zero(empty, {0}, 0, options);
  assert(!zero.step());
  options.time_limit_ms = -1;
  bool thrown = false;
  try { LargeNeighborhoodSearch<SequenceProblem<int>> bad(empty, {0}, 0, options); }
  catch (const std::invalid_argument&) { thrown = true; }
  assert(thrown);
  options.time_limit_ms = 100000;
  options.iteration_limit = 3;
  options.maximize = false;
  options.acceptance = LnsAcceptance::RecordToRecord;
  options.start_margin = options.end_margin = 5;
  SequenceProblem<int> feedback{{104, 102, 99}, {}};
  LargeNeighborhoodSearch<SequenceProblem<int>> fb(feedback, {100}, 100, options);
  assert(fb.step() && fb.last_outcome() == LnsOutcome::Accepted);
  assert(fb.step() && fb.last_outcome() == LnsOutcome::ImprovedCurrent);
  assert(fb.step() && fb.last_outcome() == LnsOutcome::ImprovedBest);
  options.time_limit_ms = 1.0;
  options.iteration_limit = std::numeric_limits<std::uint64_t>::max();
  LargeNeighborhoodSearch<SequenceProblem<int>> expired(empty, {0}, 0, options);
  while (expired.elapsed_ms() < 1.1) {}
  assert(!expired.step() && !expired.step() && expired.iterations() == 0);
  std::cout << "LNS control tests passed\n";
}
