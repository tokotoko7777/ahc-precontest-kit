#include <bits/stdc++.h>
using namespace std;

// library/timer.hpp
struct Timer {
  chrono::steady_clock::time_point start = chrono::steady_clock::now();

  void reset() { start = chrono::steady_clock::now(); }

  double elapsed_ms() const {
    const auto now = chrono::steady_clock::now();
    return chrono::duration<double, milli>(now - start).count();
  }

  bool is_over(double limit_ms) const { return elapsed_ms() >= limit_ms; }

  double remaining_ms(double limit_ms) const {
    const double value = limit_ms - elapsed_ms();
    return value > 0.0 ? value : 0.0;
  }

  double progress(double limit_ms) const {
    if (limit_ms <= 0.0) return 1.0;
    return clamp(elapsed_ms() / limit_ms, 0.0, 1.0);
  }
};

// library/zobrist-hash.hpp
struct ZobristHash {
  int positions;
  int value_kinds;
  vector<vector<uint64_t>> table;

  ZobristHash(int positions, int value_kinds, uint64_t seed = 0)
      : positions(positions),
        value_kinds(value_kinds),
        table(positions < 0 ? 0 : positions,
              vector<uint64_t>(value_kinds <= 0 ? 0 : value_kinds)) {
    assert(positions >= 0);
    assert(value_kinds > 0);
    mt19937_64 engine(seed);
    for (auto& row : table) {
      for (uint64_t& value : row) value = engine();
    }
  }

  uint64_t build(const vector<int>& values) const {
    assert(static_cast<int>(values.size()) == positions);
    uint64_t hash = 0;
    for (int position = 0; position < positions; ++position) {
      assert(0 <= values[position] && values[position] < value_kinds);
      hash ^= table[position][values[position]];
    }
    return hash;
  }

  void change(uint64_t& hash,
              int position,
              int old_value,
              int new_value) const {
    assert(0 <= position && position < positions);
    assert(0 <= old_value && old_value < value_kinds);
    assert(0 <= new_value && new_value < value_kinds);
    hash ^= table[position][old_value];
    hash ^= table[position][new_value];
  }
};

// library/tree-beam-search.hpp
template <class State, class Action, class Score>
struct TreeBeamSearch {
  struct Node {
    int parent;
    int depth;
    optional<Action> action;
    Score score;
  };

  struct Candidate {
    int parent;
    Action action;
    Score score;
  };

  State state;
  int beam_width;
  bool maximize;
  int current_node = 0;
  vector<Node> nodes;
  vector<int> beam;

  TreeBeamSearch(State initial_state,
                 Score initial_score,
                 int beam_width,
                 bool maximize = true)
      : state(move(initial_state)),
        beam_width(beam_width),
        maximize(maximize) {
    assert(beam_width > 0);
    nodes.push_back({-1, 0, nullopt, move(initial_score)});
    beam.push_back(0);
  }

  template <class Expand,
            class Apply,
            class Revert,
            class Evaluate,
            class MakeKey>
  bool step_with_key(Expand expand,
                     Apply apply,
                     Revert revert,
                     Evaluate evaluate,
                     MakeKey make_key) {
    using Key = decay_t<decltype(make_key(state))>;
    struct KeyedCandidate {
      Candidate candidate;
      Key key;
    };

    vector<KeyedCandidate> candidates;

    for (int parent : beam) {
      move_to(parent, apply, revert);
      auto actions = expand(state);
      for (Action& action : actions) {
        apply(state, action);
        const Score score = evaluate(state);
        Key key = make_key(state);
        revert(state, action);
        candidates.push_back(
            {{parent, move(action), score}, move(key)});
      }
    }

    if (candidates.empty()) return false;

    stable_sort(candidates.begin(), candidates.end(),
                [&](const KeyedCandidate& a, const KeyedCandidate& b) {
      return maximize ? b.candidate.score < a.candidate.score
                      : a.candidate.score < b.candidate.score;
    });

    unordered_set<Key> used_keys;
    beam.clear();
    beam.reserve(beam_width);

    for (KeyedCandidate& keyed : candidates) {
      if (!used_keys.insert(keyed.key).second) continue;
      Candidate& candidate = keyed.candidate;
      const int depth = nodes[candidate.parent].depth + 1;
      nodes.push_back({candidate.parent,
                       depth,
                       move(candidate.action),
                       move(candidate.score)});
      beam.push_back(static_cast<int>(nodes.size()) - 1);
      if (static_cast<int>(beam.size()) == beam_width) break;
    }
    return true;
  }

  const Score& best_score() const {
    assert(!beam.empty());
    return nodes[beam.front()].score;
  }

  vector<Action> restore(int rank = 0) const {
    assert(0 <= rank && rank < static_cast<int>(beam.size()));
    int node = beam[rank];
    vector<Action> actions;
    while (nodes[node].parent != -1) {
      actions.push_back(*nodes[node].action);
      node = nodes[node].parent;
    }
    reverse(actions.begin(), actions.end());
    return actions;
  }

  template <class Apply, class Revert>
  void move_to(int target, Apply& apply, Revert& revert) {
    int from = current_node;
    int to = target;
    vector<int> path_down;

    while (nodes[from].depth > nodes[to].depth) {
      revert(state, *nodes[from].action);
      from = nodes[from].parent;
    }
    while (nodes[to].depth > nodes[from].depth) {
      path_down.push_back(to);
      to = nodes[to].parent;
    }
    while (from != to) {
      revert(state, *nodes[from].action);
      from = nodes[from].parent;
      path_down.push_back(to);
      to = nodes[to].parent;
    }

    reverse(path_down.begin(), path_down.end());
    for (int node : path_down) apply(state, *nodes[node].action);
    current_node = target;
  }
};

constexpr int N = 30;
constexpr int V = N * (N + 1) / 2;
constexpr long long ERROR_UNIT = 1'000'000'000'000LL;
constexpr long long WEIGHT_UNIT = 1'000'000LL;

struct SwapMove {
  int parent;
  int child;
};

struct PyramidState {
  array<int, V> ball{};
  int errors = 0;
  long long violation_weight = 0;
  long long height_score = 0;
  uint64_t hash = 0;
};

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  array<int, V> row{};
  array<int, V> column{};
  vector<pair<int, int>> edges;
  array<vector<int>, V> incident_edges;

  auto id = [](int x, int y) { return x * (x + 1) / 2 + y; };

  for (int x = 0; x < N; ++x) {
    for (int y = 0; y <= x; ++y) {
      const int vertex = id(x, y);
      row[vertex] = x;
      column[vertex] = y;
    }
  }

  for (int x = 0; x + 1 < N; ++x) {
    for (int y = 0; y <= x; ++y) {
      for (int next_y : {y, y + 1}) {
        const int parent = id(x, y);
        const int child = id(x + 1, next_y);
        const int edge = static_cast<int>(edges.size());
        edges.push_back({parent, child});
        incident_edges[parent].push_back(edge);
        incident_edges[child].push_back(edge);
      }
    }
  }

  PyramidState initial;
  vector<int> initial_values(V);
  for (int x = 0; x < N; ++x) {
    for (int y = 0; y <= x; ++y) {
      const int vertex = id(x, y);
      cin >> initial.ball[vertex];
      initial_values[vertex] = initial.ball[vertex];
      initial.height_score += 1LL * initial.ball[vertex] * x;
    }
  }

  auto edge_error = [&](const PyramidState& state, int edge) {
    const auto [parent, child] = edges[edge];
    return state.ball[parent] > state.ball[child] ? 1 : 0;
  };

  auto edge_weight = [&](const PyramidState& state, int edge) {
    const auto [parent, child] = edges[edge];
    return max(0, state.ball[parent] - state.ball[child]);
  };

  for (int edge = 0; edge < static_cast<int>(edges.size()); ++edge) {
    initial.errors += edge_error(initial, edge);
    initial.violation_weight += edge_weight(initial, edge);
  }

  ZobristHash zobrist(V, V, 20230625);
  initial.hash = zobrist.build(initial_values);

  auto affected_edges = [&](int first, int second) {
    array<int, 12> result{};
    int count = 0;
    for (int vertex : {first, second}) {
      for (int edge : incident_edges[vertex]) {
        bool found = false;
        for (int i = 0; i < count; ++i) found |= result[i] == edge;
        if (!found) result[count++] = edge;
      }
    }
    return pair{result, count};
  };

  auto apply = [&](PyramidState& state, const SwapMove& action) {
    const auto [affected, count] =
        affected_edges(action.parent, action.child);

    for (int i = 0; i < count; ++i) {
      state.errors -= edge_error(state, affected[i]);
      state.violation_weight -= edge_weight(state, affected[i]);
    }

    const int parent_value = state.ball[action.parent];
    const int child_value = state.ball[action.child];
    zobrist.change(
        state.hash, action.parent, parent_value, child_value);
    zobrist.change(
        state.hash, action.child, child_value, parent_value);
    swap(state.ball[action.parent], state.ball[action.child]);
    state.height_score +=
        1LL * (parent_value - child_value) *
        (row[action.child] - row[action.parent]);

    for (int i = 0; i < count; ++i) {
      state.errors += edge_error(state, affected[i]);
      state.violation_weight += edge_weight(state, affected[i]);
    }
  };

  auto evaluate = [](const PyramidState& state) {
    return -ERROR_UNIT * state.errors -
           WEIGHT_UNIT * state.violation_weight + state.height_score;
  };

  auto error_delta_after_swap = [&](const PyramidState& state,
                                    int parent,
                                    int child) {
    const auto [affected, count] = affected_edges(parent, child);
    int old_errors = 0;
    int new_errors = 0;
    long long old_weight = 0;
    long long new_weight = 0;

    auto value_after = [&](int vertex) {
      if (vertex == parent) return state.ball[child];
      if (vertex == child) return state.ball[parent];
      return state.ball[vertex];
    };

    for (int i = 0; i < count; ++i) {
      const auto [edge_parent, edge_child] = edges[affected[i]];
      old_errors += state.ball[edge_parent] > state.ball[edge_child];
      old_weight +=
          max(0, state.ball[edge_parent] - state.ball[edge_child]);

      const int after_parent = value_after(edge_parent);
      const int after_child = value_after(edge_child);
      new_errors += after_parent > after_child;
      new_weight += max(0, after_parent - after_child);
    }
    return pair{new_errors - old_errors, new_weight - old_weight};
  };

  constexpr int BRANCH_WIDTH = 6;
  auto expand = [&](const PyramidState& state) {
    struct RatedMove {
      int errors;
      long long violation_weight;
      long long height_gain;
      SwapMove action;
    };

    vector<RatedMove> rated;
    rated.reserve(edges.size());

    for (const auto& [parent, child] : edges) {
      if (state.ball[parent] < state.ball[child]) continue;
      const auto [error_delta, weight_delta] =
          error_delta_after_swap(state, parent, child);
      rated.push_back({state.errors + error_delta,
                       state.violation_weight + weight_delta,
                       state.ball[parent] - state.ball[child],
                       {parent, child}});
    }

    auto better = [](const RatedMove& a, const RatedMove& b) {
      if (a.errors != b.errors) return a.errors < b.errors;
      if (a.violation_weight != b.violation_weight) {
        return a.violation_weight < b.violation_weight;
      }
      return a.height_gain > b.height_gain;
    };

    if (static_cast<int>(rated.size()) > BRANCH_WIDTH) {
      nth_element(
          rated.begin(), rated.begin() + BRANCH_WIDTH, rated.end(), better);
      rated.resize(BRANCH_WIDTH);
    }
    sort(rated.begin(), rated.end(), better);

    vector<SwapMove> actions;
    actions.reserve(rated.size());
    for (const RatedMove& move : rated) actions.push_back(move.action);
    return actions;
  };

  constexpr int BEAM_WIDTH = 4;
  constexpr int MAX_OPERATIONS = 10000;
  constexpr double SEARCH_TIME_MS = 1800.0;

  TreeBeamSearch<PyramidState, SwapMove, long long> beam(
      initial, evaluate(initial), BEAM_WIDTH);
  Timer timer;

  long long best_score = evaluate(initial);
  vector<SwapMove> best_actions;

  for (int turn = 0; turn < MAX_OPERATIONS; ++turn) {
    if (best_score >= 0 || timer.is_over(SEARCH_TIME_MS)) break;

    const bool advanced = beam.step_with_key(
        expand,
        apply,
        apply,
        evaluate,
        [](const PyramidState& state) { return state.hash; });
    if (!advanced) break;

    if (beam.best_score() > best_score) {
      best_score = beam.best_score();
      best_actions = beam.restore();
    }
  }

  assert(static_cast<int>(best_actions.size()) <= MAX_OPERATIONS);

  PyramidState check = initial;
  for (const SwapMove& action : best_actions) apply(check, action);
  assert(evaluate(check) == best_score);

  cout << best_actions.size() << '\n';
  for (const SwapMove& action : best_actions) {
    cout << row[action.parent] << ' ' << column[action.parent] << ' '
         << row[action.child] << ' ' << column[action.child] << '\n';
  }
}
