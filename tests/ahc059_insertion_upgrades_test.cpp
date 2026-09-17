#define AHC059_LNS_NO_MAIN
#include "examples/search/ahc059_lns.cpp"

int brute_insertion(const CardPairProblem& problem,
                    const CardPairProblem::State& state, int id) {
  int best = numeric_limits<int>::max();
  const int size = static_cast<int>(state.order.size());
  for (int direction = 0; direction < 2; ++direction) {
    for (int first = 0; first <= size; ++first) {
      for (int second = first; second <= size; ++second) {
        auto candidate = state;
        candidate.order.insert(candidate.order.begin() + second, problem.cells[id][1 - direction]);
        candidate.order.insert(candidate.order.begin() + first, problem.cells[id][direction]);
        candidate.cost = problem.route_cost(candidate.order);
        if (problem.is_valid(candidate)) best = min(best, candidate.cost);
      }
    }
  }
  return best;
}

int main() {
  mt19937_64 rng(184903);
  // 3ペアの全順列から合法な全括弧形・全向きを調べる（貪欲生成に偏らない）。
  stringstream tiny;
  tiny << "4\n";
  for (int cell = 0; cell < 16; ++cell) tiny << cell / 2 << ' ';
  CardPairProblem exhaustive;
  exhaustive.read_input(tiny);
  vector<int> permutation{0, 1, 2, 3, 4, 5};
  int legal_orders = 0;
  do {
    CardPairProblem::State state{permutation, exhaustive.route_cost(permutation)};
    if (!exhaustive.is_valid(state)) continue;
    ++legal_orders;
    const auto choice = exhaustive.best_insertion(permutation, 3);
    assert(state.cost + choice.delta == brute_insertion(exhaustive, state, 3));
  } while (next_permutation(permutation.begin(), permutation.end()));
  assert(legal_orders == 240);
  for (int trial = 0; trial < 80; ++trial) {
    const int n = trial < 40 ? 4 : 6, pairs = n * n / 2;
    vector<int> labels(n * n);
    for (int j = 0; j < n * n; ++j) labels[j] = j / 2;
    shuffle(labels.begin(), labels.end(), rng);
    stringstream input;
    input << n << '\n';
    for (int id : labels) input << id << ' ';
    CardPairProblem problem;
    problem.read_input(input);
    CardPairProblem::State state;
    vector<int> ids(pairs);
    iota(ids.begin(), ids.end(), 0);
    shuffle(ids.begin(), ids.end(), rng);
    for (int id : ids) {
      const auto fast = problem.best_insertion(state.order, id);
      const int brute = brute_insertion(problem, state, id);
      assert(state.cost + fast.delta == brute);
      problem.precompute = false;
      const auto uncached = problem.best_insertion(state.order, id);
      problem.precompute = true;
      assert(fast.delta == uncached.delta && fast.first_gap == uncached.first_gap &&
             fast.second_gap == uncached.second_gap);
      problem.insert_pair(state, id);
      assert(problem.is_valid(state) && state.cost == brute);
    }
    const auto original = state;
    CardPairProblem::State scratch;
    for (int j = 0; j < 30; ++j) {
      problem.destroy(state, scratch, rng, 0.5);
      const int lower_bound = scratch.cost;
      const auto rejected = problem.repair(scratch, rng, 0.5, lower_bound - 1);
      assert(!rejected && state.order == original.order);
      const auto repaired = problem.repair(scratch, rng, 0.5,
                                           numeric_limits<long double>::infinity());
      assert(repaired && problem.is_valid(scratch) && *repaired >= lower_bound);
      assert(static_cast<int>(scratch.order.size()) == n * n);
    }
  }
  cout << "AHC059 insertion: exhaustive gap/orientation comparisons passed\n";
}
