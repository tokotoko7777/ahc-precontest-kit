#include <bits/stdc++.h>
using namespace std;

struct Timer {
  chrono::steady_clock::time_point start = chrono::steady_clock::now();
  double elapsed_ms() const {
    return chrono::duration<double, milli>(chrono::steady_clock::now() - start)
        .count();
  }
};

struct Random {
  uint64_t state;
  explicit Random(uint64_t seed) : state(seed) {}
  uint64_t next() {
    state += 0x9e3779b97f4a7c15ULL;
    uint64_t value = state;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
  }
  int next_int(int limit) { return static_cast<int>(next() % limit); }
};

struct Cycle {
  vector<int> cells;
  bitset<900> used;
  // Only entries for used cells are read.
  array<int8_t, 900> rotation{};
  array<uint8_t, 900> ports{};
  int region = 0;
};

struct Solver {
  static constexpr int N = 30;
  static constexpr double END_MS = 1850.0;
  static constexpr int MAX_CANDIDATES_PER_REGION = 180;
  // Directions are left, up, right, down, matching the official statement.
  const int dr[4] = {0, -1, 0, 1};
  const int dc[4] = {-1, 0, 1, 0};
  const int rotate_tile[8] = {1, 2, 3, 0, 5, 4, 7, 6};
  const int to[8][4] = {{1, 0, -1, -1},
                        {3, -1, -1, 0},
                        {-1, -1, 3, 2},
                        {-1, 2, 1, -1},
                        {1, 0, 3, 2},
                        {3, 2, 1, 0},
                        {2, -1, 0, -1},
                        {-1, 3, -1, 1}};

  array<int, 900> tile{};
  vector<Cycle> candidates;
  Timer timer;
  Random random{1};
  array<char, 900> on_path{};
  vector<int> path;
  int start_cell = -1;
  int start_entry = -1;
  int expansion_count = 0;
  int expansion_limit = 0;
  int minimum_length = 4;
  int active_region = 0;
  vector<int> best_path_this_search;

  bool inside(int row, int column) const {
    return 0 <= row && row < N && 0 <= column && column < N;
  }
  bool is_curve(int cell) const { return tile[cell] <= 5; }
  int neighbor(int cell, int direction) const {
    int row = cell / N + dr[direction];
    int column = cell % N + dc[direction];
    if (!inside(row, column)) return -1;
    return row * N + column;
  }
  int direction_to(int from, int destination) const {
    const int row_difference = destination / N - from / N;
    const int column_difference = destination % N - from % N;
    if (column_difference == -1) return 0;
    if (row_difference == -1) return 1;
    if (column_difference == 1) return 2;
    return 3;
  }

  void read_input() {
    string row;
    uint64_t seed = 1469598103934665603ULL;
    for (int r = 0; r < N; ++r) {
      cin >> row;
      for (int c = 0; c < N; ++c) {
        tile[r * N + c] = row[c] - '0';
        seed ^= static_cast<unsigned char>(row[c]);
        seed *= 1099511628211ULL;
      }
    }
    random = Random(seed);
  }

  bool allowed_in_active_region(int cell) const {
    const int row = cell / N;
    const int column = cell % N;
    if (active_region == 0) return row < N / 2;
    if (active_region == 1) return row >= N / 2;
    if (active_region == 2) return column < N / 2;
    return column >= N / 2;
  }

  void trim_candidates() {
    // Keep memory and the final pair search bounded.
    vector<Cycle> kept;
    for (int region = 0; region < 4; ++region) {
      vector<Cycle> same_region;
      for (Cycle& cycle : candidates) {
        if (cycle.region == region) same_region.push_back(move(cycle));
      }
      sort(same_region.begin(), same_region.end(),
           [](const Cycle& a, const Cycle& b) {
             return a.cells.size() > b.cells.size();
           });
      if (static_cast<int>(same_region.size()) >
          MAX_CANDIDATES_PER_REGION) {
        same_region.resize(MAX_CANDIDATES_PER_REGION);
      }
      for (Cycle& cycle : same_region) kept.push_back(move(cycle));
    }
    candidates = move(kept);
  }

  void add_candidate(const vector<int>& cells, int region) {
    if (cells.size() < 4) return;
    Cycle cycle;
    cycle.cells = cells;
    cycle.region = region;
    for (int index = 0; index < static_cast<int>(cells.size()); ++index) {
      const int current = cells[index];
      const int previous = cells[(index + cells.size() - 1) % cells.size()];
      const int next = cells[(index + 1) % cells.size()];
      const int entry = direction_to(current, previous);
      const int exit = direction_to(current, next);
      cycle.used.set(current);
      cycle.rotation[current] = needed_rotation(current, entry, exit);
      cycle.ports[current] = static_cast<uint8_t>((1 << entry) | (1 << exit));
    }
    candidates.push_back(move(cycle));
    if (static_cast<int>(candidates.size()) >
        MAX_CANDIDATES_PER_REGION * 8) trim_candidates();
  }

  void add_small_safe_cycles() {
    // Four curve tiles in a 2x2 square can always form a length-4 loop.
    for (int row = 0; row + 1 < N; ++row) {
      for (int column = 0; column + 1 < N; ++column) {
        vector<int> square = {row * N + column, row * N + column + 1,
                              (row + 1) * N + column + 1,
                              (row + 1) * N + column};
        bool possible = true;
        for (int cell : square) possible &= is_curve(cell);
        if (!possible) continue;
        for (int region = 0; region < 4; ++region) {
          active_region = region;
          bool fits = true;
          for (int cell : square) fits &= allowed_in_active_region(cell);
          if (fits) add_candidate(square, region);
        }
      }
    }
  }

  void dfs(int current, int entry_direction) {
    if (++expansion_count > expansion_limit) return;
    if ((expansion_count & 255) == 0 && timer.elapsed_ms() >= END_MS) return;

    int exits[2];
    int exit_count = 0;
    if (is_curve(current)) {
      exits[exit_count++] = (entry_direction + 1) % 4;
      exits[exit_count++] = (entry_direction + 3) % 4;
      if (random.next_int(2)) swap(exits[0], exits[1]);
    } else {
      exits[exit_count++] = (entry_direction + 2) % 4;
    }

    for (int index = 0; index < exit_count; ++index) {
      const int exit_direction = exits[index];
      const int next = neighbor(current, exit_direction);
      if (next == -1 || !allowed_in_active_region(next)) continue;
      if (next == start_cell) {
        if (static_cast<int>(path.size()) >= minimum_length &&
            (exit_direction + 2) % 4 == start_entry &&
            path.size() > best_path_this_search.size()) {
          best_path_this_search = path;
        }
        continue;
      }
      if (on_path[next]) continue;

      on_path[next] = true;
      path.push_back(next);
      dfs(next, (exit_direction + 2) % 4);
      path.pop_back();
      on_path[next] = false;
      if (expansion_count > expansion_limit) return;
    }
  }

  void run_one_search() {
    // Regions 0/1 are the top/bottom halves, and 2/3 are left/right.
    active_region = random.next_int(4);
    do {
      start_cell = random.next_int(N * N);
    } while (!allowed_in_active_region(start_cell));
    int pairs[4][2];
    int pair_count = 0;
    if (is_curve(start_cell)) {
      for (int first = 0; first < 4; ++first) {
        pairs[pair_count][0] = first;
        pairs[pair_count][1] = (first + 1) % 4;
        ++pair_count;
      }
    } else {
      pairs[pair_count][0] = 0;
      pairs[pair_count++][1] = 2;
      pairs[pair_count][0] = 1;
      pairs[pair_count++][1] = 3;
    }
    int chosen = random.next_int(pair_count);
    int first_exit = pairs[chosen][0];
    start_entry = pairs[chosen][1];
    if (random.next_int(2)) swap(first_exit, start_entry);
    const int first_cell = neighbor(start_cell, first_exit);
    const int last_cell = neighbor(start_cell, start_entry);
    if (first_cell == -1 || last_cell == -1 || first_cell == last_cell) return;

    on_path.fill(false);
    on_path[start_cell] = true;
    on_path[first_cell] = true;
    path = {start_cell, first_cell};
    best_path_this_search.clear();
    expansion_count = 0;
    expansion_limit = 600 + random.next_int(2400);
    minimum_length = 4 + random.next_int(80);
    dfs(first_cell, (first_exit + 2) % 4);
    if (!best_path_this_search.empty()) {
      add_candidate(best_path_this_search, active_region);
    }
  }

  pair<int, int> choose_two_cycles() {
    trim_candidates();
    sort(candidates.begin(), candidates.end(), [](const Cycle& a,
                                                   const Cycle& b) {
      return a.cells.size() > b.cells.size();
    });
    long long best_product = -1;
    pair<int, int> answer{-1, -1};
    for (int first = 0; first < static_cast<int>(candidates.size()); ++first) {
      for (int second = first + 1;
           second < static_cast<int>(candidates.size()); ++second) {
        const long long product =
            static_cast<long long>(candidates[first].cells.size()) *
            candidates[second].cells.size();
        if (product <= best_product) break;
        bool compatible = true;
        bitset<900> shared =
            candidates[first].used & candidates[second].used;
        if (!shared.none()) {
          // Shared cells are safe only when two separate tracks of a double
          // curve tile are used with the same tile rotation.
          for (int cell = 0; cell < N * N && compatible; ++cell) {
            if (!shared[cell]) continue;
            if (tile[cell] < 4 || tile[cell] > 5 ||
                candidates[first].rotation[cell] !=
                    candidates[second].rotation[cell] ||
                (candidates[first].ports[cell] &
                 candidates[second].ports[cell]) != 0) {
              compatible = false;
            }
          }
        }
        if (compatible) {
          best_product = product;
          answer = {first, second};
          break;
        }
      }
    }
    return answer;
  }

  int needed_rotation(int cell, int entry, int exit) const {
    int state = tile[cell];
    for (int rotations = 0; rotations < 4; ++rotations) {
      if (to[state][entry] == exit) return rotations;
      state = rotate_tile[state];
    }
    return 0;
  }

  long long score_output(const string& answer) const {
    // This follows the cycle walk used by the official Rust visualizer.
    array<int, 900> state = tile;
    for (int cell = 0; cell < N * N; ++cell) {
      for (int count = 0; count < answer[cell] - '0'; ++count) {
        state[cell] = rotate_tile[state[cell]];
      }
    }

    vector<char> used(N * N * 4, false);
    vector<int> cycle_lengths;
    for (int start = 0; start < N * N; ++start) {
      for (int start_direction = 0; start_direction < 4;
           ++start_direction) {
        if (to[state[start]][start_direction] == -1 ||
            used[start * 4 + start_direction]) {
          continue;
        }
        int current = start;
        int entry = start_direction;
        int length = 0;
        while (!used[current * 4 + entry]) {
          const int exit = to[state[current]][entry];
          if (exit == -1) {
            current = -1;
            break;
          }
          used[current * 4 + entry] = true;
          used[current * 4 + exit] = true;
          ++length;
          current = neighbor(current, exit);
          if (current == -1) break;
          entry = (exit + 2) % 4;
        }
        if (current == start && entry == start_direction) {
          cycle_lengths.push_back(length);
        }
      }
    }
    if (cycle_lengths.size() < 2) return 0;
    nth_element(cycle_lengths.begin(), cycle_lengths.end() - 2,
                cycle_lengths.end());
    return static_cast<long long>(cycle_lengths.back()) *
           cycle_lengths[cycle_lengths.size() - 2];
  }

  string solve() {
    add_small_safe_cycles();
    while (timer.elapsed_ms() < END_MS) run_one_search();
    const auto [first, second] = choose_two_cycles();
    string answer(900, '0');
    for (int cycle_index : {first, second}) {
      if (cycle_index == -1) continue;
      const vector<int>& cells = candidates[cycle_index].cells;
      for (int index = 0; index < static_cast<int>(cells.size()); ++index) {
        const int current = cells[index];
        answer[current] = static_cast<char>(
            '0' + candidates[cycle_index].rotation[current]);
      }
    }
    const string zero_rotation(900, '0');
    if (score_output(zero_rotation) > score_output(answer)) {
      return zero_rotation;
    }
    return answer;
  }
};

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);
  Solver solver;
  solver.read_input();
  cout << solver.solve() << '\n';
}
