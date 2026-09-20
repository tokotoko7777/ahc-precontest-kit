#define AHC004_TEST
#include "../examples/search/ahc004_genome_sa.cpp"

// 問題側の差分cacheを、探索とは独立した単純な全文字比較と照合する。
static void check_state(const GenomeProblem& problem, const GenomeProblem::State& state) {
  vector<int> counts(problem.targets.size());
  int covered = 0, empty = 0;
  for (int id = 0; id < (int)problem.targets.size(); ++id) {
    const string& word = problem.targets[id].text;
    for (int d = 0; d < 2; ++d) for (int r = 0; r < problem.n; ++r)
      for (int c = 0; c < problem.n; ++c) {
        bool match = true;
        for (int k = 0; k < (int)word.size(); ++k)
          match &= state.board[(r+d*k)%problem.n][(c+(1-d)*k)%problem.n] == word[k];
        counts[id] += match;
      }
    if (counts[id]) covered += problem.targets[id].frequency;
  }
  for (const string& row : state.board) empty += count(row.begin(), row.end(), '.');
  assert(counts == state.count && covered == state.covered && empty == state.empty);
  auto rebuilt = problem.make_state(state.board);
  assert(rebuilt.line_hits == state.line_hits);
  assert(rebuilt.line_code == state.line_code && rebuilt.line_empty == state.line_empty);
  for (int id = 0; id < min(5, (int)problem.targets.size()); ++id) {
    const auto& word = problem.targets[id].text;
    uint64_t code = 0;
    for (int k = 0; k < (int)word.size(); ++k) code |= uint64_t(word[k]-'A') << (3*k);
    for (int line = 0; line < 2*problem.n; ++line) for (int start = 0; start < problem.n; ++start) {
      int quality = 0;
      for (int k = 0; k < (int)word.size(); ++k) {
        int cell = problem.line_cells[line][start+k];
        char c = state.board[cell/problem.n][cell%problem.n];
        quality += c == word[k] ? 1000 : c == '.' ? 80 : -250;
      }
      assert(quality == problem.placement_quality(state, line, start, code, word.size()));
    }
  }
}

int main() {
  { // suffix、重複登録、空辞書、再build、入力検証
    AhoCorasick<2> ac;
    ac.add({0,1,0}, 1); ac.add({1,0}, 2); ac.add({0}, 3); ac.add({0}, 4);
    ac.build(); ac.build();
    int state = 0;
    for (int c : {0,1,0}) state = ac.advance(state, c);
    auto matches = ac.matches(state);
    sort(matches.begin(), matches.end());
    assert((matches == vector<int>{1,2,3,4}));
    bool caught = false;
    try { ac.add({0}, 0); } catch (const invalid_argument&) { caught = true; }
    assert(caught);
    AhoCorasick<1> empty; empty.build();
    assert(empty.advance(0,0) == 0 && empty.matches(0).empty());
    AhoCorasick<2> bad;
    caught = false;
    try { bad.add({2}, 0); } catch (const out_of_range&) { caught = true; }
    assert(caught && bad.nodes.size() == 1);
    caught = false;
    try { bad.add({}, 0); } catch (const invalid_argument&) { caught = true; }
    assert(caught);
  }
  mt19937_64 rng(123);
  vector<Target> targets;
  int m = 0;
  for (int id = 0; id < 120; ++id) {
    string word(2+rng()%11, 'A');
    for (char& c : word) c += rng()%4;
    int weight = 1+rng()%3;
    targets.push_back({word, weight, 0, 0, true}); m += weight;
  }
  GenomeProblem problem(20, m, targets);
  vector<string> board(20, string(20, '.'));
  for (auto& row : board) for (char& c : row) if (rng()%5) c = 'A'+rng()%4;
  auto state = problem.make_state(board);
  check_state(problem, state);
  for (int iteration = 0; iteration < 1200; ++iteration) {
    auto move = problem.propose_move(state, rng, double(iteration)/1200);
    if (!move) continue;
    const auto before = state;
    auto delta = problem.evaluate_move(state, *move, -INFINITY);
    auto changed = state.board;
    for (auto [cell, c] : move->changes) changed[cell/20][cell%20] = c;
    auto rebuilt = problem.make_state(changed);
    assert(delta && abs(*delta - (problem.score(rebuilt)-problem.score(state))) < 1e-10);
    assert(state.board == before.board && state.count == before.count);
    // 今回の評価実装は閾値を無視する。棄却後の次試行でもscratchが漏れない。
    assert(problem.evaluate_move(state, *move, 10000) == delta);
    if (rng()%2) {
      problem.apply_move(state, *move);
      assert(state.board == rebuilt.board && state.count == rebuilt.count);
    }
    if (iteration%100 == 0) check_state(problem, state);
  }
  check_state(problem, state);
  // 巡回端、全一致時だけ有効な空白bonus、1文字消去の差分。
  GenomeProblem perfect(20, 3, vector<Target>{{"AB",2,0,0,true},{"BA",1,0,0,true}});
  board.assign(20, string(20, '.')); board[0][19]='A'; board[0][0]='B'; board[0][1]='A';
  auto full = perfect.make_state(board);
  check_state(perfect, full);
  assert(full.covered == 3 && perfect.score(full) > 3);
  GenomeProblem::Move erase{{{19,'.'}}};
  auto delta = perfect.evaluate_move(full, erase, -INFINITY);
  perfect.apply_move(full, erase);
  check_state(perfect, full);
  assert(delta && full.covered == 1 && perfect.score(full) == 1);
  cout << "AHC004: automaton + 1200 delta/cache/rollback checks passed\n";
}
