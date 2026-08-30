#include <bits/stdc++.h>
using namespace std;

constexpr int SIZE = 30;
constexpr int TURN_COUNT = 300;
constexpr int GATE_ROW = 15;

constexpr array<int, 4> DR{-1, 1, 0, 0};
constexpr array<int, 4> DC{0, 0, -1, 1};
constexpr array<char, 4> MOVE_CHAR{'U', 'D', 'L', 'R'};
constexpr array<char, 4> BLOCK_CHAR{'u', 'd', 'l', 'r'};

struct Pet {
  int row;
  int column;
  int type;
};

struct Human {
  int row;
  int column;
};

struct Job {
  int wall_column = -1;
  vector<int> rows;
  int next_index = 0;

  bool finished() const {
    return next_index == static_cast<int>(rows.size());
  }
};

bool inside(int row, int column) {
  return 0 <= row && row < SIZE && 0 <= column && column < SIZE;
}

// blockedを避ける最短路の最初の1手を返す。
// 人同士は同じマスに重なれるので、障害物として扱わない。
char first_step_to(
    int start_row,
    int start_column,
    int target_row,
    int target_column,
    const array<array<bool, SIZE>, SIZE>& blocked,
    const array<array<bool, SIZE>, SIZE>& reserved_block) {
  if (start_row == target_row && start_column == target_column) return '.';

  array<array<int, SIZE>, SIZE> distance;
  array<array<int, SIZE>, SIZE> first_direction;
  for (auto& row : distance) row.fill(-1);
  for (auto& row : first_direction) row.fill(-1);

  array<pair<int, int>, SIZE * SIZE> queue{};
  int head = 0;
  int tail = 0;
  queue[tail++] = {start_row, start_column};
  distance[start_row][start_column] = 0;

  while (head < tail) {
    const auto [row, column] = queue[head++];
    for (int direction = 0; direction < 4; ++direction) {
      const int next_row = row + DR[direction];
      const int next_column = column + DC[direction];
      if (!inside(next_row, next_column)) continue;
      if (blocked[next_row][next_column]) continue;
      if (reserved_block[next_row][next_column]) continue;
      if (distance[next_row][next_column] != -1) continue;

      distance[next_row][next_column] = distance[row][column] + 1;
      first_direction[next_row][next_column] =
          distance[row][column] == 0
              ? direction
              : first_direction[row][column];
      if (next_row == target_row && next_column == target_column) {
        return MOVE_CHAR[first_direction[next_row][next_column]];
      }
      queue[tail++] = {next_row, next_column};
    }
  }
  return '.';
}

bool can_block_cell(
    int row,
    int column,
    const vector<Pet>& pets,
    const vector<Human>& humans) {
  if (!inside(row, column)) return false;
  for (const Pet& pet : pets) {
    if (pet.row == row && pet.column == column) return false;
    if (abs(pet.row - row) + abs(pet.column - column) == 1) return false;
  }
  for (const Human& human : humans) {
    if (human.row == row && human.column == column) return false;
  }
  return true;
}

vector<Job> make_jobs(const vector<Human>& humans, int pet_count) {
  const int human_count = static_cast<int>(humans.size());

  // ペット数から、期待得点が高くなりやすい帯幅を概算する。
  // 必要な壁数は必ず人数以下にし、各壁へ最低1人を割り当てる。
  const int estimated_best_wall_count =
      static_cast<int>(lround(30.0 * (pet_count - 1) / (pet_count + 61.0)));
  const int wall_count =
      min(human_count, max(4, estimated_best_wall_count));

  vector<int> wall_columns;
  for (int index = 0; index < wall_count; ++index) {
    wall_columns.push_back(
        (index + 1) * SIZE / (wall_count + 1));
  }

  vector<int> worker_count(wall_count, 1);
  for (int worker = wall_count; worker < human_count; ++worker) {
    ++worker_count[(worker - wall_count) % wall_count];
  }

  struct Slot {
    int wall_column;
    int first_row;
    int last_row;
  };
  vector<Slot> slots;
  for (int wall = 0; wall < wall_count; ++wall) {
    for (int worker = 0; worker < worker_count[wall]; ++worker) {
      const int first_row = SIZE * worker / worker_count[wall];
      const int last_row = SIZE * (worker + 1) / worker_count[wall] - 1;
      slots.push_back({wall_columns[wall], first_row, last_row});
    }
  }

  // 近い人から担当区間へ割り当て、初期集合時間を短くする。
  vector<Job> jobs(human_count);
  vector<bool> used_slot(slots.size(), false);
  for (int human = 0; human < human_count; ++human) {
    int best_slot = -1;
    int best_distance = numeric_limits<int>::max();
    for (int slot = 0; slot < static_cast<int>(slots.size()); ++slot) {
      if (used_slot[slot]) continue;
      const int row_distance = min(
          abs(humans[human].row - slots[slot].first_row),
          abs(humans[human].row - slots[slot].last_row));
      const int distance = row_distance +
          abs(humans[human].column - (slots[slot].wall_column - 1));
      if (distance < best_distance) {
        best_distance = distance;
        best_slot = slot;
      }
    }

    used_slot[best_slot] = true;
    const Slot slot = slots[best_slot];
    jobs[human].wall_column = slot.wall_column;
    if (abs(humans[human].row - slot.first_row) <=
        abs(humans[human].row - slot.last_row)) {
      for (int row = slot.first_row; row <= slot.last_row; ++row) {
        if (row != GATE_ROW) jobs[human].rows.push_back(row);
      }
    } else {
      for (int row = slot.last_row; row >= slot.first_row; --row) {
        if (row != GATE_ROW) jobs[human].rows.push_back(row);
      }
    }
  }
  return jobs;
}

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  int pet_count;
  cin >> pet_count;
  vector<Pet> pets(pet_count);
  for (Pet& pet : pets) {
    cin >> pet.row >> pet.column >> pet.type;
    --pet.row;
    --pet.column;
  }

  int human_count;
  cin >> human_count;
  vector<Human> humans(human_count);
  for (Human& human : humans) {
    cin >> human.row >> human.column;
    --human.row;
    --human.column;
  }

  vector<Job> jobs = make_jobs(humans, pet_count);
  vector<int> wall_columns;
  for (const Job& job : jobs) wall_columns.push_back(job.wall_column);
  sort(wall_columns.begin(), wall_columns.end());
  wall_columns.erase(
      unique(wall_columns.begin(), wall_columns.end()),
      wall_columns.end());

  vector<int> closer(wall_columns.size(), -1);
  for (int human = 0; human < human_count; ++human) {
    const int wall = static_cast<int>(
        lower_bound(wall_columns.begin(), wall_columns.end(),
                    jobs[human].wall_column) - wall_columns.begin());
    if (closer[wall] == -1) closer[wall] = human;
  }

  array<array<bool, SIZE>, SIZE> blocked{};
  enum Phase { PREPARE, BUILD_WALLS, CLOSE_GATES, DONE };
  Phase phase = PREPARE;
  int next_wall_to_close = 0;
  int gate_wait = 0;

  for (int turn = 0; turn < TURN_COUNT; ++turn) {
    string actions(human_count, '.');
    array<array<bool, SIZE>, SIZE> reserved_block{};
    bool closed_gate_this_turn = false;

    if (phase == PREPARE) {
      bool everyone_ready = true;
      for (int human = 0; human < human_count; ++human) {
        const int target_row = jobs[human].rows.front();
        const int target_column = jobs[human].wall_column - 1;
        if (humans[human].row != target_row ||
            humans[human].column != target_column) {
          everyone_ready = false;
          actions[human] = first_step_to(
              humans[human].row,
              humans[human].column,
              target_row,
              target_column,
              blocked,
              reserved_block);
        }
      }
      if (everyone_ready) phase = BUILD_WALLS;
    }

    if (phase == BUILD_WALLS) {
      // 先に壁設置を予約し、そのマスへ別の人が同時に移動しないようにする。
      for (int human = 0; human < human_count; ++human) {
        Job& job = jobs[human];
        while (!job.finished() &&
               blocked[job.rows[job.next_index]][job.wall_column]) {
          ++job.next_index;
        }
        if (job.finished()) continue;

        const int target_row = job.rows[job.next_index];
        const int stand_column = job.wall_column - 1;
        if (humans[human].row == target_row &&
            humans[human].column == stand_column &&
            can_block_cell(target_row, job.wall_column, pets, humans)) {
          actions[human] = 'r';
          reserved_block[target_row][job.wall_column] = true;
        }
      }

      for (int human = 0; human < human_count; ++human) {
        if (actions[human] != '.') continue;
        Job& job = jobs[human];
        if (job.finished()) continue;

        const int target_row = job.rows[job.next_index];
        const int target_column = job.wall_column - 1;
        if (humans[human].row == target_row &&
            humans[human].column == target_column) {
          continue;  // ペットが離れるまで安全に待つ。
        }
        actions[human] = first_step_to(
            humans[human].row,
            humans[human].column,
            target_row,
            target_column,
            blocked,
            reserved_block);
      }

      bool all_jobs_finished = true;
      for (const Job& job : jobs) {
        if (!job.finished()) all_jobs_finished = false;
      }
      if (all_jobs_finished) phase = CLOSE_GATES;
    }

    if (phase == CLOSE_GATES) {
      if (next_wall_to_close == static_cast<int>(wall_columns.size())) {
        phase = DONE;
      } else {
        const int wall = next_wall_to_close;
        const int wall_column = wall_columns[wall];
        const int previous_wall =
            wall == 0
                ? -1
                : wall_columns[wall - 1];
        int pets_in_band = 0;
        for (const Pet& pet : pets) {
          if (previous_wall < pet.column && pet.column < wall_column) {
            ++pets_in_band;
          }
        }

        const int walls_left =
            static_cast<int>(wall_columns.size()) - next_wall_to_close;
        const int turns_left = TURN_COUNT - turn;
        const int wait_budget = max(
            4,
            (turns_left - 2 * walls_left) / max(1, walls_left));
        int acceptable_pets = 0;
        if (gate_wait * 2 >= wait_budget) acceptable_pets = 1;
        if (gate_wait * 4 >= wait_budget * 3) acceptable_pets = 2;
        if (gate_wait >= wait_budget) acceptable_pets = pet_count;

        // 原則として右隣の帯に居る人が外側から閉じる。
        // 最後の壁だけは右隣に担当者が居ないので内側から閉じる。
        const bool close_from_right =
            wall + 1 < static_cast<int>(wall_columns.size());
        const int human = close_from_right ? closer[wall + 1] : closer[wall];
        const int stand_column =
            wall_column + (close_from_right ? 1 : -1);
        const char block_action = close_from_right ? 'l' : 'r';

        if (humans[human].row != GATE_ROW ||
            humans[human].column != stand_column) {
          actions[human] = first_step_to(
              humans[human].row,
              humans[human].column,
              GATE_ROW,
              stand_column,
              blocked,
              reserved_block);
        } else if (pets_in_band <= acceptable_pets &&
                   can_block_cell(GATE_ROW, wall_column, pets, humans)) {
          actions[human] = block_action;
          reserved_block[GATE_ROW][wall_column] = true;
          closed_gate_this_turn = true;
        } else {
          ++gate_wait;
        }
      }
    }

    cout << actions << endl;  // 対話問題なので毎ターンflushする

    // 人間の行動を手元の状態にも反映する。
    for (int human = 0; human < human_count; ++human) {
      const char action = actions[human];
      for (int direction = 0; direction < 4; ++direction) {
        if (action == MOVE_CHAR[direction]) {
          humans[human].row += DR[direction];
          humans[human].column += DC[direction];
        }
      }
    }
    for (int row = 0; row < SIZE; ++row) {
      for (int column = 0; column < SIZE; ++column) {
        if (reserved_block[row][column]) blocked[row][column] = true;
      }
    }
    for (int human = 0; human < human_count; ++human) {
      if (actions[human] == 'r' && !jobs[human].finished()) {
        ++jobs[human].next_index;
      }
    }
    if (closed_gate_this_turn) {
      ++next_wall_to_close;
      gate_wait = 0;
    }

    // 返された文字列を順に適用し、次ターンの合法性判定へ使う。
    for (Pet& pet : pets) {
      string movement;
      cin >> movement;
      if (movement == ".") continue;
      for (char move : movement) {
        for (int direction = 0; direction < 4; ++direction) {
          if (move == MOVE_CHAR[direction]) {
            pet.row += DR[direction];
            pet.column += DC[direction];
          }
        }
      }
    }
  }
}
