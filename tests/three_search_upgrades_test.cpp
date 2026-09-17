#include <cassert>
#include <array>
#include <iostream>
#include "library/chokudai-search.hpp"
#include "library/monte-carlo-tree-search.hpp"
#include "library/iterated-local-search.hpp"

struct TreeProblem {
  struct State { int depth = 0, choice = 0; };
  using Score = long long; using Action = int;
  std::vector<int> generate_actions(const State& s) const { return s.depth ? std::vector<int>{0} : std::vector<int>{0,1}; }
  Score evaluate_action(const State& s, int a) const { return s.depth ? 0 : 10-a; }
  void apply_action(State& s, int& a) const { if (!s.depth) s.choice = a; ++s.depth; }
  bool is_terminal(const State& s) const { return s.depth == 2; }
  Score final_score(const State& s) const { return s.choice ? 100 : 20; }
};
struct ChanceProblem {
  struct State { int stage = 0, coin = 0; double reward = 0; };
  using Action = int;
  std::array<int,2> generate_actions(const State&) const { return {0,1}; }
  bool is_terminal(const State& s) const { return s.stage == 2; }
  uint64_t sample_transition(State& s, int a, std::mt19937_64& rng) const {
    if (!s.stage) {
      if (!a) { s.stage = 2; s.reward = 0.7; return 0; }
      s.stage = 1; s.coin = static_cast<int>(rng() % 2); return s.coin;
    }
    s.reward = a == s.coin ? 1 : 0; s.stage = 2; return 0;
  }
  double rollout(State s, std::mt19937_64&) const { return s.stage == 2 ? s.reward : 0; }
};
struct SequenceLocal {
  using Score = double;
  struct State { double value; explicit State(double v) : value(v) {} };
  std::vector<double> scores; std::size_t at = 0; int kicks = 0;
  void perturb(const State& s, State& c, std::mt19937_64&, const IlsBudget&) { c=s; ++kicks; }
  double local_search(State& s, std::mt19937_64&, const IlsBudget&) { return s.value = scores.at(at++); }
};
template <class F> void expect_invalid(F f) {
  bool threw=false; try { f(); } catch (const std::invalid_argument&) { threw=true; } assert(threw);
}
int main() {
  TreeProblem problem;
  ChokudaiOptions c; c.max_depth=2; c.capacity_per_depth=2; c.time_limit_ms=100000;
  ChokudaiSearch<TreeProblem> search(problem, {}, 0, c); search.run();
  assert(search.best_score() == 100 && search.best_state()->choice == 1 && !search.step());
  c.maximize=false;
  ChokudaiSearch<TreeProblem> minimize(problem, {}, 0, c); minimize.run();
  assert(minimize.best_score()==20);
  c.max_depth=1;
  ChokudaiSearch<TreeProblem> shallow(problem, {}, 0, c); shallow.run(); assert(!shallow.best_state());
  c.max_depth=2; c.expansion_limit=0;
  ChokudaiSearch<TreeProblem> zero(problem, {}, 0, c); assert(!zero.step());
  c.capacity_per_depth=0; expect_invalid([&]{ ChokudaiSearch<TreeProblem> bad(problem,{},0,c); });
  c.capacity_per_depth=1; c.expansion_limit=100; c.maximize=true;
  ChokudaiSearch<TreeProblem> bounded(problem,{},0,c); bounded.run();
  assert(bounded.best_score()==20); // 容量1では低い順位値の有望な枝も捨てる近似。
  ChokudaiSearch<TreeProblem> terminal(problem,{2,1},0,c); terminal.run();
  assert(terminal.best_score()==100 && terminal.evaluations()==0);
  struct FloatingTree : TreeProblem {
    using Score=double;
    double evaluate_action(const State& s,int a) const {
      return !s.depth && a==0 ? std::numeric_limits<double>::quiet_NaN() : 1;
    }
  } floating;
  ChokudaiSearch<FloatingTree> finite_only(floating,{},0,c); finite_only.run();
  assert(finite_only.best_score()==100);
  expect_invalid([&]{ ChokudaiSearch<FloatingTree> bad(floating,{},std::numeric_limits<double>::infinity(),c); });
  ChanceProblem chance;
  MctsOptions m; m.time_limit_ms=100000; m.iteration_limit=10000; m.max_depth=3;
  MonteCarloTreeSearch<ChanceProblem> uct(chance,123), same(chance,123);
  assert(uct.choose_action({},m)==1); assert(same.choose_action({},m)==1);
  assert(uct.nodes()==3 && same.nodes()==3 && uct.iterations()==10000);
  m.max_depth=1;
  assert(uct.choose_action({},m)==0 && uct.nodes()==1);
  m.max_depth=3;
  m.max_nodes=1;
  assert(uct.choose_action({},m)==0 && uct.nodes()==1); // 木を伸ばせない時も安全に動く。
  m.iteration_limit=0; assert(uct.choose_action({},m)==0);
  assert(!uct.choose_action({2,0,0.5},m));
  m.iteration_limit=1000; m.maximize=false;
  assert(uct.choose_action({},m)==1); // rollout値0を最小化。
  m.max_depth=0; expect_invalid([&]{ uct.choose_action({},m); });
  struct BadReward : ChanceProblem { double rollout(State, std::mt19937_64&) const { return 2; } } bad;
  MonteCarloTreeSearch<BadReward> invalid(bad); m.max_depth=1;
  expect_invalid([&]{ invalid.choose_action({},m); });
  struct EmptyChance : ChanceProblem {
    std::vector<int> generate_actions(const State&) const { return {}; }
  } empty;
  MonteCarloTreeSearch<EmptyChance> empty_search(empty);
  assert(!empty_search.choose_action({},m) && empty_search.iterations()==0);
  struct Chain {
    using State=int; using Action=int;
    std::array<int,1> generate_actions(int) const { return {0}; }
    bool is_terminal(int state) const { return state==16; }
    uint64_t sample_transition(int& state,int,std::mt19937_64&) const { ++state; return 0; }
    double rollout(int state,std::mt19937_64&) const { return state/16.0; }
  } chain;
  MonteCarloTreeSearch<Chain> growing(chain);
  m.maximize=true; m.max_depth=20; m.max_nodes=100;
  assert(growing.choose_action(0,m)==0 && growing.nodes()==16); // vector再確保と決定的な結果ID。
  IlsOptions o; o.time_limit_ms=100000; o.iteration_limit=3;
  SequenceLocal seq{{11,9,12}};
  IteratedLocalSearch<SequenceLocal> ils(seq, SequenceLocal::State(10),10,o); ils.run();
  assert(ils.best_score()==12 && ils.current_score()==12 && ils.accepted()==2 && seq.kicks==2);
  o.accept_worse=true; o.restart_after=1;
  SequenceLocal restart{{10,8,6}};
  IteratedLocalSearch<SequenceLocal> r(restart,SequenceLocal::State(10),10,o); r.run();
  assert(r.best_score()==10 && r.current_score()==10 && r.accepted()==3);
  o.restart_after=0; o.maximize=false;
  SequenceLocal minseq{{10,8,6}};
  IteratedLocalSearch<SequenceLocal> mi(minseq,SequenceLocal::State(10),10,o); mi.run();
  assert(mi.best_score()==6);
  SequenceLocal nanseq{{std::numeric_limits<double>::quiet_NaN(), 12, 11}};
  o.maximize=true; o.accept_worse=false;
  IteratedLocalSearch<SequenceLocal> na(nanseq,SequenceLocal::State(10),10,o); na.run();
  assert(na.best_score()==12 && na.accepted()==1);
  o.iteration_limit=0;
  SequenceLocal untouched{{}};
  IteratedLocalSearch<SequenceLocal> no_steps(untouched,SequenceLocal::State(10),10,o); no_steps.run();
  assert(no_steps.best_score()==10 && no_steps.iterations()==0 && untouched.at==0);
  o.time_limit_ms=0; expect_invalid([&]{ IteratedLocalSearch<SequenceLocal> invalid_ils(seq,SequenceLocal::State(10),10,o); });
  std::cout << "chokudai / closed-loop UCT / ILS tests passed\n";
}
