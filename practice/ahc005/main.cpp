#include <bits/stdc++.h>
using namespace std;

struct Point {
  int row;
  int column;
};

struct Solver {
  static constexpr int INF = 1'000'000'000;

  int n = 0;
  int start_row = 0;
  int start_column = 0;
  vector<string> grid;

  vector<Point> roads;
  vector<vector<int>> road_id;
  vector<vector<int>> next_road;
  vector<int> enter_cost;

  vector<int> horizontal_segment;
  vector<int> vertical_segment;
  vector<vector<int>> horizontal_members;
  vector<vector<int>> vertical_members;

  vector<vector<unsigned long long>> visible_cells;
  int bit_words = 0;

  // terminal 0 is always the starting square.
  vector<int> terminals;
  vector<vector<int>> distance_between_terminals;
  vector<vector<int>> shortest_path_parent;

  bool is_road(int row, int column) const {
    return 0 <= row && row < n && 0 <= column && column < n &&
           grid[row][column] != '#';
  }

  char move_letter(int from, int to) const {
    const int dr = roads[to].row - roads[from].row;
    const int dc = roads[to].column - roads[from].column;
    if (dr == -1) return 'U';
    if (dr == 1) return 'D';
    if (dc == -1) return 'L';
    return 'R';
  }

  void read_input() {
    cin >> n >> start_row >> start_column;
    grid.resize(n);
    for (string& row : grid) cin >> row;

    road_id.assign(n, vector<int>(n, -1));
    for (int row = 0; row < n; ++row) {
      for (int column = 0; column < n; ++column) {
        if (!is_road(row, column)) continue;
        road_id[row][column] = static_cast<int>(roads.size());
        roads.push_back({row, column});
        enter_cost.push_back(grid[row][column] - '0');
      }
    }

    const int road_count = static_cast<int>(roads.size());
    next_road.assign(road_count, {});
    const int dr[4] = {-1, 0, 1, 0};
    const int dc[4] = {0, 1, 0, -1};
    for (int id = 0; id < road_count; ++id) {
      for (int direction = 0; direction < 4; ++direction) {
        const int nr = roads[id].row + dr[direction];
        const int nc = roads[id].column + dc[direction];
        if (is_road(nr, nc)) next_road[id].push_back(road_id[nr][nc]);
      }
    }
  }

  // Give every maximal horizontal/vertical road its own segment number.
  void build_segments() {
    const int road_count = static_cast<int>(roads.size());
    horizontal_segment.assign(road_count, -1);
    vertical_segment.assign(road_count, -1);

    for (int row = 0; row < n; ++row) {
      int column = 0;
      while (column < n) {
        if (!is_road(row, column)) {
          ++column;
          continue;
        }
        horizontal_members.push_back({});
        const int segment = static_cast<int>(horizontal_members.size()) - 1;
        while (column < n && is_road(row, column)) {
          const int id = road_id[row][column];
          horizontal_segment[id] = segment;
          horizontal_members.back().push_back(id);
          ++column;
        }
      }
    }

    for (int column = 0; column < n; ++column) {
      int row = 0;
      while (row < n) {
        if (!is_road(row, column)) {
          ++row;
          continue;
        }
        vertical_members.push_back({});
        const int segment = static_cast<int>(vertical_members.size()) - 1;
        while (row < n && is_road(row, column)) {
          const int id = road_id[row][column];
          vertical_segment[id] = segment;
          vertical_members.back().push_back(id);
          ++row;
        }
      }
    }
  }

  void set_bit(vector<unsigned long long>& bits, int index) const {
    bits[index / 64] |= 1ULL << (index % 64);
  }

  bool has_bit(const vector<unsigned long long>& bits, int index) const {
    return (bits[index / 64] >> (index % 64)) & 1ULL;
  }

  void build_visibility_sets() {
    const int road_count = static_cast<int>(roads.size());
    bit_words = (road_count + 63) / 64;
    visible_cells.assign(
        road_count, vector<unsigned long long>(bit_words, 0ULL));

    for (int id = 0; id < road_count; ++id) {
      for (int other : horizontal_members[horizontal_segment[id]]) {
        set_bit(visible_cells[id], other);
      }
      for (int other : vertical_members[vertical_segment[id]]) {
        set_bit(visible_cells[id], other);
      }
    }
  }

  int uncovered_gain(const vector<unsigned long long>& covered,
                     int candidate) const {
    int gain = 0;
    for (int word = 0; word < bit_words; ++word) {
      gain += __builtin_popcountll(visible_cells[candidate][word] &
                                   ~covered[word]);
    }
    return gain;
  }

  bool all_bits_are_set(const vector<unsigned long long>& covered) const {
    const int road_count = static_cast<int>(roads.size());
    for (int id = 0; id < road_count; ++id) {
      if (!has_bit(covered, id)) return false;
    }
    return true;
  }

  // Greedy set cover: choose squares which see many still-unseen roads.
  vector<int> choose_observation_points() const {
    const int road_count = static_cast<int>(roads.size());
    const int start = road_id[start_row][start_column];
    vector<int> chosen = {start};
    vector<char> is_chosen(road_count, false);
    is_chosen[start] = true;
    vector<unsigned long long> covered = visible_cells[start];

    while (!all_bits_are_set(covered)) {
      int best = -1;
      int best_gain = -1;
      for (int candidate = 0; candidate < road_count; ++candidate) {
        if (is_chosen[candidate]) continue;
        const int gain = uncovered_gain(covered, candidate);
        if (gain > best_gain ||
            (gain == best_gain && best != -1 &&
             enter_cost[candidate] < enter_cost[best])) {
          best = candidate;
          best_gain = gain;
        }
      }

      // Every road sees itself, so this is only a defensive fallback.
      if (best == -1 || best_gain <= 0) break;
      chosen.push_back(best);
      is_chosen[best] = true;
      for (int word = 0; word < bit_words; ++word) {
        covered[word] |= visible_cells[best][word];
      }
    }

    // Greedy choices made early can become redundant after later choices.
    vector<int> cover_count(road_count, 0);
    for (int point : chosen) {
      for (int id = 0; id < road_count; ++id) {
        cover_count[id] += has_bit(visible_cells[point], id);
      }
    }
    for (int index = static_cast<int>(chosen.size()) - 1; index >= 1;
         --index) {
      const int point = chosen[index];
      bool removable = true;
      for (int id = 0; id < road_count; ++id) {
        if (has_bit(visible_cells[point], id) && cover_count[id] == 1) {
          removable = false;
          break;
        }
      }
      if (!removable) continue;
      for (int id = 0; id < road_count; ++id) {
        cover_count[id] -= has_bit(visible_cells[point], id);
      }
      chosen.erase(chosen.begin() + index);
    }
    return chosen;
  }

  bool build_shortest_paths() {
    const int terminal_count = static_cast<int>(terminals.size());
    const int road_count = static_cast<int>(roads.size());
    distance_between_terminals.assign(
        terminal_count, vector<int>(terminal_count, INF));
    shortest_path_parent.assign(
        terminal_count, vector<int>(road_count, -1));

    for (int source_index = 0; source_index < terminal_count;
         ++source_index) {
      vector<int> distance(road_count, INF);
      priority_queue<pair<int, int>, vector<pair<int, int>>,
                     greater<pair<int, int>>>
          queue;
      const int source = terminals[source_index];
      distance[source] = 0;
      queue.push({0, source});

      while (!queue.empty()) {
        const auto [current_distance, current] = queue.top();
        queue.pop();
        if (current_distance != distance[current]) continue;

        for (int next : next_road[current]) {
          const int new_distance = current_distance + enter_cost[next];
          if (new_distance >= distance[next]) continue;
          distance[next] = new_distance;
          shortest_path_parent[source_index][next] = current;
          queue.push({new_distance, next});
        }
      }

      for (int destination_index = 0; destination_index < terminal_count;
           ++destination_index) {
        distance_between_terminals[source_index][destination_index] =
            distance[terminals[destination_index]];
        if (distance[terminals[destination_index]] == INF) return false;
      }
    }
    return true;
  }

  vector<int> make_nearest_neighbor_order() const {
    const int terminal_count = static_cast<int>(terminals.size());
    vector<int> order = {0};
    vector<char> used(terminal_count, false);
    used[0] = true;
    while (static_cast<int>(order.size()) < terminal_count) {
      int best = -1;
      for (int candidate = 1; candidate < terminal_count; ++candidate) {
        if (used[candidate]) continue;
        if (best == -1 ||
            distance_between_terminals[order.back()][candidate] <
                distance_between_terminals[order.back()][best]) {
          best = candidate;
        }
      }
      order.push_back(best);
      used[best] = true;
    }
    return order;
  }

  // Moving into a cell costs that cell's digit. For two terminals a and b,
  // dist(a,b)-cost(b) equals dist(b,a)-cost(a), so this value is symmetric.
  // It lets ordinary 2-opt and relocate deltas work on this directed-looking
  // distance table.
  int symmetric_distance(int first, int second) const {
    return distance_between_terminals[first][second] -
           enter_cost[terminals[second]];
  }

  bool apply_best_two_opt(vector<int>& order) const {
    const int size = static_cast<int>(order.size());
    int best_delta = 0;
    int best_left = -1;
    int best_right = -1;

    for (int left = 1; left + 1 < size; ++left) {
      for (int right = left + 1; right < size; ++right) {
        const int a = order[left - 1];
        const int b = order[left];
        const int c = order[right];
        const int d = order[(right + 1) % size];
        const int delta = symmetric_distance(a, c) +
                          symmetric_distance(b, d) -
                          symmetric_distance(a, b) -
                          symmetric_distance(c, d);
        if (delta < best_delta) {
          best_delta = delta;
          best_left = left;
          best_right = right;
        }
      }
    }
    if (best_left == -1) return false;
    reverse(order.begin() + best_left, order.begin() + best_right + 1);
    return true;
  }

  bool apply_best_relocate(vector<int>& order) const {
    const int size = static_cast<int>(order.size());
    int best_delta = 0;
    int best_index = -1;
    int best_after = -1;

    for (int index = 1; index < size; ++index) {
      const int point = order[index];
      const int previous = order[index - 1];
      const int next = order[(index + 1) % size];
      const int remove_delta = symmetric_distance(previous, next) -
                               symmetric_distance(previous, point) -
                               symmetric_distance(point, next);

      for (int edge = 0; edge < size; ++edge) {
        if (edge == index || (edge + 1) % size == index) continue;
        const int before = order[edge];
        const int after = order[(edge + 1) % size];
        const int delta = remove_delta + symmetric_distance(before, point) +
                          symmetric_distance(point, after) -
                          symmetric_distance(before, after);
        if (delta < best_delta) {
          best_delta = delta;
          best_index = index;
          best_after = before;
        }
      }
    }

    if (best_index == -1) return false;
    const int point = order[best_index];
    order.erase(order.begin() + best_index);
    const int after_index = static_cast<int>(
        find(order.begin(), order.end(), best_after) - order.begin());
    order.insert(order.begin() + after_index + 1, point);
    return true;
  }

  void improve_visit_order(vector<int>& order) const {
    while (true) {
      bool changed = false;
      changed |= apply_best_two_opt(order);
      changed |= apply_best_relocate(order);
      if (!changed) break;
    }
  }

  bool mark_shortest_path_segments(int from_index, int to_index,
                                   vector<char>& seen_horizontal,
                                   vector<char>& seen_vertical) const {
    const int source = terminals[from_index];
    int current = terminals[to_index];
    while (true) {
      seen_horizontal[horizontal_segment[current]] = true;
      seen_vertical[vertical_segment[current]] = true;
      if (current == source) break;
      current = shortest_path_parent[from_index][current];
      if (current == -1) return false;
    }
    return true;
  }

  bool route_covers_every_road(const vector<int>& order) const {
    vector<char> seen_horizontal(horizontal_members.size(), false);
    vector<char> seen_vertical(vertical_members.size(), false);
    const int size = static_cast<int>(order.size());
    for (int index = 0; index < size; ++index) {
      if (!mark_shortest_path_segments(order[index],
                                       order[(index + 1) % size],
                                       seen_horizontal, seen_vertical)) {
        return false;
      }
    }
    for (int id = 0; id < static_cast<int>(roads.size()); ++id) {
      if (!seen_horizontal[horizontal_segment[id]] &&
          !seen_vertical[vertical_segment[id]]) {
        return false;
      }
    }
    return true;
  }

  // A shortest path often observes extra roads. Remove the waypoint whose
  // deletion saves most, but only after checking the complete visibility rule.
  void remove_unneeded_waypoints(vector<int>& order) const {
    while (order.size() > 1) {
      int best_position = -1;
      int best_saving = -1;
      for (int position = 1; position < static_cast<int>(order.size());
           ++position) {
        vector<int> trial = order;
        trial.erase(trial.begin() + position);
        if (!route_covers_every_road(trial)) continue;

        const int previous = order[position - 1];
        const int point = order[position];
        const int next = order[(position + 1) % order.size()];
        const int saving =
            distance_between_terminals[previous][point] +
            distance_between_terminals[point][next] -
            distance_between_terminals[previous][next];
        if (saving > best_saving) {
          best_saving = saving;
          best_position = position;
        }
      }
      if (best_position == -1) break;
      order.erase(order.begin() + best_position);
    }
  }

  bool append_shortest_path(int from_index, int to_index,
                            string& answer) const {
    const int source = terminals[from_index];
    int current = terminals[to_index];
    vector<int> reversed_path;
    while (current != source) {
      reversed_path.push_back(current);
      current = shortest_path_parent[from_index][current];
      if (current == -1) return false;
    }
    reversed_path.push_back(source);
    reverse(reversed_path.begin(), reversed_path.end());
    for (int index = 1; index < static_cast<int>(reversed_path.size());
         ++index) {
      answer.push_back(
          move_letter(reversed_path[index - 1], reversed_path[index]));
    }
    return true;
  }

  string make_answer(const vector<int>& order) const {
    string answer;
    for (int index = 0; index < static_cast<int>(order.size()); ++index) {
      if (!append_shortest_path(order[index],
                                order[(index + 1) % order.size()], answer)) {
        return {};
      }
    }
    return answer;
  }

  bool answer_is_valid(const string& answer) const {
    int row = start_row;
    int column = start_column;
    vector<unsigned long long> covered(bit_words, 0ULL);
    const auto observe = [&](int road, vector<unsigned long long>& bits) {
      for (int word = 0; word < bit_words; ++word) {
        bits[word] |= visible_cells[road][word];
      }
    };
    observe(road_id[row][column], covered);

    for (char move : answer) {
      if (move == 'U') --row;
      if (move == 'D') ++row;
      if (move == 'L') --column;
      if (move == 'R') ++column;
      if (!is_road(row, column)) return false;
      observe(road_id[row][column], covered);
    }
    return row == start_row && column == start_column &&
           all_bits_are_set(covered);
  }

  void dfs_fallback(int current, vector<char>& visited, string& answer) const {
    visited[current] = true;
    for (int next : next_road[current]) {
      if (visited[next]) continue;
      answer.push_back(move_letter(current, next));
      dfs_fallback(next, visited, answer);
      answer.push_back(move_letter(next, current));
    }
  }

  string make_safe_fallback() const {
    vector<char> visited(roads.size(), false);
    string answer;
    dfs_fallback(road_id[start_row][start_column], visited, answer);
    return answer;
  }

  string solve() {
    build_segments();
    build_visibility_sets();
    terminals = choose_observation_points();

    if (!build_shortest_paths()) return make_safe_fallback();
    vector<int> order = make_nearest_neighbor_order();
    improve_visit_order(order);

    // Do this after order optimization: the check also includes the exact
    // shortest paths used by the current order.
    remove_unneeded_waypoints(order);
    string answer = make_answer(order);
    if (!answer_is_valid(answer)) answer = make_safe_fallback();
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
