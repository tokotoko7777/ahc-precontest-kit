#include <bits/stdc++.h>
using namespace std;

// library/timer.hpp
struct Timer {
  chrono::steady_clock::time_point start;

  Timer() : start(chrono::steady_clock::now()) {}

  double elapsed_ms() const {
    const auto now = chrono::steady_clock::now();
    return chrono::duration<double, milli>(now - start).count();
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

  double next_double() {
    return uniform_real_distribution<double>(0.0, 1.0)(engine);
  }
};

struct Target {
  string text;
  int frequency = 0;
  int covered_weight = 0;
  int overlap_strength = 0;
  bool maximal = true;
};

// library/sequence-overlap.hpp
template <class Sequence>
int suffix_prefix_overlap(const Sequence& first, const Sequence& second) {
  const int limit = static_cast<int>(min(first.size(), second.size()));
  for (int length = limit; length >= 1; --length) {
    bool same = true;
    for (int index = 0; index < length; ++index) {
      if (first[first.size() - length + index] != second[index]) {
        same = false;
        break;
      }
    }
    if (same) return length;
  }
  return 0;
}

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  int n, m;
  cin >> n >> m;
  vector<string> input_strings(m);
  map<string, int> frequency;
  uint64_t input_hash = 1469598103934665603ULL;
  for (string& text : input_strings) {
    cin >> text;
    ++frequency[text];
    for (char letter : text) {
      input_hash ^= static_cast<unsigned char>(letter);
      input_hash *= 1099511628211ULL;
    }
  }
  Timer timer;
  constexpr double GREEDY_END_MS = 650.0;
  constexpr double SEARCH_END_MS = 2780.0;

  vector<Target> targets;
  targets.reserve(frequency.size());
  for (const auto& [text, count] : frequency) {
    targets.push_back({text, count});
  }

  const int target_count = static_cast<int>(targets.size());
  for (int i = 0; i < target_count; ++i) {
    for (int j = 0; j < target_count; ++j) {
      if (i == j) continue;
      if (targets[i].text.size() < targets[j].text.size() &&
          targets[j].text.find(targets[i].text) != string::npos) {
        targets[i].maximal = false;
      }
    }
  }

  vector<int> maximal_indices;
  for (int i = 0; i < target_count; ++i) {
    if (!targets[i].maximal) continue;
    maximal_indices.push_back(i);
    for (int j = 0; j < target_count; ++j) {
      if (targets[i].text.find(targets[j].text) != string::npos) {
        targets[i].covered_weight += targets[j].frequency;
      }
    }
  }

  for (int i : maximal_indices) {
    array<int, 3> largest{};
    for (int j : maximal_indices) {
      if (i == j) continue;
      const int overlap = max(
          suffix_prefix_overlap(targets[i].text, targets[j].text),
          suffix_prefix_overlap(targets[j].text, targets[i].text));
      if (overlap > largest[0]) {
        largest[0] = overlap;
        sort(largest.begin(), largest.end());
      }
    }
    targets[i].overlap_strength = largest[0] + largest[1] + largest[2];
  }

  using Board = vector<string>;

  const auto is_present = [&](const Board& board, const string& text) {
    const int length = static_cast<int>(text.size());
    for (int direction = 0; direction < 2; ++direction) {
      for (int line = 0; line < n; ++line) {
        for (int start = 0; start < n; ++start) {
          bool same = true;
          for (int offset = 0; offset < length; ++offset) {
            const int row = direction == 0 ? line : (start + offset) % n;
            const int column = direction == 0 ? (start + offset) % n : line;
            if (board[row][column] != text[offset]) {
              same = false;
              break;
            }
          }
          if (same) return true;
        }
      }
    }
    return false;
  };

  const auto count_score = [&](const Board& board) {
    int covered = 0;
    for (const Target& target : targets) {
      if (is_present(board, target.text)) covered += target.frequency;
    }
    int empty = 0;
    for (const string& row : board) {
      empty += count(row.begin(), row.end(), '.');
    }
    return pair<int, int>{covered, empty};
  };

  Random random(input_hash);
  Board best_board(n, string(n, '.'));
  pair<int, int> best_score{-1, -1};
  int attempt = 0;

  do {
    Board board(n, string(n, '.'));
    vector<int> order = maximal_indices;
    vector<uint64_t> priority(target_count);
    for (int index : order) {
      const uint64_t base =
          1000000ULL * targets[index].text.size() +
          5000ULL * targets[index].covered_weight +
          3000ULL * targets[index].overlap_strength;
      const uint64_t noise =
          attempt == 0 ? 0 : random.next_u64() % 2500000ULL;
      priority[index] = base + noise;
    }
    sort(order.begin(), order.end(), [&](int first, int second) {
      return priority[first] > priority[second];
    });

    vector<int> remaining;
    remaining.reserve(target_count);
    for (int i = 0; i < target_count; ++i) {
      if (!targets[i].maximal) remaining.push_back(i);
    }
    sort(remaining.begin(), remaining.end(), [&](int first, int second) {
      if (targets[first].frequency != targets[second].frequency) {
        return targets[first].frequency > targets[second].frequency;
      }
      return targets[first].text.size() > targets[second].text.size();
    });
    order.insert(order.end(), remaining.begin(), remaining.end());

    for (int target_index : order) {
      const string& text = targets[target_index].text;
      if (is_present(board, text)) continue;

      long long best_quality = numeric_limits<long long>::min();
      int best_direction = -1;
      int best_line = -1;
      int best_start = -1;
      int equal_candidates = 0;

      for (int direction = 0; direction < 2; ++direction) {
        for (int line = 0; line < n; ++line) {
          int line_filled = 0;
          for (int position = 0; position < n; ++position) {
            const int row = direction == 0 ? line : position;
            const int column = direction == 0 ? position : line;
            line_filled += board[row][column] != '.';
          }

          for (int start = 0; start < n; ++start) {
            bool compatible = true;
            int matching = 0;
            int new_cells = 0;
            for (int offset = 0; offset < static_cast<int>(text.size());
                 ++offset) {
              const int row = direction == 0 ? line : (start + offset) % n;
              const int column =
                  direction == 0 ? (start + offset) % n : line;
              const char current = board[row][column];
              if (current == '.') {
                ++new_cells;
              } else if (current == text[offset]) {
                ++matching;
              } else {
                compatible = false;
                break;
              }
            }
            if (!compatible) continue;

            long long quality = 1000000LL * matching - 10000LL * new_cells;
            if (matching == 0) {
              quality -= 100LL * line_filled;
            } else {
              quality += 10LL * line_filled;
            }

            if (quality > best_quality) {
              best_quality = quality;
              best_direction = direction;
              best_line = line;
              best_start = start;
              equal_candidates = 1;
            } else if (quality == best_quality) {
              ++equal_candidates;
              if (random.next_int(0, equal_candidates) == 0) {
                best_direction = direction;
                best_line = line;
                best_start = start;
              }
            }
          }
        }
      }

      if (best_direction == -1) continue;
      for (int offset = 0; offset < static_cast<int>(text.size()); ++offset) {
        const int row = best_direction == 0
                            ? best_line
                            : (best_start + offset) % n;
        const int column = best_direction == 0
                               ? (best_start + offset) % n
                               : best_line;
        board[row][column] = text[offset];
      }
    }

    const pair<int, int> score = count_score(board);
    if (score > best_score) {
      best_score = score;
      best_board = move(board);
    }
    ++attempt;
  } while (timer.elapsed_ms() < GREEDY_END_MS);

  // 各入力文字列が盤面の何か所に現れているかを数える。
  // 1マスを書き換えた時は、そのマスを通る巡回部分列だけを更新すればよい。
  vector<unordered_map<uint64_t, int>> target_by_code(13);
  for (int length = 2; length <= 12; ++length) {
    target_by_code[length].reserve(target_count * 2);
  }
  for (int i = 0; i < target_count; ++i) {
    uint64_t code = 0;
    for (char letter : targets[i].text) {
      code = (code << 3) | static_cast<uint64_t>(letter - 'A');
    }
    target_by_code[targets[i].text.size()][code] = i;
  }

  struct Window {
    int direction;
    int line;
    int start;
    int length;
  };
  vector<Window> windows;
  windows.reserve(2 * n * n * 11);
  for (int direction = 0; direction < 2; ++direction) {
    for (int line = 0; line < n; ++line) {
      for (int start = 0; start < n; ++start) {
        for (int length = 2; length <= 12; ++length) {
          windows.push_back({direction, line, start, length});
        }
      }
    }
  }

  const auto window_id = [&](int direction, int line, int start, int length) {
    return (((direction * n + line) * n + start) * 11 + (length - 2));
  };

  const auto window_target = [&](const Board& board, const Window& window) {
    uint64_t code = 0;
    for (int offset = 0; offset < window.length; ++offset) {
      const int row = window.direction == 0
                          ? window.line
                          : (window.start + offset) % n;
      const int column = window.direction == 0
                             ? (window.start + offset) % n
                             : window.line;
      const char letter = board[row][column];
      if (letter == '.') return -1;
      code = (code << 3) | static_cast<uint64_t>(letter - 'A');
    }
    const auto found = target_by_code[window.length].find(code);
    return found == target_by_code[window.length].end() ? -1
                                                        : found->second;
  };

  Board current_board = best_board;
  vector<int> occurrence_count(target_count, 0);
  for (const Window& window : windows) {
    const int index = window_target(current_board, window);
    if (index >= 0) ++occurrence_count[index];
  }
  int current_covered = 0;
  for (int i = 0; i < target_count; ++i) {
    if (occurrence_count[i] > 0) current_covered += targets[i].frequency;
  }
  assert(current_covered == best_score.first);

  vector<int> seen_window(windows.size(), 0);
  int stamp = 0;
  while (timer.elapsed_ms() < SEARCH_END_MS && current_covered < m) {
    int chosen_target = -1;
    for (int trial = 0; trial < 32; ++trial) {
      const int candidate = random.next_int(0, target_count);
      if (occurrence_count[candidate] == 0) {
        chosen_target = candidate;
        break;
      }
    }
    if (chosen_target == -1) {
      const int first = random.next_int(0, target_count);
      for (int offset = 0; offset < target_count; ++offset) {
        const int candidate = (first + offset) % target_count;
        if (occurrence_count[candidate] == 0) {
          chosen_target = candidate;
          break;
        }
      }
    }
    if (chosen_target == -1) break;

    const string& text = targets[chosen_target].text;
    long long best_placement_quality = numeric_limits<long long>::min();
    int chosen_direction = 0;
    int chosen_line = 0;
    int chosen_start = 0;
    for (int trial = 0; trial < 72; ++trial) {
      const int direction = random.next_int(0, 2);
      const int line = random.next_int(0, n);
      const int start = random.next_int(0, n);
      int same = 0;
      int empty = 0;
      int conflict = 0;
      for (int offset = 0; offset < static_cast<int>(text.size()); ++offset) {
        const int row = direction == 0 ? line : (start + offset) % n;
        const int column = direction == 0 ? (start + offset) % n : line;
        const char current = current_board[row][column];
        if (current == text[offset]) ++same;
        else if (current == '.') ++empty;
        else ++conflict;
      }
      const long long quality = 1000LL * same + 80LL * empty - 250LL * conflict;
      if (quality > best_placement_quality ||
          (quality == best_placement_quality && random.next_int(0, 2) == 0)) {
        best_placement_quality = quality;
        chosen_direction = direction;
        chosen_line = line;
        chosen_start = start;
      }
    }

    struct CellChange {
      int row;
      int column;
      char before;
      char after;
    };
    vector<CellChange> changes;
    changes.reserve(text.size());
    for (int offset = 0; offset < static_cast<int>(text.size()); ++offset) {
      const int row = chosen_direction == 0
                          ? chosen_line
                          : (chosen_start + offset) % n;
      const int column = chosen_direction == 0
                             ? (chosen_start + offset) % n
                             : chosen_line;
      if (current_board[row][column] != text[offset]) {
        changes.push_back(
            {row, column, current_board[row][column], text[offset]});
      }
    }
    if (changes.empty()) continue;

    ++stamp;
    vector<int> affected_windows;
    affected_windows.reserve(changes.size() * 160);
    for (const CellChange& change : changes) {
      for (int length = 2; length <= 12; ++length) {
        if (target_by_code[length].empty()) continue;
        for (int offset = 0; offset < length; ++offset) {
          const int horizontal_start = (change.column - offset + n) % n;
          const int horizontal =
              window_id(0, change.row, horizontal_start, length);
          if (seen_window[horizontal] != stamp) {
            seen_window[horizontal] = stamp;
            affected_windows.push_back(horizontal);
          }

          const int vertical_start = (change.row - offset + n) % n;
          const int vertical =
              window_id(1, change.column, vertical_start, length);
          if (seen_window[vertical] != stamp) {
            seen_window[vertical] = stamp;
            affected_windows.push_back(vertical);
          }
        }
      }
    }

    vector<int> old_targets;
    old_targets.reserve(affected_windows.size());
    for (int id : affected_windows) {
      old_targets.push_back(window_target(current_board, windows[id]));
    }
    for (const CellChange& change : changes) {
      current_board[change.row][change.column] = change.after;
    }

    vector<int> new_targets;
    new_targets.reserve(affected_windows.size());
    int candidate_covered = current_covered;
    for (int position = 0;
         position < static_cast<int>(affected_windows.size()); ++position) {
      const int old_target = old_targets[position];
      const int new_target =
          window_target(current_board, windows[affected_windows[position]]);
      new_targets.push_back(new_target);
      if (old_target == new_target) continue;
      if (old_target >= 0 && --occurrence_count[old_target] == 0) {
        candidate_covered -= targets[old_target].frequency;
      }
      if (new_target >= 0 && occurrence_count[new_target]++ == 0) {
        candidate_covered += targets[new_target].frequency;
      }
    }

    const int improvement = candidate_covered - current_covered;
    const double progress =
        clamp((timer.elapsed_ms() - GREEDY_END_MS) /
                  (SEARCH_END_MS - GREEDY_END_MS),
              0.0, 1.0);
    const double temperature = 3.0 * pow(0.03 / 3.0, progress);
    const bool accept = improvement >= 0 ||
                        random.next_double() < exp(improvement / temperature);
    if (accept) {
      current_covered = candidate_covered;
      if (current_covered > best_score.first) {
        best_score = {current_covered, 0};
        best_board = current_board;
      }
    } else {
      for (int position = 0;
           position < static_cast<int>(affected_windows.size()); ++position) {
        const int old_target = old_targets[position];
        const int new_target = new_targets[position];
        if (old_target == new_target) continue;
        if (new_target >= 0) --occurrence_count[new_target];
        if (old_target >= 0) ++occurrence_count[old_target];
      }
      for (const CellChange& change : changes) {
        current_board[change.row][change.column] = change.before;
      }
    }
  }

  for (const string& row : best_board) cout << row << '\n';
}
