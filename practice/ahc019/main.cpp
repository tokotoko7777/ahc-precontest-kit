#include <bits/stdc++.h>
using namespace std;

#ifndef TIME_LIMIT_MS
#define TIME_LIMIT_MS 5300
#endif

struct Timer {
  chrono::steady_clock::time_point start = chrono::steady_clock::now();

  double elapsed_ms() const {
    return chrono::duration<double, milli>(chrono::steady_clock::now() - start)
        .count();
  }
};

struct Random {
  uint64_t state;

  explicit Random(uint64_t seed = 1) : state(seed) {}

  uint64_t next_u64() {
    state += 0x9e3779b97f4a7c15ULL;
    uint64_t value = state;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
  }

  int next_int(int limit) {
    return static_cast<int>(next_u64() % static_cast<uint64_t>(limit));
  }
};

struct Rotation {
  // result[dimension] = sign[dimension] * source[axis[dimension]]
  array<int, 3> axis{};
  array<int, 3> sign{};
};

struct Candidate {
  vector<unsigned short> cell[2];
  array<array<unsigned short, 14>, 2> front_mask{};
  array<array<unsigned short, 14>, 2> right_mask{};
  bool dead = false;
};

struct Block {
  vector<unsigned short> cell[2];
};

struct Solver {
  int size = 0;
  int volume = 0;
  array<array<string, 14>, 2> front{};
  array<array<string, 14>, 2> right{};
  vector<unsigned char> allowed[2];
  vector<unsigned char> occupied[2];
  array<array<unsigned short, 14>, 2> covered_front{};
  array<array<unsigned short, 14>, 2> covered_right{};
  vector<unsigned short> allowed_cells[2];
  vector<Rotation> rotations;
  vector<Candidate> candidates;
  vector<Block> blocks;
  Timer timer;
  Random random{1};

  int cell_id(int x, int y, int z) const {
    return x * size * size + y * size + z;
  }

  array<int, 3> position(int id) const {
    const int x = id / (size * size);
    const int y = id / size % size;
    const int z = id % size;
    return {x, y, z};
  }

  bool inside(const array<int, 3>& point) const {
    return 0 <= point[0] && point[0] < size && 0 <= point[1] &&
           point[1] < size && 0 <= point[2] && point[2] < size;
  }

  void read_input() {
    cin >> size;
    uint64_t seed = static_cast<uint64_t>(size) + 1469598103934665603ULL;
    for (int object = 0; object < 2; ++object) {
      for (int z = 0; z < size; ++z) cin >> front[object][z];
      for (int z = 0; z < size; ++z) cin >> right[object][z];
      for (int z = 0; z < size; ++z) {
        for (char value : front[object][z]) {
          seed ^= static_cast<unsigned char>(value);
          seed *= 1099511628211ULL;
        }
        for (char value : right[object][z]) {
          seed ^= static_cast<unsigned char>(value);
          seed *= 1099511628211ULL;
        }
      }
    }
    random = Random(seed);

    volume = size * size * size;
    for (int object = 0; object < 2; ++object) {
      allowed[object].assign(volume, false);
      occupied[object].assign(volume, false);
      for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y) {
          for (int z = 0; z < size; ++z) {
            const int id = cell_id(x, y, z);
            allowed[object][id] =
                front[object][z][x] == '1' && right[object][z][y] == '1';
            if (allowed[object][id]) {
              allowed_cells[object].push_back(static_cast<unsigned short>(id));
            }
          }
        }
      }
    }
  }

  static int permutation_sign(const array<int, 3>& permutation) {
    int inversions = 0;
    for (int i = 0; i < 3; ++i) {
      for (int j = i + 1; j < 3; ++j) {
        inversions += permutation[i] > permutation[j];
      }
    }
    return (inversions % 2 == 0) ? 1 : -1;
  }

  void make_rotations() {
    array<int, 3> permutation = {0, 1, 2};
    do {
      const int parity = permutation_sign(permutation);
      for (int sx : {-1, 1}) {
        for (int sy : {-1, 1}) {
          for (int sz : {-1, 1}) {
            if (parity * sx * sy * sz != 1) continue;
            rotations.push_back({permutation, {sx, sy, sz}});
          }
        }
      }
    } while (next_permutation(permutation.begin(), permutation.end()));
  }

  array<int, 3> rotate_point(const array<int, 3>& point,
                             const Rotation& rotation) const {
    array<int, 3> result{};
    for (int dimension = 0; dimension < 3; ++dimension) {
      result[dimension] = rotation.sign[dimension] *
                          point[rotation.axis[dimension]];
    }
    return result;
  }

  static uint64_t transform_key(int rotation_index,
                                const array<int, 3>& translation) {
    uint64_t key = static_cast<uint64_t>(rotation_index);
    for (int dimension = 0; dimension < 3; ++dimension) {
      key = (key << 7) |
            static_cast<uint64_t>(translation[dimension] + 32);
    }
    return key;
  }

  Candidate make_candidate(const vector<int>& component,
                           const vector<int>& mapped_cell) const {
    Candidate candidate;
    candidate.cell[0].reserve(component.size());
    candidate.cell[1].reserve(component.size());
    for (int first_id : component) {
      const int second_id = mapped_cell[first_id];
      candidate.cell[0].push_back(static_cast<unsigned short>(first_id));
      candidate.cell[1].push_back(static_cast<unsigned short>(second_id));

      const auto first = position(first_id);
      const auto second = position(second_id);
      candidate.front_mask[0][first[2]] |=
          static_cast<unsigned short>(1U << first[0]);
      candidate.right_mask[0][first[2]] |=
          static_cast<unsigned short>(1U << first[1]);
      candidate.front_mask[1][second[2]] |=
          static_cast<unsigned short>(1U << second[0]);
      candidate.right_mask[1][second[2]] |=
          static_cast<unsigned short>(1U << second[1]);
    }
    return candidate;
  }

  void add_transform(int rotation_index, const array<int, 3>& translation) {
    const Rotation& rotation = rotations[rotation_index];
    vector<int> mapped_cell(volume, -1);
    for (unsigned short short_id : allowed_cells[0]) {
      const int first_id = short_id;
      const array<int, 3> first = position(first_id);
      array<int, 3> second = rotate_point(first, rotation);
      for (int dimension = 0; dimension < 3; ++dimension) {
        second[dimension] += translation[dimension];
      }
      if (!inside(second)) continue;
      const int second_id = cell_id(second[0], second[1], second[2]);
      if (allowed[1][second_id]) mapped_cell[first_id] = second_id;
    }

    vector<unsigned char> visited(volume, false);
    vector<vector<int>> components;
    const int dx[6] = {1, -1, 0, 0, 0, 0};
    const int dy[6] = {0, 0, 1, -1, 0, 0};
    const int dz[6] = {0, 0, 0, 0, 1, -1};
    for (unsigned short short_id : allowed_cells[0]) {
      const int start_id = short_id;
      if (mapped_cell[start_id] == -1 || visited[start_id]) continue;
      vector<int> component;
      vector<int> stack = {start_id};
      visited[start_id] = true;
      while (!stack.empty()) {
        const int current_id = stack.back();
        stack.pop_back();
        component.push_back(current_id);
        const auto current = position(current_id);
        for (int direction = 0; direction < 6; ++direction) {
          const int nx = current[0] + dx[direction];
          const int ny = current[1] + dy[direction];
          const int nz = current[2] + dz[direction];
          if (nx < 0 || size <= nx || ny < 0 || size <= ny || nz < 0 ||
              size <= nz) {
            continue;
          }
          const int next_id = cell_id(nx, ny, nz);
          if (mapped_cell[next_id] == -1 || visited[next_id]) continue;
          visited[next_id] = true;
          stack.push_back(next_id);
        }
      }
      if (component.size() >= 2) components.push_back(move(component));
    }

    if (components.empty()) return;
    partial_sort(components.begin(),
                 components.begin() + min<int>(2, components.size()),
                 components.end(),
                 [](const vector<int>& lhs, const vector<int>& rhs) {
                   return lhs.size() > rhs.size();
                 });
    const int take = min<int>(2, components.size());
    for (int index = 0; index < take; ++index) {
      candidates.push_back(make_candidate(components[index], mapped_cell));
    }
  }

  unsigned short required_front_mask(int object, int z) const {
    unsigned short mask = 0;
    for (int x = 0; x < size; ++x) {
      if (front[object][z][x] == '1') {
        mask |= static_cast<unsigned short>(1U << x);
      }
    }
    return mask;
  }

  unsigned short required_right_mask(int object, int z) const {
    unsigned short mask = 0;
    for (int y = 0; y < size; ++y) {
      if (right[object][z][y] == '1') {
        mask |= static_cast<unsigned short>(1U << y);
      }
    }
    return mask;
  }

  int remaining_cells(
      int object, const array<unsigned short, 14>& extra_front,
      const array<unsigned short, 14>& extra_right) const {
    int result = 0;
    for (int z = 0; z < size; ++z) {
      const unsigned short front_done =
          static_cast<unsigned short>(covered_front[object][z] |
                                      extra_front[z]);
      const unsigned short right_done =
          static_cast<unsigned short>(covered_right[object][z] |
                                      extra_right[z]);
      const unsigned short missing_front = static_cast<unsigned short>(
          required_front_mask(object, z) & ~front_done);
      const unsigned short missing_right = static_cast<unsigned short>(
          required_right_mask(object, z) & ~right_done);
      result += max(__builtin_popcount(static_cast<unsigned int>(missing_front)),
                    __builtin_popcount(static_cast<unsigned int>(missing_right)));
    }
    return result;
  }

  int remaining_cells(int object) const {
    const array<unsigned short, 14> empty{};
    return remaining_cells(object, empty, empty);
  }

  void generate_candidates() {
    make_rotations();
    unordered_set<uint64_t> used_transform;
    used_transform.reserve(16384);

    auto try_transform = [&](int rotation_index,
                             const array<int, 3>& translation) {
      const uint64_t key = transform_key(rotation_index, translation);
      if (!used_transform.insert(key).second) return;
      add_transform(rotation_index, translation);
    };

    // Cube-centred transforms are cheap and include all 24 orientations.
    for (int rotation_index = 0;
         rotation_index < static_cast<int>(rotations.size());
         ++rotation_index) {
      array<int, 3> translation{};
      for (int dimension = 0; dimension < 3; ++dimension) {
        translation[dimension] =
            rotations[rotation_index].sign[dimension] == 1 ? 0 : size - 1;
      }
      try_transform(rotation_index, translation);
    }

    const double generation_end = TIME_LIMIT_MS * 0.52;
    int local_radius = 1;
    int attempts = 0;
    while (timer.elapsed_ms() < generation_end && candidates.size() < 9000) {
      const int rotation_index = random.next_int(rotations.size());
      array<int, 3> translation{};

      // Half of the attempts use an occupied-cell pair as anchors. This samples
      // broad translations while guaranteeing at least one overlap.
      if (attempts % 2 == 0) {
        const int first_id =
            allowed_cells[0][random.next_int(allowed_cells[0].size())];
        const int second_id =
            allowed_cells[1][random.next_int(allowed_cells[1].size())];
        const auto rotated =
            rotate_point(position(first_id), rotations[rotation_index]);
        const auto second = position(second_id);
        for (int dimension = 0; dimension < 3; ++dimension) {
          translation[dimension] = second[dimension] - rotated[dimension];
        }
      } else {
        for (int dimension = 0; dimension < 3; ++dimension) {
          const int centre =
              rotations[rotation_index].sign[dimension] == 1 ? 0 : size - 1;
          translation[dimension] =
              centre + random.next_int(2 * local_radius + 1) - local_radius;
        }
        if (attempts % 400 == 399 && local_radius < size - 1) ++local_radius;
      }
      try_transform(rotation_index, translation);
      ++attempts;
    }

    // Keep the most promising candidates. The rank deliberately includes both
    // objects instead of only the currently worse one, because their roles can
    // switch after a few blocks have been selected.
    const int base_remaining[2] = {remaining_cells(0), remaining_cells(1)};
    vector<pair<double, int>> order;
    order.reserve(candidates.size());
    for (int index = 0; index < static_cast<int>(candidates.size()); ++index) {
      const Candidate& candidate = candidates[index];
      const int after0 = remaining_cells(
          0, candidate.front_mask[0], candidate.right_mask[0]);
      const int after1 = remaining_cells(
          1, candidate.front_mask[1], candidate.right_mask[1]);
      const double rank = (base_remaining[0] - after0) +
                          (base_remaining[1] - after1) +
                          0.02 * log2(candidate.cell[0].size() + 1.0);
      order.push_back({rank, index});
    }
    const int keep = min<int>(5000, order.size());
    partial_sort(order.begin(), order.begin() + keep, order.end(),
                 greater<pair<double, int>>());
    vector<Candidate> reduced;
    reduced.reserve(keep);
    for (int index = 0; index < keep; ++index) {
      reduced.push_back(move(candidates[order[index].second]));
    }
    candidates = move(reduced);
  }

  bool overlaps(const Candidate& candidate) const {
    for (int index = 0; index < static_cast<int>(candidate.cell[0].size());
         ++index) {
      if (occupied[0][candidate.cell[0][index]] ||
          occupied[1][candidate.cell[1][index]]) {
        return true;
      }
    }
    return false;
  }

  vector<Candidate> split_into_free_components(
      const Candidate& candidate) const {
    vector<int> mapped_cell(volume, -1);
    for (int index = 0; index < static_cast<int>(candidate.cell[0].size());
         ++index) {
      const int first_id = candidate.cell[0][index];
      const int second_id = candidate.cell[1][index];
      if (!occupied[0][first_id] && !occupied[1][second_id]) {
        mapped_cell[first_id] = second_id;
      }
    }

    vector<unsigned char> visited(volume, false);
    vector<vector<int>> components;
    const int dx[6] = {1, -1, 0, 0, 0, 0};
    const int dy[6] = {0, 0, 1, -1, 0, 0};
    const int dz[6] = {0, 0, 0, 0, 1, -1};
    for (unsigned short short_id : candidate.cell[0]) {
      const int start_id = short_id;
      if (mapped_cell[start_id] == -1 || visited[start_id]) continue;
      vector<int> component;
      vector<int> stack = {start_id};
      visited[start_id] = true;
      while (!stack.empty()) {
        const int current_id = stack.back();
        stack.pop_back();
        component.push_back(current_id);
        const auto current = position(current_id);
        for (int direction = 0; direction < 6; ++direction) {
          const int nx = current[0] + dx[direction];
          const int ny = current[1] + dy[direction];
          const int nz = current[2] + dz[direction];
          if (nx < 0 || size <= nx || ny < 0 || size <= ny || nz < 0 ||
              size <= nz) {
            continue;
          }
          const int next_id = cell_id(nx, ny, nz);
          if (mapped_cell[next_id] == -1 || visited[next_id]) continue;
          visited[next_id] = true;
          stack.push_back(next_id);
        }
      }
      if (component.size() >= 2) components.push_back(move(component));
    }

    const int take = min<int>(3, components.size());
    partial_sort(components.begin(), components.begin() + take,
                 components.end(),
                 [](const vector<int>& lhs, const vector<int>& rhs) {
                   return lhs.size() > rhs.size();
                 });
    vector<Candidate> result;
    result.reserve(take);
    for (int index = 0; index < take; ++index) {
      result.push_back(make_candidate(components[index], mapped_cell));
    }
    return result;
  }

  void select_common_blocks(int forced_first_rank) {
    int remaining[2] = {remaining_cells(0), remaining_cells(1)};
    int iteration = 0;
    const double selection_end = TIME_LIMIT_MS * 0.80;
    while (timer.elapsed_ms() < selection_end && iteration < 80) {
      int best_index = -1;
      double best_gain = 1e-12;
      int best_after[2] = {0, 0};
      vector<Candidate> split_candidates;
      struct FirstOption {
        double gain;
        int index;
        int after0;
        int after1;
      };
      vector<FirstOption> first_options;
      for (int index = 0; index < static_cast<int>(candidates.size());
           ++index) {
        Candidate& candidate = candidates[index];
        if (candidate.dead) continue;
        if ((index & 255) == 0 && timer.elapsed_ms() >= selection_end) break;

        const int after0 = remaining_cells(
            0, candidate.front_mask[0], candidate.right_mask[0]);
        const int after1 = remaining_cells(
            1, candidate.front_mask[1], candidate.right_mask[1]);
        const double gain = max(remaining[0], remaining[1]) -
                            max(after0, after1) -
                            1.0 / candidate.cell[0].size();
        if (gain <= 1e-12) continue;
        if (overlaps(candidate)) {
          candidate.dead = true;
          if (candidates.size() + split_candidates.size() < 10000) {
            vector<Candidate> pieces = split_into_free_components(candidate);
            for (Candidate& piece : pieces) {
              split_candidates.push_back(move(piece));
            }
          }
          continue;
        }
        if (iteration == 0 && forced_first_rank >= 0) {
          first_options.push_back({gain, index, after0, after1});
          continue;
        }
        if (gain <= best_gain) continue;
        best_gain = gain;
        best_index = index;
        best_after[0] = after0;
        best_after[1] = after1;
      }
      for (Candidate& candidate : split_candidates) {
        candidates.push_back(move(candidate));
      }
      if (iteration == 0 && forced_first_rank >= 0 &&
          !first_options.empty()) {
        const int take =
            min<int>(forced_first_rank + 1, first_options.size());
        partial_sort(first_options.begin(), first_options.begin() + take,
                     first_options.end(),
                     [](const FirstOption& lhs, const FirstOption& rhs) {
                       if (lhs.gain != rhs.gain) return lhs.gain > rhs.gain;
                       return lhs.index < rhs.index;
                     });
        const FirstOption& chosen_option = first_options[take - 1];
        best_index = chosen_option.index;
        best_after[0] = chosen_option.after0;
        best_after[1] = chosen_option.after1;
      }
      if (best_index == -1) break;

      Candidate& chosen = candidates[best_index];
      Block block;
      for (int object = 0; object < 2; ++object) {
        for (int z = 0; z < size; ++z) {
          covered_front[object][z] |= chosen.front_mask[object][z];
          covered_right[object][z] |= chosen.right_mask[object][z];
        }
        block.cell[object] = chosen.cell[object];
        for (unsigned short id : block.cell[object]) {
          occupied[object][id] = true;
        }
      }
      blocks.push_back(move(block));
      chosen.dead = true;
      remaining[0] = best_after[0];
      remaining[1] = best_after[1];
      ++iteration;
    }
  }

  vector<unsigned short> make_filler_variant(int object, int variation) const {
    vector<unsigned short> filler;
    mt19937 generator(static_cast<unsigned int>(
        1234567 + object * 1000003 + variation * 9176));
    for (int z = 0; z < size; ++z) {
      vector<int> missing_x;
      vector<int> missing_y;
      vector<int> every_x;
      vector<int> every_y;
      for (int x = 0; x < size; ++x) {
        if (front[object][z][x] != '1') continue;
        every_x.push_back(x);
        if ((covered_front[object][z] >> x & 1U) == 0) {
          missing_x.push_back(x);
        }
      }
      for (int y = 0; y < size; ++y) {
        if (right[object][z][y] != '1') continue;
        every_y.push_back(y);
        if ((covered_right[object][z] >> y & 1U) == 0) {
          missing_y.push_back(y);
        }
      }
      if (variation != 0) {
        shuffle(missing_x.begin(), missing_x.end(), generator);
        shuffle(missing_y.begin(), missing_y.end(), generator);
        shuffle(every_x.begin(), every_x.end(), generator);
        shuffle(every_y.begin(), every_y.end(), generator);
      }

      vector<pair<int, int>> layer_cells;
      if (variation == 0) {
        const int count = max(missing_x.size(), missing_y.size());
        for (int index = 0; index < count; ++index) {
          const int x = missing_x.empty()
                            ? every_x[index % every_x.size()]
                            : missing_x[index % missing_x.size()];
          const int y = missing_y.empty()
                            ? every_y[index % every_y.size()]
                            : missing_y[index % missing_y.size()];
          layer_cells.push_back({x, y});
        }
      } else if (missing_x.size() >= missing_y.size()) {
        for (int index = 0; index < static_cast<int>(missing_x.size());
             ++index) {
          const int x = missing_x[index];
          int y = every_y[index % every_y.size()];
          if (index < static_cast<int>(missing_y.size())) y = missing_y[index];
          layer_cells.push_back({x, y});
        }
      } else {
        for (int index = 0; index < static_cast<int>(missing_y.size());
             ++index) {
          const int y = missing_y[index];
          int x = every_x[index % every_x.size()];
          if (index < static_cast<int>(missing_x.size())) x = missing_x[index];
          layer_cells.push_back({x, y});
        }
      }

      for (const auto& [x, y] : layer_cells) {
        const int id = cell_id(x, y, z);
        // A missing projection line has no occupied cell, so this is guaranteed
        // to be free. Keep the guard as a submission-safety check.
        if (occupied[object][id]) {
          for (int alternative_x : every_x) {
            for (int alternative_y : every_y) {
              const int alternative = cell_id(alternative_x, alternative_y, z);
              if (!occupied[object][alternative]) {
                filler.push_back(static_cast<unsigned short>(alternative));
                goto placed;
              }
            }
          }
        } else {
          filler.push_back(static_cast<unsigned short>(id));
        }
      placed:;
      }
    }
    return filler;
  }

  pair<vector<unsigned short>, vector<unsigned short>> make_fillers() const {
#ifdef UNIT_BASELINE
    return {make_filler_variant(0, 0), make_filler_variant(1, 0)};
#else
    struct FillerChoice {
      vector<unsigned short> cell;
      int dominoes = 0;
    };
    vector<FillerChoice> choice[2];
    for (int object = 0; object < 2; ++object) {
      // Keep one construction for every attainable matching size. Which side
      // should contain more dominoes depends on the other side, so maximizing
      // the two independently is not always optimal.
      array<int, 100> representative;
      representative.fill(-1);
      for (int variation = 0; variation < 80; ++variation) {
        vector<unsigned short> cell =
            make_filler_variant(object, variation);
        const int dominoes = domino_matching(cell).size();
        if (representative[dominoes] != -1) continue;
        representative[dominoes] = choice[object].size();
        choice[object].push_back({move(cell), dominoes});
      }
    }

    int best0 = 0;
    int best1 = 0;
    double best_cost = 1e100;
    for (int index0 = 0; index0 < static_cast<int>(choice[0].size());
         ++index0) {
      for (int index1 = 0; index1 < static_cast<int>(choice[1].size());
           ++index1) {
        const int match0 = choice[0][index0].dominoes;
        const int match1 = choice[1][index1].dominoes;
        for (int use0 = 0; use0 <= match0; ++use0) {
          for (int use1 = 0; use1 <= match1; ++use1) {
            const int left0 =
                choice[0][index0].cell.size() - 2 * use0;
            const int left1 =
                choice[1][index1].cell.size() - 2 * use1;
            const double cost =
                0.5 * (use0 + use1) + max(left0, left1);
            if (cost < best_cost) {
              best_cost = cost;
              best0 = index0;
              best1 = index1;
            }
          }
        }
      }
    }
    return {move(choice[0][best0].cell), move(choice[1][best1].cell)};
#endif
  }

  vector<pair<unsigned short, unsigned short>> domino_matching(
      const vector<unsigned short>& filler) const {
    vector<unsigned char> present(volume, false);
    for (unsigned short id : filler) present[id] = true;
    vector<int> matched_to(volume, -1);
    vector<int> visit_stamp(volume, 0);
    int stamp = 0;
    const int dx[6] = {1, -1, 0, 0, 0, 0};
    const int dy[6] = {0, 0, 1, -1, 0, 0};
    const int dz[6] = {0, 0, 0, 0, 1, -1};

    function<bool(int)> augment = [&](int even_id) {
      const auto current = position(even_id);
      for (int direction = 0; direction < 6; ++direction) {
        const int nx = current[0] + dx[direction];
        const int ny = current[1] + dy[direction];
        const int nz = current[2] + dz[direction];
        if (nx < 0 || size <= nx || ny < 0 || size <= ny || nz < 0 ||
            size <= nz) {
          continue;
        }
        const int odd_id = cell_id(nx, ny, nz);
        if (!present[odd_id] || visit_stamp[odd_id] == stamp) continue;
        visit_stamp[odd_id] = stamp;
        if (matched_to[odd_id] == -1 || augment(matched_to[odd_id])) {
          matched_to[odd_id] = even_id;
          return true;
        }
      }
      return false;
    };

    for (unsigned short short_id : filler) {
      const int id = short_id;
      const auto point = position(id);
      if ((point[0] + point[1] + point[2]) % 2 != 0) continue;
      ++stamp;
      augment(id);
    }

    vector<pair<unsigned short, unsigned short>> result;
    for (int odd_id = 0; odd_id < volume; ++odd_id) {
      if (matched_to[odd_id] != -1) {
        result.push_back({static_cast<unsigned short>(matched_to[odd_id]),
                          static_cast<unsigned short>(odd_id)});
      }
    }
    return result;
  }

  void append_filler_blocks(const vector<unsigned short>& first_filler,
                            const vector<unsigned short>& second_filler) {
    vector<unsigned char> used[2] = {vector<unsigned char>(volume, false),
                                     vector<unsigned char>(volume, false)};

#ifndef UNIT_BASELINE
    const auto first_dominoes = domino_matching(first_filler);
    const auto second_dominoes = domino_matching(second_filler);

    // If only one side has another required domino, place an additional domino
    // in any still-free legal position of the other object. Extra legal cells
    // do not alter a silhouette, and the formerly one-sided volume-2 block now
    // costs 1/2 instead of 2 in the official objective.
    vector<unsigned char> is_filler[2] = {
        vector<unsigned char>(volume, false),
        vector<unsigned char>(volume, false)};
    for (unsigned short id : first_filler) is_filler[0][id] = true;
    for (unsigned short id : second_filler) is_filler[1][id] = true;

    vector<unsigned short> free_cell[2];
    for (int object = 0; object < 2; ++object) {
      for (int id = 0; id < volume; ++id) {
        if (allowed[object][id] && !occupied[object][id] &&
            !is_filler[object][id]) {
          free_cell[object].push_back(static_cast<unsigned short>(id));
        }
      }
    }
    const auto free_dominoes0 = domino_matching(free_cell[0]);
    const auto free_dominoes1 = domino_matching(free_cell[1]);

    int use_first = 0;
    int use_second = 0;
    double best_cost = max(first_filler.size(), second_filler.size());
    for (int count0 = 0; count0 <= static_cast<int>(first_dominoes.size());
         ++count0) {
      for (int count1 = 0;
           count1 <= static_cast<int>(second_dominoes.size()); ++count1) {
        const int common = min(count0, count1);
        if (count0 - common > static_cast<int>(free_dominoes1.size()) ||
            count1 - common > static_cast<int>(free_dominoes0.size())) {
          continue;
        }
        const int left0 = first_filler.size() - 2 * count0;
        const int left1 = second_filler.size() - 2 * count1;
        const double cost = 0.5 * (count0 + count1) + max(left0, left1);
        if (cost < best_cost) {
          best_cost = cost;
          use_first = count0;
          use_second = count1;
        }
      }
    }

    const int common_dominoes = min(use_first, use_second);
    for (int index = 0; index < common_dominoes; ++index) {
      Block block;
      block.cell[0] = {first_dominoes[index].first,
                       first_dominoes[index].second};
      block.cell[1] = {second_dominoes[index].first,
                       second_dominoes[index].second};
      used[0][first_dominoes[index].first] = true;
      used[0][first_dominoes[index].second] = true;
      used[1][second_dominoes[index].first] = true;
      used[1][second_dominoes[index].second] = true;
      blocks.push_back(move(block));
    }

    const int add_for_first = use_first - common_dominoes;
    for (int offset = 0; offset < add_for_first; ++offset) {
      const int index = common_dominoes + offset;
      Block block;
      block.cell[0] = {first_dominoes[index].first,
                       first_dominoes[index].second};
      block.cell[1] = {free_dominoes1[offset].first,
                       free_dominoes1[offset].second};
      used[0][first_dominoes[index].first] = true;
      used[0][first_dominoes[index].second] = true;
      blocks.push_back(move(block));
    }

    const int add_for_second = use_second - common_dominoes;
    for (int offset = 0; offset < add_for_second; ++offset) {
      const int index = common_dominoes + offset;
      Block block;
      block.cell[0] = {free_dominoes0[offset].first,
                       free_dominoes0[offset].second};
      block.cell[1] = {second_dominoes[index].first,
                       second_dominoes[index].second};
      used[1][second_dominoes[index].first] = true;
      used[1][second_dominoes[index].second] = true;
      blocks.push_back(move(block));
    }
#endif

    vector<unsigned short> single[2];
    for (unsigned short id : first_filler) {
      if (!used[0][id]) single[0].push_back(id);
    }
    for (unsigned short id : second_filler) {
      if (!used[1][id]) single[1].push_back(id);
    }
    const int common_singles = min(single[0].size(), single[1].size());
    for (int index = 0; index < common_singles; ++index) {
      Block block;
      block.cell[0] = {single[0][index]};
      block.cell[1] = {single[1][index]};
      blocks.push_back(move(block));
    }
    for (int index = common_singles;
         index < static_cast<int>(single[0].size()); ++index) {
      Block block;
      block.cell[0] = {single[0][index]};
      blocks.push_back(move(block));
    }
    for (int index = common_singles;
         index < static_cast<int>(single[1].size()); ++index) {
      Block block;
      block.cell[1] = {single[1][index]};
      blocks.push_back(move(block));
    }
  }

  bool validate_locally() const {
    vector<int> label[2] = {vector<int>(volume), vector<int>(volume)};
    for (int block_index = 0; block_index < static_cast<int>(blocks.size());
         ++block_index) {
      for (int object = 0; object < 2; ++object) {
        for (unsigned short id : blocks[block_index].cell[object]) {
          if (!allowed[object][id] || label[object][id] != 0) return false;
          label[object][id] = block_index + 1;
        }
      }
    }
    for (int object = 0; object < 2; ++object) {
      for (int z = 0; z < size; ++z) {
        for (int x = 0; x < size; ++x) {
          bool seen = false;
          for (int y = 0; y < size; ++y) {
            seen |= label[object][cell_id(x, y, z)] != 0;
          }
          if (seen != (front[object][z][x] == '1')) return false;
        }
        for (int y = 0; y < size; ++y) {
          bool seen = false;
          for (int x = 0; x < size; ++x) {
            seen |= label[object][cell_id(x, y, z)] != 0;
          }
          if (seen != (right[object][z][y] == '1')) return false;
        }
      }
    }
    return true;
  }

  double objective_value() const {
    double result = 0.0;
    for (const Block& block : blocks) {
      if (block.cell[0].empty() || block.cell[1].empty()) {
        result += block.cell[0].size() + block.cell[1].size();
      } else {
        result += 1.0 / block.cell[0].size();
      }
    }
    return result;
  }

  void clear_construction() {
    blocks.clear();
    for (int object = 0; object < 2; ++object) {
      fill(covered_front[object].begin(), covered_front[object].end(), 0);
      fill(covered_right[object].begin(), covered_right[object].end(), 0);
      fill(occupied[object].begin(), occupied[object].end(), false);
    }
  }

  void finish_current_construction() {
    auto filler = make_fillers();
    append_filler_blocks(filler.first, filler.second);
  }

  void print_answer() const {
    vector<int> label[2] = {vector<int>(volume), vector<int>(volume)};
    for (int block_index = 0; block_index < static_cast<int>(blocks.size());
         ++block_index) {
      for (int object = 0; object < 2; ++object) {
        for (unsigned short id : blocks[block_index].cell[object]) {
          label[object][id] = block_index + 1;
        }
      }
    }
    cout << blocks.size() << '\n';
    for (int object = 0; object < 2; ++object) {
      for (int id = 0; id < volume; ++id) {
        if (id != 0) cout << ' ';
        cout << label[object][id];
      }
      cout << '\n';
    }
  }

  void solve() {
#ifndef UNIT_BASELINE
    generate_candidates();
    const vector<Candidate> original_candidates = candidates;
    vector<Block> best_blocks;
    double best_objective = 1e100;
    for (int trial = 0; trial < 16; ++trial) {
      if (trial > 0 && timer.elapsed_ms() >= TIME_LIMIT_MS * 0.70) break;
      clear_construction();
      candidates = original_candidates;
      select_common_blocks(trial);
      finish_current_construction();
      const double objective = objective_value();
      if (objective < best_objective) {
        best_objective = objective;
        best_blocks = blocks;
      }
    }
    blocks = move(best_blocks);
#else
    finish_current_construction();
#endif

    // The construction is designed to be valid by definition. If this small
    // independent check ever fails during modification, fall back to the
    // always-valid unit-block construction instead of printing a risky answer.
    if (!validate_locally()) {
      blocks.clear();
      fill(covered_front.begin(), covered_front.end(),
           array<unsigned short, 14>{});
      fill(covered_right.begin(), covered_right.end(),
           array<unsigned short, 14>{});
      for (int object = 0; object < 2; ++object) {
        fill(occupied[object].begin(), occupied[object].end(), false);
      }
      finish_current_construction();
    }
    print_answer();
  }
};

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  Solver solver;
  solver.read_input();
  solver.solve();
  return 0;
}
