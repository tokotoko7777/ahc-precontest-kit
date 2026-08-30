#include <bits/stdc++.h>
using namespace std;

// library/batched-timer.hpp
struct BatchedTimer {
  double time_limit_ms;
  int check_interval;
  int calls_until_check = 0;
  bool over = false;
  double last_elapsed_ms = 0.0;
  chrono::steady_clock::time_point start;

  BatchedTimer(double time_limit_ms, int check_interval)
      : time_limit_ms(time_limit_ms),
        check_interval(check_interval),
        start(chrono::steady_clock::now()) {
    assert(time_limit_ms > 0.0);
    assert(check_interval > 0);
  }

  void reset() {
    calls_until_check = 0;
    over = false;
    last_elapsed_ms = 0.0;
    start = chrono::steady_clock::now();
  }

  double elapsed_ms() const {
    const auto now = chrono::steady_clock::now();
    return chrono::duration<double, milli>(now - start).count();
  }

  bool is_over() {
    if (over) return true;
    if (calls_until_check > 0) {
      --calls_until_check;
      return false;
    }
    calls_until_check = check_interval - 1;
    last_elapsed_ms = elapsed_ms();
    over = last_elapsed_ms >= time_limit_ms;
    return over;
  }

  double cached_progress() const {
    return clamp(last_elapsed_ms / time_limit_ms, 0.0, 1.0);
  }
};

// library/random.hpp
struct Random {
  mt19937_64 engine;

  explicit Random(uint64_t seed = 0) : engine(seed) {}

  uint64_t next_u64() { return engine(); }

  template <class Int>
  Int next_int(Int left, Int right) {
    assert(left < right);
    uniform_int_distribution<Int> distribution(left, right - 1);
    return distribution(engine);
  }

  template <class Real = double>
  Real next_real(Real left = Real(0), Real right = Real(1)) {
    assert(left < right);
    uniform_real_distribution<Real> distribution(left, right);
    return distribution(engine);
  }

  double next_double() { return next_real<double>(); }

  template <class T>
  void shuffle(vector<T>& values) {
    std::shuffle(values.begin(), values.end(), engine);
  }

  template <class T>
  T& choice(vector<T>& values) {
    assert(!values.empty());
    return values[next_int<size_t>(0, values.size())];
  }

  template <class T>
  const T& choice(const vector<T>& values) {
    assert(!values.empty());
    return values[next_int<size_t>(0, values.size())];
  }

  template <class Weight>
  int weighted_index(const vector<Weight>& weights) {
    assert(!weights.empty());
    long double total = 0.0L;
    for (const Weight& weight : weights) {
      assert(weight >= Weight(0));
      total += static_cast<long double>(weight);
    }
    assert(total > 0.0L);

    const long double target = next_real<long double>(0.0L, total);
    long double sum = 0.0L;
    for (int index = 0; index < static_cast<int>(weights.size()); ++index) {
      sum += static_cast<long double>(weights[index]);
      if (target < sum) return index;
    }
    return static_cast<int>(weights.size()) - 1;
  }
};

// library/simulated-annealing.hpp
struct SimulatedAnnealing {
  double start_temperature;
  double end_temperature;
  mt19937_64 engine;

  SimulatedAnnealing(
      double start_temperature,
      double end_temperature,
      uint64_t seed = 0)
      : start_temperature(start_temperature),
        end_temperature(end_temperature),
        engine(seed) {
    assert(start_temperature > 0.0);
    assert(end_temperature > 0.0);
  }

  double temperature(double progress) const {
    progress = clamp(progress, 0.0, 1.0);
    return start_temperature *
           pow(end_temperature / start_temperature, progress);
  }

  template <class Score>
  bool accept(Score improvement, double progress) {
    const double value = static_cast<double>(improvement);
    if (value >= 0.0) return true;
    const double probability = exp(value / temperature(progress));
    uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(engine) < probability;
  }
};

// library/best-keeper.hpp
template <class Score, class State>
struct BestKeeper {
  Score best_score;
  State best_state;
  bool maximize;

  BestKeeper(Score initial_score, State initial_state, bool maximize = true)
      : best_score(move(initial_score)),
        best_state(move(initial_state)),
        maximize(maximize) {}

  bool update(const Score& score, const State& state) {
    const bool better = maximize ? best_score < score : score < best_score;
    if (!better) return false;
    best_score = score;
    best_state = state;
    return true;
  }
};

constexpr int BOARD_SIZE = 50;
constexpr int CELL_COUNT = BOARD_SIZE * BOARD_SIZE;

struct Board {
  array<int, CELL_COUNT> tile{};
  array<int, CELL_COUNT> point{};
  array<array<int, 4>, CELL_COUNT> next{};
  array<int, CELL_COUNT> next_count{};

  Board() {
    for (int row = 0; row < BOARD_SIZE; ++row) {
      for (int column = 0; column < BOARD_SIZE; ++column) {
        const int cell = row * BOARD_SIZE + column;
        if (row > 0) next[cell][next_count[cell]++] = cell - BOARD_SIZE;
        if (row + 1 < BOARD_SIZE) {
          next[cell][next_count[cell]++] = cell + BOARD_SIZE;
        }
        if (column > 0) next[cell][next_count[cell]++] = cell - 1;
        if (column + 1 < BOARD_SIZE) {
          next[cell][next_count[cell]++] = cell + 1;
        }
      }
    }
  }
};

struct CandidateCell {
  int cell;
  int priority;
};

// start_cellから、まだ使っていないタイルだけを通る末尾を貪欲に作る。
// 戻り値にはstart_cellを含めない。
vector<int> grow_tail(
    const Board& board,
    int start_cell,
    array<unsigned char, CELL_COUNT>& used_tile,
    Random& random,
    int noise_width) {
  vector<int> tail;
  tail.reserve(CELL_COUNT);
  int current = start_cell;

  while (true) {
    array<CandidateCell, 4> candidates{};
    int candidate_count = 0;

    for (int index = 0; index < board.next_count[current]; ++index) {
      const int to = board.next[current][index];
      if (used_tile[board.tile[to]]) continue;

      // 1手進んだ後にも何方向へ進めるかを見る。
      used_tile[board.tile[to]] = 1;
      int onward_count = 0;
      int best_next_point = 0;
      for (int next_index = 0;
           next_index < board.next_count[to];
           ++next_index) {
        const int next_cell = board.next[to][next_index];
        if (used_tile[board.tile[next_cell]]) continue;
        ++onward_count;
        best_next_point = max(best_next_point, board.point[next_cell]);
      }
      used_tile[board.tile[to]] = 0;

      // 得点だけでなく、袋小路へ入りにくい候補を優先する。
      const int noise = static_cast<int>(random.next_u64() % noise_width);
      int priority = 4 * board.point[to] + 45 * onward_count +
                     best_next_point + noise;
      if (onward_count == 0) priority -= 600;
      candidates[candidate_count++] = {to, priority};
    }

    if (candidate_count == 0) break;
    int chosen = 0;
    for (int index = 1; index < candidate_count; ++index) {
      if (candidates[chosen].priority < candidates[index].priority) {
        chosen = index;
      }
    }

    current = candidates[chosen].cell;
    used_tile[board.tile[current]] = 1;
    tail.push_back(current);
  }

  return tail;
}

struct SegmentRepair {
  const Board& board;
  Random& random;
  array<unsigned char, CELL_COUNT>& used_tile;
  int target;
  int max_edges;
  int expansion_limit;
  int expansions = 0;
  int best_score;
  vector<int> route;
  vector<int> best_middle;

  SegmentRepair(
      const Board& board,
      Random& random,
      array<unsigned char, CELL_COUNT>& used_tile,
      int target,
      int max_edges,
      int expansion_limit,
      int old_score,
      vector<int> old_middle)
      : board(board),
        random(random),
        used_tile(used_tile),
        target(target),
        max_edges(max_edges),
        expansion_limit(expansion_limit),
        best_score(old_score),
        best_middle(move(old_middle)) {
    route.reserve(max_edges);
  }

  int manhattan_distance(int first, int second) const {
    return abs(first / BOARD_SIZE - second / BOARD_SIZE) +
           abs(first % BOARD_SIZE - second % BOARD_SIZE);
  }

  void search(int current, int used_edges, int score) {
    if (++expansions > expansion_limit) return;

    const int remaining = max_edges - used_edges;
    const int distance = manhattan_distance(current, target);
    if (distance > remaining) return;

    array<CandidateCell, 4> candidates{};
    int candidate_count = 0;

    for (int index = 0; index < board.next_count[current]; ++index) {
      const int to = board.next[current][index];
      if (to == target) {
        if (score > best_score) {
          best_score = score;
          best_middle = route;
        }
        continue;
      }
      if (used_edges + 1 >= max_edges) continue;
      if (used_tile[board.tile[to]]) continue;

      const int next_distance = manhattan_distance(to, target);
      const int noise = static_cast<int>(random.next_u64() % 120);
      const int priority =
          5 * board.point[to] - 8 * next_distance + noise;
      candidates[candidate_count++] = {to, priority};
    }

    // 候補は最大4個なので、汎用sortより単純な交換の方が軽い。
    for (int first = 0; first < candidate_count; ++first) {
      for (int second = first + 1; second < candidate_count; ++second) {
        if (candidates[first].priority < candidates[second].priority) {
          swap(candidates[first], candidates[second]);
        }
      }
    }

    for (int index = 0; index < candidate_count; ++index) {
      if (expansions >= expansion_limit) break;
      const int to = candidates[index].cell;
      used_tile[board.tile[to]] = 1;
      route.push_back(to);
      search(to, used_edges + 1, score + board.point[to]);
      route.pop_back();
      used_tile[board.tile[to]] = 0;
    }
  }
};

int path_score(const Board& board, const vector<int>& path) {
  int score = 0;
  for (int cell : path) score += board.point[cell];
  return score;
}

array<unsigned char, CELL_COUNT> used_tiles(
    const Board& board,
    const vector<int>& path) {
  array<unsigned char, CELL_COUNT> used{};
  for (int cell : path) used[board.tile[cell]] = 1;
  return used;
}

vector<int> prefix_scores(const Board& board, const vector<int>& path) {
  vector<int> sum(path.size() + 1, 0);
  for (int index = 0; index < static_cast<int>(path.size()); ++index) {
    sum[index + 1] = sum[index] + board.point[path[index]];
  }
  return sum;
}

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  int start_row, start_column;
  cin >> start_row >> start_column;

  Board board;
  for (int cell = 0; cell < CELL_COUNT; ++cell) cin >> board.tile[cell];
  for (int cell = 0; cell < CELL_COUNT; ++cell) cin >> board.point[cell];
  const int start_cell = start_row * BOARD_SIZE + start_column;

  uint64_t seed = 0x9e3779b97f4a7c15ULL;
  for (int cell = 0; cell < CELL_COUNT; ++cell) {
    seed ^= static_cast<uint64_t>(board.tile[cell] * 101 + board.point[cell]);
    seed = seed * 0xbf58476d1ce4e5b9ULL + 0x94d049bb133111ebULL;
  }
  Random random(seed);
  BatchedTimer timer(1870.0, 16);

  // まず乱数を変えた貪欲解を多数作り、一番良いものを初期解にする。
  vector<int> current_path{start_cell};
  int current_score = board.point[start_cell];
  int initial_tries = 0;
  do {
    array<unsigned char, CELL_COUNT> used{};
    used[board.tile[start_cell]] = 1;
    vector<int> path{start_cell};
    const int noise_width = 80 + static_cast<int>(random.next_u64() % 321);
    vector<int> tail =
        grow_tail(board, start_cell, used, random, noise_width);
    path.insert(path.end(), tail.begin(), tail.end());
    const int score = path_score(board, path);
    if (score > current_score) {
      current_score = score;
      current_path = move(path);
    }
    ++initial_tries;
  } while (initial_tries < 24 || timer.elapsed_ms() < 170.0);

  BestKeeper<int, vector<int>> best(current_score, current_path);
  SimulatedAnnealing annealing(2400.0, 8.0, seed ^ 0x123456789abcdef0ULL);
  array<unsigned char, CELL_COUNT> current_used =
      used_tiles(board, current_path);
  vector<int> current_prefix = prefix_scores(board, current_path);
  int iteration = 0;

  while (!timer.is_over()) {
    if (current_path.size() == 1) break;
    const double progress = timer.cached_progress();
    vector<int> candidate_path;
    int candidate_score = current_score;

    // 末尾を大きく作り直す近傍。序盤ほど長い範囲を壊す。
    if (current_path.size() < 4 || random.next_u64() % 10 < 7) {
      const int path_size = static_cast<int>(current_path.size());
      const int changing_limit = min(
          path_size - 1,
          24 + static_cast<int>((1.0 - progress) * 650.0));
      const int removed =
          1 + static_cast<int>(random.next_u64() % changing_limit);
      const int cut = path_size - 1 - removed;

      array<unsigned char, CELL_COUNT> used = current_used;
      for (int index = cut + 1; index < path_size; ++index) {
        used[board.tile[current_path[index]]] = 0;
      }

      const int noise_width =
          45 + static_cast<int>((1.0 - progress) * 260.0);
      vector<int> new_tail =
          grow_tail(board, current_path[cut], used, random, noise_width);
      candidate_path.assign(current_path.begin(), current_path.begin() + cut + 1);
      candidate_path.insert(
          candidate_path.end(), new_tail.begin(), new_tail.end());
      candidate_score = current_prefix[cut + 1];
      for (int cell : new_tail) candidate_score += board.point[cell];
    } else {
      // 内部の短い区間を外し、両端を小さなDFSでつなぎ直す。
      const int path_size = static_cast<int>(current_path.size());
      const int max_span = min(
          path_size - 1,
          5 + static_cast<int>((1.0 - progress) * 13.0));
      const int span = 2 + static_cast<int>(
                               random.next_u64() % max(1, max_span - 1));
      const int left = static_cast<int>(
          random.next_u64() % (path_size - span));
      const int right = left + span;

      array<unsigned char, CELL_COUNT> used = current_used;
      vector<int> old_middle;
      int old_middle_score = 0;
      for (int index = left + 1; index < right; ++index) {
        const int cell = current_path[index];
        used[board.tile[cell]] = 0;
        old_middle.push_back(cell);
        old_middle_score += board.point[cell];
      }

      const int extra_edges =
          2 + static_cast<int>((1.0 - progress) * 8.0);
      const int edge_limit = min(30, span + extra_edges);
      const int expansion_limit =
          220 + static_cast<int>((1.0 - progress) * 650.0);
      SegmentRepair repair(
          board,
          random,
          used,
          current_path[right],
          edge_limit,
          expansion_limit,
          old_middle_score,
          old_middle);
      repair.search(current_path[left], 0, 0);

      candidate_path.assign(
          current_path.begin(), current_path.begin() + left + 1);
      candidate_path.insert(
          candidate_path.end(),
          repair.best_middle.begin(),
          repair.best_middle.end());
      candidate_path.insert(
          candidate_path.end(),
          current_path.begin() + right,
          current_path.end());
      candidate_score =
          current_score - old_middle_score + repair.best_score;
    }

    const int improvement = candidate_score - current_score;
    if (annealing.accept(improvement, progress)) {
      current_score = candidate_score;
      current_path = move(candidate_path);
      current_used = used_tiles(board, current_path);
      current_prefix = prefix_scores(board, current_path);
      best.update(current_score, current_path);
    }

    // 焼きなまし中の解が大きく崩れた時は、保存済みの最良解へ戻る。
    ++iteration;
    if ((iteration & 511) == 0 && current_score + 6000 < best.best_score) {
      current_score = best.best_score;
      current_path = best.best_state;
      current_used = used_tiles(board, current_path);
      current_prefix = prefix_scores(board, current_path);
    }
  }

  const vector<int>& answer = best.best_state;
  string moves;
  moves.reserve(answer.size() - 1);
  for (int index = 1; index < static_cast<int>(answer.size()); ++index) {
    const int difference = answer[index] - answer[index - 1];
    if (difference == -BOARD_SIZE) moves += 'U';
    if (difference == BOARD_SIZE) moves += 'D';
    if (difference == -1) moves += 'L';
    if (difference == 1) moves += 'R';
  }
  cout << moves << '\n';
}
