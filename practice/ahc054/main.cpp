#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <queue>
#include <tuple>
#include <utility>
#include <vector>

using namespace std;

// AHC054: Treant's Forest
//
// The program uses three small ideas:
// 1. On turn 0, put a five-cell guard around the flower.  The guard leaves
//    only a bent entrance, which makes accidentally reaching the flower hard.
// 2. Add side walls to a short corridor leading into that entrance.
// 3. On every turn, try a few Treants on the four visible rays.  Choose the
//    plan which reveals the fewest new cells, without breaking connectivity.

struct Point {
    int row;
    int col;
};

struct BfsResult {
    vector<char> reachable;
    int flower_distance;
};

struct RayCandidate {
    Point point;
    int direction;
};

struct GuardCandidate {
    vector<Point> plan;
    int stage_score;
    int reachable_cells;
    int flower_distance;
    int wall_distance;
};

const array<int, 4> DR = {-1, 1, 0, 0};
const array<int, 4> DC = {0, 0, -1, 1};

int board_size;
Point entrance;
Point flower;
vector<string> original_board;
vector<char> placed;
vector<char> revealed;

bool inside(Point point) {
    return 0 <= point.row && point.row < board_size &&
           0 <= point.col && point.col < board_size;
}

int cell_id(Point point) {
    return point.row * board_size + point.col;
}

bool same_point(Point left, Point right) {
    return left.row == right.row && left.col == right.col;
}

bool is_original_tree(Point point) {
    return original_board[point.row][point.col] == 'T';
}

vector<char> make_plan_mask(const vector<Point>& plan) {
    vector<char> mask(board_size * board_size, false);
    for (Point point : plan) {
        mask[cell_id(point)] = true;
    }
    return mask;
}

bool blocked_with_plan(Point point, const vector<char>& plan_mask) {
    const int id = cell_id(point);
    return is_original_tree(point) || placed[id] || plan_mask[id];
}

BfsResult run_bfs(const vector<Point>& plan) {
    const vector<char> plan_mask = make_plan_mask(plan);
    const int cell_count = board_size * board_size;
    BfsResult result{vector<char>(cell_count, false), -1};

    if (blocked_with_plan(entrance, plan_mask)) {
        return result;
    }

    vector<int> distance(cell_count, -1);
    queue<Point> que;
    que.push(entrance);
    result.reachable[cell_id(entrance)] = true;
    distance[cell_id(entrance)] = 0;

    while (!que.empty()) {
        const Point current = que.front();
        que.pop();
        for (int direction = 0; direction < 4; ++direction) {
            const Point next{current.row + DR[direction],
                             current.col + DC[direction]};
            if (!inside(next) || blocked_with_plan(next, plan_mask)) {
                continue;
            }
            const int next_id = cell_id(next);
            if (result.reachable[next_id]) {
                continue;
            }
            result.reachable[next_id] = true;
            distance[next_id] = distance[cell_id(current)] + 1;
            que.push(next);
        }
    }
    result.flower_distance = distance[cell_id(flower)];
    return result;
}

bool can_place(Point point, const vector<char>& extra_blocked) {
    if (!inside(point) || same_point(point, flower)) {
        return false;
    }
    const int id = cell_id(point);
    return !is_original_tree(point) && !placed[id] && !revealed[id] &&
           !extra_blocked[id];
}

vector<Point> merge_plans(const vector<Point>& first,
                          const vector<Point>& second) {
    vector<Point> merged = first;
    vector<char> used = make_plan_mask(first);
    for (Point point : second) {
        if (!used[cell_id(point)]) {
            used[cell_id(point)] = true;
            merged.push_back(point);
        }
    }
    return merged;
}

// Do not cut off a formerly reachable cell unless that exact cell receives a
// Treant.  This conservative rule keeps all possible destinations usable.
bool keeps_previous_component(const vector<Point>& complete_plan,
                              const vector<char>& previously_reachable,
                              const BfsResult& after) {
    const vector<char> plan_mask = make_plan_mask(complete_plan);
    if (after.flower_distance == -1) {
        return false;
    }

    for (int id = 0; id < board_size * board_size; ++id) {
        if (previously_reachable[id] && !plan_mask[id] &&
            !after.reachable[id]) {
            return false;
        }
    }
    return true;
}

vector<char> provisional_reachability(Point player) {
    vector<char> provisionally_reachable(board_size * board_size, false);
    queue<Point> que;
    provisionally_reachable[cell_id(player)] = true;
    que.push(player);
    while (!que.empty()) {
        const Point current = que.front();
        que.pop();
        for (int direction = 0; direction < 4; ++direction) {
            const Point next{current.row + DR[direction],
                             current.col + DC[direction]};
            if (!inside(next) || provisionally_reachable[cell_id(next)]) {
                continue;
            }
            // In the adventurer's tentative map, an unrevealed cell is empty.
            // Only a tree which has already been seen blocks this BFS.
            const bool known_tree =
                revealed[cell_id(next)] &&
                (is_original_tree(next) || placed[cell_id(next)]);
            if (known_tree) {
                continue;
            }
            provisionally_reachable[cell_id(next)] = true;
            que.push(next);
        }
    }
    return provisionally_reachable;
}

// Count cells that would become visible from the current position after the
// proposed placements.  Smaller is better.
int count_newly_revealed(Point player, const vector<Point>& complete_plan,
                         const vector<char>& provisionally_reachable) {
    const vector<char> plan_mask = make_plan_mask(complete_plan);
    int count = 0;
    for (int direction = 0; direction < 4; ++direction) {
        Point current = player;
        while (true) {
            current.row += DR[direction];
            current.col += DC[direction];
            if (!inside(current)) {
                break;
            }
            if (!revealed[cell_id(current)] &&
                provisionally_reachable[cell_id(current)]) {
                ++count;
            }
            if (blocked_with_plan(current, plan_mask)) {
                break;
            }
        }
    }
    return count;
}

Point transform_offset(Point offset, int rotation, bool reflection) {
    if (reflection) {
        offset.col = -offset.col;
    }
    for (int repeat = 0; repeat < rotation; ++repeat) {
        offset = Point{-offset.col, offset.row};
    }
    return offset;
}

bool adjacent(Point left, Point right) {
    return abs(left.row - right.row) + abs(left.col - right.col) == 1;
}

int count_reachable_cells(const BfsResult& result) {
    return static_cast<int>(count(result.reachable.begin(),
                                  result.reachable.end(), true));
}

int count_free_cells(const vector<Point>& plan) {
    const vector<char> plan_mask = make_plan_mask(plan);
    int count_free = 0;
    for (int row = 0; row < board_size; ++row) {
        for (int col = 0; col < board_size; ++col) {
            const Point point{row, col};
            if (!blocked_with_plan(point, plan_mask)) {
                ++count_free;
            }
        }
    }
    return count_free;
}

// Make one corridor segment from start to a cell next to anchor.  Previous
// corridor cells stay open but, except for start, cannot be reused.
pair<vector<Point>, vector<Point>> make_corridor_segment(
    Point start, Point anchor, const vector<Point>& fixed_plan,
    const vector<Point>& previous_corridor, Point opening) {
    const int cell_count = board_size * board_size;
    vector<char> forbidden = make_plan_mask(fixed_plan);
    for (int row = 0; row < board_size; ++row) {
        for (int col = 0; col < board_size; ++col) {
            const Point point{row, col};
            if (is_original_tree(point) || placed[cell_id(point)]) {
                forbidden[cell_id(point)] = true;
            }
        }
    }
    for (Point point : previous_corridor) {
        forbidden[cell_id(point)] = true;
    }
    forbidden[cell_id(start)] = false;
    forbidden[cell_id(flower)] = true;
    if (inside(opening) && !same_point(opening, start)) {
        forbidden[cell_id(opening)] = true;
    }

    vector<int> parent(cell_count, -1);
    queue<Point> que;
    que.push(start);
    parent[cell_id(start)] = cell_id(start);
    Point goal{-1, -1};

    while (!que.empty()) {
        const Point current = que.front();
        que.pop();
        if (adjacent(current, anchor)) {
            goal = current;
            break;
        }
        for (int direction = 0; direction < 4; ++direction) {
            const Point next{current.row + DR[direction],
                             current.col + DC[direction]};
            if (!inside(next) || forbidden[cell_id(next)] ||
                parent[cell_id(next)] != -1) {
                continue;
            }
            parent[cell_id(next)] = cell_id(current);
            que.push(next);
        }
    }
    if (goal.row == -1) {
        return {{}, {}};
    }

    vector<Point> path;
    for (int id = cell_id(goal);; id = parent[id]) {
        path.push_back(Point{id / board_size, id % board_size});
        if (id == cell_id(start)) {
            break;
        }
    }
    reverse(path.begin(), path.end());

    // A wall beside the entrance can interfere with the very first move.
    for (Point point : path) {
        if (same_point(point, entrance) || adjacent(point, entrance)) {
            return {{}, {}};
        }
    }

    vector<char> keep_open(cell_count, false);
    for (Point point : previous_corridor) {
        keep_open[cell_id(point)] = true;
    }
    for (Point point : path) {
        keep_open[cell_id(point)] = true;
    }
    keep_open[cell_id(flower)] = true;
    if (inside(opening)) {
        keep_open[cell_id(opening)] = true;
    }

    vector<Point> walls;
    vector<char> used = make_plan_mask(fixed_plan);
    for (int index = 0; index + 1 < static_cast<int>(path.size());
         ++index) {
        const Point current = path[index];
        for (int direction = 0; direction < 4; ++direction) {
            const Point next{current.row + DR[direction],
                             current.col + DC[direction]};
            if (!inside(next) || keep_open[cell_id(next)] ||
                used[cell_id(next)] || is_original_tree(next) ||
                placed[cell_id(next)] || revealed[cell_id(next)]) {
                continue;
            }
            used[cell_id(next)] = true;
            walls.push_back(next);
        }
    }
    return {path, walls};
}

bool better_forward_guard(const GuardCandidate& candidate,
                          const GuardCandidate& best) {
    if (candidate.stage_score != best.stage_score) {
        return candidate.stage_score > best.stage_score;
    }
    if (candidate.reachable_cells != best.reachable_cells) {
        return candidate.reachable_cells > best.reachable_cells;
    }
    if (candidate.flower_distance != best.flower_distance) {
        return candidate.flower_distance > best.flower_distance;
    }
    return candidate.plan.size() < best.plan.size();
}

bool better_backward_guard(const GuardCandidate& candidate,
                           const GuardCandidate& best) {
    if (candidate.stage_score != best.stage_score) {
        return candidate.stage_score > best.stage_score;
    }
    if (candidate.reachable_cells != best.reachable_cells) {
        return candidate.reachable_cells > best.reachable_cells;
    }
    if (candidate.wall_distance != best.wall_distance) {
        return candidate.wall_distance < best.wall_distance;
    }
    return candidate.plan.size() < best.plan.size();
}

vector<Point> choose_flower_fort() {
    const array<Point, 5> tree_offsets = {
        Point{-2, 0}, Point{-1, 1}, Point{0, -1},
        Point{0, 1}, Point{1, 0}};
    const array<Point, 2> open_offsets = {
        Point{-1, 0}, Point{-1, -1}};

    GuardCandidate best_forward{{}, -1, -1, -1, board_size + 1};
    GuardCandidate best_backward = best_forward;

    for (int rotation = 0; rotation < 4; ++rotation) {
        for (int reflection_value = 0; reflection_value < 2;
             ++reflection_value) {
            const bool reflection = reflection_value != 0;
            bool valid = true;
            vector<Point> plan;
            vector<char> plan_mask(board_size * board_size, false);

            for (Point offset : open_offsets) {
                const Point moved = transform_offset(offset, rotation,
                                                     reflection);
                const Point point{flower.row + moved.row,
                                  flower.col + moved.col};
                if (inside(point) &&
                    (is_original_tree(point) || placed[cell_id(point)])) {
                    valid = false;
                }
            }
            if (!valid) {
                continue;
            }

            for (Point offset : tree_offsets) {
                const Point moved = transform_offset(offset, rotation,
                                                     reflection);
                const Point point{flower.row + moved.row,
                                  flower.col + moved.col};
                if (!inside(point) || is_original_tree(point) ||
                    placed[cell_id(point)]) {
                    continue;
                }
                if (!can_place(point, plan_mask)) {
                    valid = false;
                    break;
                }
                plan_mask[cell_id(point)] = true;
                plan.push_back(point);
            }
            if (!valid) {
                continue;
            }

            const Point opening_offset =
                transform_offset(Point{-1, 0}, rotation, reflection);
            const Point opening{flower.row + opening_offset.row,
                                flower.col + opening_offset.col};
            const Point corridor_start_offset =
                transform_offset(Point{-1, -1}, rotation, reflection);
            const Point corridor_start{
                flower.row + corridor_start_offset.row,
                flower.col + corridor_start_offset.col};
            if (!inside(corridor_start) ||
                blocked_with_plan(corridor_start, plan_mask)) {
                continue;
            }

            int wall_distance = 0;
            if (opening_offset.row == -1) {
                wall_distance = flower.row;
            } else if (opening_offset.row == 1) {
                wall_distance = board_size - 1 - flower.row;
            } else if (opening_offset.col == -1) {
                wall_distance = flower.col;
            } else {
                wall_distance = board_size - 1 - flower.col;
            }

            const array<Point, 3> forward_offsets = {
                Point{-2, 0}, Point{0, 1}, Point{1, 0}};
            const array<Point, 3> backward_offsets = {
                Point{1, 0}, Point{0, 1}, Point{-2, 0}};

            for (int order = 0; order < 2; ++order) {
                vector<Point> complete_plan = plan;
                vector<Point> corridor = {corridor_start};
                Point current = corridor_start;
                // Two points per normal segment let us represent the one-point
                // fallback penalty below without floating-point numbers.
                int stage_score = 0;

                auto consider_current = [&]() {
                    const BfsResult result = run_bfs(complete_plan);
                    if (result.flower_distance == -1) {
                        return;
                    }
                    const int reachable_count = count_reachable_cells(result);
                    const int unreachable_count =
                        count_free_cells(complete_plan) - reachable_count;
                    if (unreachable_count >
                        board_size * board_size / 4) {
                        return;
                    }
                    const GuardCandidate candidate{
                        complete_plan, stage_score, reachable_count,
                        result.flower_distance, wall_distance};
                    GuardCandidate& best_for_order =
                        order == 0 ? best_forward : best_backward;
                    const bool is_better =
                        order == 0
                            ? better_forward_guard(candidate,
                                                   best_for_order)
                            : better_backward_guard(candidate,
                                                    best_for_order);
                    if (is_better) {
                        best_for_order = candidate;
                    }
                };
                consider_current();

                const array<Point, 3>& offsets =
                    order == 0 ? forward_offsets : backward_offsets;
                for (int segment = 0; segment < 3; ++segment) {
                    const Point offset = offsets[segment];
                    const Point moved =
                        transform_offset(offset, rotation, reflection);
                    const Point anchor{flower.row + moved.row,
                                       flower.col + moved.col};
                    if (!inside(anchor)) {
                        stage_score += 2;
                        consider_current();
                        continue;
                    }

                    bool already_adjacent = false;
                    for (Point point : corridor) {
                        already_adjacent |= adjacent(point, anchor);
                    }
                    if (already_adjacent) {
                        stage_score += 2;
                        consider_current();
                        continue;
                    }

                    auto [path, walls] = make_corridor_segment(
                        current, anchor, complete_plan, corridor, opening);
                    bool used_fallback = false;

                    // Occasionally all four neighbors of the middle guard
                    // cell are already trees.  In the forward order only,
                    // route beside the next tree instead.  It is slightly
                    // weaker than completing the intended segment, hence +1
                    // rather than +2 below.
                    if (path.empty() && order == 0 && segment == 1) {
                        const Point fallback_offset = transform_offset(
                            Point{0, 2}, rotation, reflection);
                        const Point fallback_anchor{
                            flower.row + fallback_offset.row,
                            flower.col + fallback_offset.col};
                        if (inside(fallback_anchor) &&
                            (is_original_tree(fallback_anchor) ||
                             placed[cell_id(fallback_anchor)])) {
                            tie(path, walls) = make_corridor_segment(
                                current, fallback_anchor, complete_plan,
                                corridor, opening);
                            used_fallback = !path.empty();
                        }
                    }
                    if (path.empty()) {
                        break;
                    }
                    complete_plan = merge_plans(complete_plan, walls);
                    for (int index = 1;
                         index < static_cast<int>(path.size()); ++index) {
                        corridor.push_back(path[index]);
                    }
                    current = path.back();
                    stage_score += used_fallback ? 1 : 2;
                    consider_current();
                }
            }
        }
    }
    if (better_backward_guard(best_backward, best_forward)) {
        return best_backward.plan;
    }
    return best_forward.plan;
}

vector<RayCandidate> collect_ray_candidates(
    Point player, const vector<Point>& fixed_plan) {
    const vector<char> fixed_mask = make_plan_mask(fixed_plan);
    vector<RayCandidate> candidates;

    for (int direction = 0; direction < 4; ++direction) {
        Point current = player;
        int collected = 0;
        for (int distance = 1; distance <= 6; ++distance) {
            current.row += DR[direction];
            current.col += DC[direction];
            if (!inside(current) ||
                blocked_with_plan(current, fixed_mask)) {
                break;
            }
            if (can_place(current, fixed_mask)) {
                candidates.push_back({current, direction});
                ++collected;
                if (collected == 6) {
                    break;
                }
            }
        }
    }
    return candidates;
}

vector<Point> choose_ray_blocks(Point player,
                                const vector<Point>& fixed_plan,
                                int maximum_directions) {
    const vector<RayCandidate> candidates =
        collect_ray_candidates(player, fixed_plan);
    const BfsResult before = run_bfs(fixed_plan);
    const vector<char> provisional = provisional_reachability(player);

    vector<Point> best_plan;
    const int baseline_reveal =
        count_newly_revealed(player, fixed_plan, provisional);
    int best_reveal = baseline_reveal;
    int best_flower_distance = before.flower_distance;

    // Each direction contributes either no Treant or one nearby candidate.
    // We use at most three directions on turn 0 and two afterwards.
    array<vector<Point>, 4> by_direction;
    for (const RayCandidate& candidate : candidates) {
        by_direction[candidate.direction].push_back(candidate.point);
    }

    vector<Point> chosen;
    auto search = [&](auto&& self, int direction, int used_directions) -> void {
        if (direction == 4) {
            if (chosen.empty()) {
                return;
            }
            const vector<Point> complete = merge_plans(fixed_plan, chosen);
            const BfsResult after = run_bfs(complete);
            if (!keeps_previous_component(complete, before.reachable,
                                          after)) {
                return;
            }
            const int reveal =
                count_newly_revealed(player, complete, provisional);
            const int distance = after.flower_distance;
            if (reveal < best_reveal ||
                (reveal == best_reveal &&
                 distance > best_flower_distance)) {
                best_reveal = reveal;
                best_flower_distance = distance;
                best_plan = chosen;
            }
            return;
        }

        self(self, direction + 1, used_directions);
        if (used_directions == maximum_directions) {
            return;
        }
        for (Point point : by_direction[direction]) {
            chosen.push_back(point);
            self(self, direction + 1, used_directions + 1);
            chosen.pop_back();
        }
    };
    search(search, 0, 0);
    return best_plan;
}

// The entrance is on the top edge, so the first observation has only three
// useful rays: down, right, and left.  Prefer blocking all three, then two,
// then one.  This gives the initial uncertainty a strong push without adding
// complexity to later turns.
vector<Point> choose_first_ray_blocks(Point player,
                                      const vector<Point>& fixed_plan) {
    const vector<RayCandidate> candidates =
        collect_ray_candidates(player, fixed_plan);
    array<vector<Point>, 4> by_direction;
    for (const RayCandidate& candidate : candidates) {
        by_direction[candidate.direction].push_back(candidate.point);
    }
    const BfsResult before = run_bfs(fixed_plan);
    const vector<char> provisional = provisional_reachability(player);

    auto best_for_directions = [&](const vector<int>& directions) {
        vector<Point> best_plan;
        int best_reveal = board_size * board_size + 1;
        vector<Point> chosen;
        auto search = [&](auto&& self, int index) -> void {
            if (index == static_cast<int>(directions.size())) {
                const vector<Point> complete =
                    merge_plans(fixed_plan, chosen);
                const BfsResult after = run_bfs(complete);
                if (!keeps_previous_component(complete, before.reachable,
                                              after)) {
                    return;
                }
                const int reveal =
                    count_newly_revealed(player, complete, provisional);
                if (reveal < best_reveal) {
                    best_reveal = reveal;
                    best_plan = chosen;
                }
                return;
            }
            for (Point point : by_direction[directions[index]]) {
                chosen.push_back(point);
                self(self, index + 1);
                chosen.pop_back();
            }
        };
        search(search, 0);
        return pair<vector<Point>, int>{best_plan, best_reveal};
    };

    const auto [triple, triple_reveal] =
        best_for_directions({1, 3, 2});
    (void)triple_reveal;
    if (!triple.empty()) {
        return triple;
    }

    vector<Point> best_pair;
    int best_pair_reveal = board_size * board_size + 1;
    const array<array<int, 2>, 3> pairs = {
        array<int, 2>{1, 3}, array<int, 2>{1, 2},
        array<int, 2>{3, 2}};
    for (const auto& directions : pairs) {
        const auto [plan, reveal] = best_for_directions(
            {directions[0], directions[1]});
        if (!plan.empty() && reveal < best_pair_reveal) {
            best_pair = plan;
            best_pair_reveal = reveal;
        }
    }
    if (!best_pair.empty()) {
        return best_pair;
    }

    vector<Point> best_single;
    int best_single_reveal = board_size * board_size + 1;
    for (int direction : {1, 3, 2}) {
        const auto [plan, reveal] = best_for_directions({direction});
        if (!plan.empty() && reveal < best_single_reveal) {
            best_single = plan;
            best_single_reveal = reveal;
        }
    }
    const int baseline_reveal =
        count_newly_revealed(player, fixed_plan, provisional);
    if (best_single_reveal < baseline_reveal) {
        return best_single;
    }
    return {};
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int flower_row;
    int flower_col;
    if (!(cin >> board_size >> flower_row >> flower_col)) {
        return 0;
    }
    flower = Point{flower_row, flower_col};
    entrance = Point{0, board_size / 2};
    original_board.resize(board_size);
    for (string& row : original_board) {
        cin >> row;
    }
    placed.assign(board_size * board_size, false);
    revealed.assign(board_size * board_size, false);

#ifndef SIMPLE_BASELINE
    const auto start_time = chrono::steady_clock::now();
    bool first_turn = true;
#endif

    while (true) {
        Point player;
        int newly_revealed_count;
        if (!(cin >> player.row >> player.col >> newly_revealed_count)) {
            return 0;
        }
        for (int index = 0; index < newly_revealed_count; ++index) {
            Point point;
            cin >> point.row >> point.col;
            revealed[cell_id(point)] = true;
        }
        if (same_point(player, flower)) {
            return 0;
        }

#ifdef SIMPLE_BASELINE
        cout << -1 << '\n' << flush;
        return 0;
#else
        const double elapsed_ms =
            chrono::duration<double, milli>(chrono::steady_clock::now() -
                                             start_time)
                .count();
        if (elapsed_ms > 1650.0) {
            cout << -1 << '\n' << flush;
            return 0;
        }

        vector<Point> fixed_plan;
        if (first_turn) {
            fixed_plan = choose_flower_fort();
        }
        vector<Point> ray_plan;
        if (first_turn) {
            ray_plan = choose_first_ray_blocks(player, fixed_plan);
        } else {
            ray_plan = choose_ray_blocks(player, fixed_plan, 2);
        }
        vector<Point> plan = merge_plans(fixed_plan, ray_plan);

        // A final, cheap safety check guards against implementation mistakes.
        if (run_bfs(plan).flower_distance == -1) {
            plan.clear();
        }

        cout << plan.size();
        for (Point point : plan) {
            cout << ' ' << point.row << ' ' << point.col;
            placed[cell_id(point)] = true;
        }
        cout << '\n' << flush;
        first_turn = false;
#endif
    }
}
