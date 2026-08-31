#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <queue>
#include <string>
#include <utility>
#include <vector>

using namespace std;

struct Operation {
    char direction = 'H';
    int row = 0;
    int column = 0;
    int height = 1;
    int width = 1;
};

int n;
int cell_count;
vector<int> board;
vector<int> position_of_card;
vector<string> vertical_walls;
vector<string> horizontal_walls;
vector<vector<int>> graph_edges;
vector<vector<int>> graph_distance;
vector<char> active_cell;
vector<vector<int>> inactive_prefix;
vector<vector<int>> vertical_wall_prefix;
vector<vector<int>> horizontal_wall_prefix;
vector<int> segment_left;
vector<int> segment_right;
vector<int> segment_top;
vector<int> segment_bottom;
vector<Operation> answer;

int cell_id(int row, int column) {
    return row * n + column;
}

// board と「各カードの現在位置」を同時に更新する。
void apply_operation(const Operation& operation) {
    if (operation.direction == 'H') {
        const int half = operation.width / 2;
        for (int row_offset = 0; row_offset < operation.height; ++row_offset) {
            for (int column_offset = 0; column_offset < half; ++column_offset) {
                const int first = cell_id(operation.row + row_offset,
                                          operation.column + column_offset);
                const int second = first + half;
                swap(board[first], board[second]);
                position_of_card[board[first]] = first;
                position_of_card[board[second]] = second;
            }
        }
    } else {
        const int half = operation.height / 2;
        for (int row_offset = 0; row_offset < half; ++row_offset) {
            for (int column_offset = 0; column_offset < operation.width; ++column_offset) {
                const int first = cell_id(operation.row + row_offset,
                                          operation.column + column_offset);
                const int second = first + half * n;
                swap(board[first], board[second]);
                position_of_card[board[first]] = first;
                position_of_card[board[second]] = second;
            }
        }
    }
    answer.push_back(operation);
}

void swap_adjacent_cells(int first, int second) {
    const int first_row = first / n;
    const int first_column = first % n;
    const int second_row = second / n;
    const int second_column = second % n;
    if (first_row == second_row) {
        apply_operation(Operation{'H', first_row,
                                  min(first_column, second_column), 1, 2});
    } else {
        apply_operation(Operation{'V', min(first_row, second_row),
                                  first_column, 2, 1});
    }
}

vector<int> adjacent_path(int start, int goal) {
    vector<int> previous(cell_count, -1);
    queue<int> que;
    previous[start] = start;
    que.push(start);
    while (!que.empty()) {
        const int cell = que.front();
        que.pop();
        if (cell == goal) break;
        for (const int next : graph_edges[cell]) {
            if (!active_cell[next] || previous[next] != -1) continue;
            previous[next] = cell;
            que.push(next);
        }
    }

    vector<int> path;
    for (int cell = goal; cell != start; cell = previous[cell]) {
        assert(cell != -1);
        path.push_back(cell);
    }
    reverse(path.begin(), path.end());
    return path;
}

void move_card_with_adjacent_swaps(int target_cell) {
    int current = position_of_card[target_cell];
    const vector<int> path = adjacent_path(current, target_cell);
    for (const int next : path) {
        swap_adjacent_cells(current, next);
        current = next;
    }
    assert(board[target_cell] == target_cell);
}

vector<int> spanning_tree_postorder() {
    int root = -1;
    for (int cell = 0; cell < cell_count; ++cell) {
        if (active_cell[cell]) {
            root = cell;
            break;
        }
    }
    if (root == -1) return {};

    vector<int> parent(cell_count, -1);
    vector<int> bfs_order;
    queue<int> que;
    parent[root] = root;
    que.push(root);
    while (!que.empty()) {
        const int cell = que.front();
        que.pop();
        bfs_order.push_back(cell);
        for (const int next : graph_edges[cell]) {
            if (!active_cell[next] || parent[next] != -1) continue;
            parent[next] = cell;
            que.push(next);
        }
    }

    reverse(bfs_order.begin(), bfs_order.end());
    return bfs_order;
}

// 必ず完成する保険。連結な未確定領域の葉から、隣接交換だけで揃える。
void finish_with_adjacent_swaps() {
    const vector<int> order = spanning_tree_postorder();
    for (int index = 0; index + 1 < static_cast<int>(order.size()); ++index) {
        const int target = order[index];
        move_card_with_adjacent_swaps(target);
        active_cell[target] = false;
    }
}

// 同じ行・列で一気に移動できる区間を、各マスについてまとめて求める。
pair<int, int> horizontal_segment(int cell) {
    return {segment_left[cell], segment_right[cell]};
}

pair<int, int> vertical_segment(int cell) {
    return {segment_top[cell], segment_bottom[cell]};
}

void rebuild_active_segments() {
    segment_left.assign(cell_count, 0);
    segment_right.assign(cell_count, 0);
    segment_top.assign(cell_count, 0);
    segment_bottom.assign(cell_count, 0);

    for (int row = 0; row < n; ++row) {
        int column = 0;
        while (column < n) {
            if (!active_cell[cell_id(row, column)]) {
                ++column;
                continue;
            }
            const int left = column;
            while (column + 1 < n
                   && active_cell[cell_id(row, column + 1)]
                   && vertical_walls[row][column] == '0') {
                ++column;
            }
            const int right = column;
            for (int inside = left; inside <= right; ++inside) {
                const int cell = cell_id(row, inside);
                segment_left[cell] = left;
                segment_right[cell] = right;
            }
            ++column;
        }
    }

    for (int column = 0; column < n; ++column) {
        int row = 0;
        while (row < n) {
            if (!active_cell[cell_id(row, column)]) {
                ++row;
                continue;
            }
            const int top = row;
            while (row + 1 < n
                   && active_cell[cell_id(row + 1, column)]
                   && horizontal_walls[row][column] == '0') {
                ++row;
            }
            const int bottom = row;
            for (int inside = top; inside <= bottom; ++inside) {
                const int cell = cell_id(inside, column);
                segment_top[cell] = top;
                segment_bottom[cell] = bottom;
            }
            ++row;
        }
    }
}

// 1操作でカードが移れるマス同士を辺で結んだグラフ。
vector<vector<int>> build_macro_graph() {
    vector<vector<int>> macro_graph(cell_count);
    for (int cell = 0; cell < cell_count; ++cell) {
        if (!active_cell[cell]) continue;
        const int row = cell / n;
        const int column = cell % n;

        const auto [left, right] = horizontal_segment(cell);
        for (int distance = 1; column + distance <= right; ++distance) {
            const int first_start = max(left, column - distance + 1);
            const int last_start = min(column, right - 2 * distance + 1);
            if (first_start <= last_start) {
                macro_graph[cell].push_back(cell + distance);
            }
        }
        for (int distance = 1; column - distance >= left; ++distance) {
            const int first_start = max(left, column - 2 * distance + 1);
            const int last_start = min(column - distance,
                                       right - 2 * distance + 1);
            if (first_start <= last_start) {
                macro_graph[cell].push_back(cell - distance);
            }
        }

        const auto [top, bottom] = vertical_segment(cell);
        for (int distance = 1; row + distance <= bottom; ++distance) {
            const int first_start = max(top, row - distance + 1);
            const int last_start = min(row, bottom - 2 * distance + 1);
            if (first_start <= last_start) {
                macro_graph[cell].push_back(cell + distance * n);
            }
        }
        for (int distance = 1; row - distance >= top; ++distance) {
            const int first_start = max(top, row - 2 * distance + 1);
            const int last_start = min(row - distance,
                                       bottom - 2 * distance + 1);
            if (first_start <= last_start) {
                macro_graph[cell].push_back(cell - distance * n);
            }
        }
    }
    return macro_graph;
}

vector<int> shortest_macro_path(const vector<vector<int>>& macro_graph,
                                int start, int goal) {
    vector<int> previous(cell_count, -1);
    queue<int> que;
    previous[start] = start;
    que.push(start);
    while (!que.empty()) {
        const int cell = que.front();
        que.pop();
        if (cell == goal) break;
        for (const int next : macro_graph[cell]) {
            if (previous[next] != -1) continue;
            previous[next] = cell;
            que.push(next);
        }
    }

    vector<int> path;
    for (int cell = goal; cell != start; cell = previous[cell]) {
        assert(cell != -1);
        path.push_back(cell);
    }
    reverse(path.begin(), path.end());
    return path;
}

// 目的カード以外も動くので、全カードの距離変化を正確に計算する。
pair<long long, int> operation_quality(const Operation& operation) {
    long long distance_change = 0;
    int wrong_change = 0;

    if (operation.direction == 'H') {
        const int half = operation.width / 2;
        for (int row_offset = 0; row_offset < operation.height; ++row_offset) {
            for (int column_offset = 0; column_offset < half; ++column_offset) {
                const int first = cell_id(operation.row + row_offset,
                                          operation.column + column_offset);
                const int second = first + half;
                const int first_card = board[first];
                const int second_card = board[second];
                distance_change += graph_distance[second][first_card]
                                 + graph_distance[first][second_card]
                                 - graph_distance[first][first_card]
                                 - graph_distance[second][second_card];
                wrong_change += static_cast<int>(first_card != second)
                              + static_cast<int>(second_card != first)
                              - static_cast<int>(first_card != first)
                              - static_cast<int>(second_card != second);
            }
        }
    } else {
        const int half = operation.height / 2;
        for (int row_offset = 0; row_offset < half; ++row_offset) {
            for (int column_offset = 0; column_offset < operation.width;
                 ++column_offset) {
                const int first = cell_id(operation.row + row_offset,
                                          operation.column + column_offset);
                const int second = first + half * n;
                const int first_card = board[first];
                const int second_card = board[second];
                distance_change += graph_distance[second][first_card]
                                 + graph_distance[first][second_card]
                                 - graph_distance[first][first_card]
                                 - graph_distance[second][second_card];
                wrong_change += static_cast<int>(first_card != second)
                              + static_cast<int>(second_card != first)
                              - static_cast<int>(first_card != first)
                              - static_cast<int>(second_card != second);
            }
        }
    }
    return {distance_change, wrong_change};
}

// 矩形内の非アクティブマス数・壁数を O(1) で調べるための累積和。
int rectangle_sum(const vector<vector<int>>& prefix,
                  int top, int left, int bottom, int right) {
    if (top >= bottom || left >= right) return 0;
    return prefix[bottom][right] - prefix[top][right]
         - prefix[bottom][left] + prefix[top][left];
}

void rebuild_inactive_prefix() {
    inactive_prefix.assign(n + 1, vector<int>(n + 1, 0));
    for (int row = 0; row < n; ++row) {
        for (int column = 0; column < n; ++column) {
            inactive_prefix[row + 1][column + 1]
                = inactive_prefix[row][column + 1]
                + inactive_prefix[row + 1][column]
                - inactive_prefix[row][column]
                + static_cast<int>(!active_cell[cell_id(row, column)]);
        }
    }
}

bool valid_active_rectangle(int top, int left, int height, int width) {
    const int bottom = top + height;
    const int right = left + width;
    if (top < 0 || left < 0 || bottom > n || right > n) return false;
    return rectangle_sum(inactive_prefix, top, left, bottom, right) == 0
        && rectangle_sum(vertical_wall_prefix, top, left,
                         bottom, right - 1) == 0
        && rectangle_sum(horizontal_wall_prefix, top, left,
                         bottom - 1, right) == 0;
}

bool valid_rectangle(const Operation& operation) {
    const int bottom = operation.row + operation.height;
    const int right = operation.column + operation.width;
    if (operation.row < 0 || operation.column < 0
        || bottom > n || right > n) {
        return false;
    }

    return rectangle_sum(vertical_wall_prefix, operation.row,
                         operation.column, bottom, right - 1) == 0
        && rectangle_sum(horizontal_wall_prefix, operation.row,
                         operation.column, bottom - 1, right) == 0;
}

// 明らかに同値な操作だけを消す・まとめる。可換性を確認してから動かす。
bool same_operation(const Operation& first, const Operation& second) {
    return first.direction == second.direction
        && first.row == second.row
        && first.column == second.column
        && first.height == second.height
        && first.width == second.width;
}

bool disjoint_rectangles(const Operation& first, const Operation& second) {
    return first.row + first.height <= second.row
        || second.row + second.height <= first.row
        || first.column + first.width <= second.column
        || second.column + second.width <= first.column;
}

bool operations_commute(const Operation& first, const Operation& second) {
    if (disjoint_rectangles(first, second)) return true;
    if (first.direction == 'H' && second.direction == 'H'
        && first.column == second.column
        && first.width == second.width) {
        return true;
    }
    if (first.direction == 'V' && second.direction == 'V'
        && first.row == second.row
        && first.height == second.height) {
        return true;
    }
    return false;
}

bool merge_operations(const Operation& first, const Operation& second,
                      Operation& merged) {
    if (first.direction != second.direction) return false;

    if (first.direction == 'H'
        && first.column == second.column
        && first.width == second.width
        && (first.row + first.height == second.row
            || second.row + second.height == first.row)) {
        merged = Operation{'H', min(first.row, second.row), first.column,
                           first.height + second.height, first.width};
        return valid_rectangle(merged);
    }
    if (first.direction == 'V'
        && first.row == second.row
        && first.height == second.height
        && (first.column + first.width == second.column
            || second.column + second.width == first.column)) {
        merged = Operation{'V', first.row, min(first.column, second.column),
                           first.height, first.width + second.width};
        return valid_rectangle(merged);
    }
    return false;
}

vector<Operation> compress_operations(const vector<Operation>& operations) {
    vector<Operation> result;
    result.reserve(operations.size());

    for (const Operation& original : operations) {
        result.push_back(original);
        bool changed = true;
        while (changed && !result.empty()) {
            changed = false;
            const int current_index = static_cast<int>(result.size()) - 1;
            const Operation current = result[current_index];

            for (int earlier = current_index - 1; earlier >= 0; --earlier) {
                if (same_operation(result[earlier], current)) {
                    result.erase(result.begin() + current_index);
                    result.erase(result.begin() + earlier);
                    changed = true;
                    break;
                }

                Operation merged;
                if (merge_operations(result[earlier], current, merged)) {
                    bool can_commute = true;
                    for (int middle = earlier + 1;
                         middle < current_index; ++middle) {
                        if (!operations_commute(result[middle], merged)) {
                            can_commute = false;
                            break;
                        }
                    }
                    if (can_commute) {
                        result[current_index] = merged;
                        result.erase(result.begin() + earlier);
                        changed = true;
                        break;
                    }
                }

                if (!operations_commute(result[earlier], current)) break;
            }
        }
    }
    return result;
}

vector<Operation> operations_moving_card(int from, int to) {
    vector<Operation> result;
    const int from_row = from / n;
    const int from_column = from % n;
    const int to_row = to / n;
    const int to_column = to % n;

    if (from_row == to_row) {
        const int distance = abs(from_column - to_column);
        const auto [left, right] = horizontal_segment(from);
        int first_start;
        int last_start;
        if (to_column > from_column) {
            first_start = max(left, from_column - distance + 1);
            last_start = min(from_column, right - 2 * distance + 1);
        } else {
            first_start = max(left, from_column - 2 * distance + 1);
            last_start = min(from_column - distance,
                             right - 2 * distance + 1);
        }
        for (int start = first_start; start <= last_start; ++start) {
            for (int top = 0; top <= from_row; ++top) {
                for (int bottom = from_row; bottom < n; ++bottom) {
                    const int height = bottom - top + 1;
                    if (valid_active_rectangle(top, start, height, 2 * distance)) {
                        result.push_back(Operation{'H', top, start, height,
                                                   2 * distance});
                    }
                }
            }
        }
    } else {
        const int distance = abs(from_row - to_row);
        const auto [top, bottom] = vertical_segment(from);
        int first_start;
        int last_start;
        if (to_row > from_row) {
            first_start = max(top, from_row - distance + 1);
            last_start = min(from_row, bottom - 2 * distance + 1);
        } else {
            first_start = max(top, from_row - 2 * distance + 1);
            last_start = min(from_row - distance,
                             bottom - 2 * distance + 1);
        }
        for (int start = first_start; start <= last_start; ++start) {
            for (int left = 0; left <= from_column; ++left) {
                for (int right = from_column; right < n; ++right) {
                    const int width = right - left + 1;
                    if (valid_active_rectangle(start, left, 2 * distance, width)) {
                        result.push_back(Operation{'V', start, left,
                                                   2 * distance, width});
                    }
                }
            }
        }
    }
    return result;
}

void apply_best_operation(int from, int to) {
    vector<Operation> candidates = operations_moving_card(from, to);
    assert(!candidates.empty());
    Operation best = candidates[0];
    pair<long long, int> best_quality = operation_quality(best);
    for (int index = 1; index < static_cast<int>(candidates.size()); ++index) {
        const pair<long long, int> quality = operation_quality(candidates[index]);
        if (quality < best_quality) {
            best_quality = quality;
            best = candidates[index];
        }
    }
    apply_operation(best);
}

bool is_geometric_boundary(int cell) {
    const int row = cell / n;
    const int column = cell % n;
    if (row == 0 || row + 1 == n || column == 0 || column + 1 == n) {
        return true;
    }
    return !active_cell[cell - n] || !active_cell[cell + n]
        || !active_cell[cell - 1] || !active_cell[cell + 1];
}

// このマスを確定して取り除いても、残りが連結かを判定する。
vector<char> find_articulation_points() {
    vector<int> discovered(cell_count, -1);
    vector<int> low(cell_count, -1);
    vector<int> parent(cell_count, -1);
    vector<char> articulation(cell_count, false);
    int timer = 0;

    auto dfs = [&](auto&& self, int cell) -> void {
        discovered[cell] = low[cell] = timer++;
        int child_count = 0;
        for (const int next : graph_edges[cell]) {
            if (!active_cell[next]) continue;
            if (discovered[next] == -1) {
                parent[next] = cell;
                ++child_count;
                self(self, next);
                low[cell] = min(low[cell], low[next]);
                if (parent[cell] == -1 && child_count >= 2) {
                    articulation[cell] = true;
                }
                if (parent[cell] != -1 && low[next] >= discovered[cell]) {
                    articulation[cell] = true;
                }
            } else if (next != parent[cell]) {
                low[cell] = min(low[cell], discovered[next]);
            }
        }
    };

    for (int cell = 0; cell < cell_count; ++cell) {
        if (active_cell[cell] && discovered[cell] == -1) {
            dfs(dfs, cell);
        }
    }
    return articulation;
}

// 5種類の剥がし方を、回転・反転した8方向から試す。
int peel_priority(int cell, int style) {
    const int row = cell / n;
    const int column = cell % n;
    int transformed_row = row;
    int transformed_column = column;
    const int transform = style % 8;
    switch (transform) {
        case 0: transformed_row = row; transformed_column = column; break;
        case 1: transformed_row = row; transformed_column = n - 1 - column; break;
        case 2: transformed_row = n - 1 - row; transformed_column = column; break;
        case 3:
            transformed_row = n - 1 - row;
            transformed_column = n - 1 - column;
            break;
        case 4: transformed_row = column; transformed_column = row; break;
        case 5:
            transformed_row = column;
            transformed_column = n - 1 - row;
            break;
        case 6:
            transformed_row = n - 1 - column;
            transformed_column = row;
            break;
        default:
            transformed_row = n - 1 - column;
            transformed_column = n - 1 - row;
            break;
    }
    const int family = style / 8;
    if (family == 0) {
        return transformed_row * n + transformed_column;
    }
    if (family == 1) {
        if (transformed_row % 2 == 1) {
            transformed_column = n - 1 - transformed_column;
        }
        return transformed_row * n + transformed_column;
    }
    if (family == 2 || family == 3) {
        const int row_order = transformed_row * 2 < n
            ? 2 * transformed_row
            : 2 * (n - 1 - transformed_row) + 1;
        if (family == 3 && row_order % 2 == 1) {
            transformed_column = n - 1 - transformed_column;
        }
        return row_order * n + transformed_column;
    }

    const int layer = min(min(transformed_row, transformed_column),
                          min(n - 1 - transformed_row,
                              n - 1 - transformed_column));
    const int side = n - 2 * layer;
    const int before = n * n - side * side;
    if (side == 1) return before;

    int around = 0;
    if (transformed_row == layer) {
        around = transformed_column - layer;
    } else if (transformed_column == n - 1 - layer) {
        around = side - 1 + transformed_row - layer;
    } else if (transformed_row == n - 1 - layer) {
        around = 3 * (side - 1) - (transformed_column - layer);
    } else {
        around = 4 * (side - 1) - (transformed_row - layer);
    }
    return before + around;
}

// 1つの順序で最後まで構築する。確定済みマスを含む操作は決して行わない。
void solve_advanced(int style, int route_variant,
                    chrono::steady_clock::time_point deadline) {
    int remaining = cell_count;

    while (remaining > 1) {
        if (chrono::steady_clock::now() >= deadline) {
            finish_with_adjacent_swaps();
            return;
        }

        const vector<char> articulation = find_articulation_points();
        vector<int> candidates;
        for (int cell = 0; cell < cell_count; ++cell) {
            if (active_cell[cell] && !articulation[cell]
                && is_geometric_boundary(cell)) {
                candidates.push_back(cell);
            }
        }
        if (candidates.empty()) {
            for (int cell = 0; cell < cell_count; ++cell) {
                if (active_cell[cell] && !articulation[cell]) {
                    candidates.push_back(cell);
                }
            }
        }

        int best_target = -1;
        for (const int target : candidates) {
            if (best_target == -1
                || peel_priority(target, style)
                       < peel_priority(best_target, style)) {
                best_target = target;
            }
        }

        assert(best_target != -1);
        rebuild_inactive_prefix();
        rebuild_active_segments();
        vector<vector<int>> macro_graph = build_macro_graph();
        if (route_variant == 1) {
            for (vector<int>& next_cells : macro_graph) {
                reverse(next_cells.begin(), next_cells.end());
            }
        } else if (route_variant == 2) {
            for (int cell = 0; cell < cell_count; ++cell) {
                vector<int>& next_cells = macro_graph[cell];
                int group_begin = 0;
                while (group_begin < static_cast<int>(next_cells.size())) {
                    const bool horizontal = next_cells[group_begin] / n
                                          == cell / n;
                    const bool positive = next_cells[group_begin] > cell;
                    int group_end = group_begin + 1;
                    while (group_end < static_cast<int>(next_cells.size())
                           && (next_cells[group_end] / n == cell / n)
                                  == horizontal
                           && (next_cells[group_end] > cell) == positive) {
                        ++group_end;
                    }
                    reverse(next_cells.begin() + group_begin,
                            next_cells.begin() + group_end);
                    group_begin = group_end;
                }
            }
        }
        const vector<int> best_path = shortest_macro_path(
            macro_graph, position_of_card[best_target], best_target);
        int current = position_of_card[best_target];
        for (const int next : best_path) {
            apply_best_operation(current, next);
            current = next;
        }
        assert(board[best_target] == best_target);
        active_cell[best_target] = false;
        --remaining;
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cin >> n;
    cell_count = n * n;
    board.resize(cell_count);
    position_of_card.resize(cell_count);
    for (int cell = 0; cell < cell_count; ++cell) {
        cin >> board[cell];
        position_of_card[board[cell]] = cell;
    }

    vertical_walls.resize(n);
    horizontal_walls.resize(n - 1);
    for (string& row : vertical_walls) cin >> row;
    for (string& row : horizontal_walls) cin >> row;

    vertical_wall_prefix.assign(n + 1, vector<int>(n + 1, 0));
    horizontal_wall_prefix.assign(n + 1, vector<int>(n + 1, 0));
    for (int row = 0; row < n; ++row) {
        for (int column = 0; column < n; ++column) {
            const int vertical_wall = column + 1 < n
                ? static_cast<int>(vertical_walls[row][column] == '1') : 0;
            const int horizontal_wall = row + 1 < n
                ? static_cast<int>(horizontal_walls[row][column] == '1') : 0;
            vertical_wall_prefix[row + 1][column + 1]
                = vertical_wall_prefix[row][column + 1]
                + vertical_wall_prefix[row + 1][column]
                - vertical_wall_prefix[row][column] + vertical_wall;
            horizontal_wall_prefix[row + 1][column + 1]
                = horizontal_wall_prefix[row][column + 1]
                + horizontal_wall_prefix[row + 1][column]
                - horizontal_wall_prefix[row][column] + horizontal_wall;
        }
    }

    graph_edges.assign(cell_count, {});
    for (int row = 0; row < n; ++row) {
        for (int column = 0; column < n; ++column) {
            const int cell = cell_id(row, column);
            if (column + 1 < n && vertical_walls[row][column] == '0') {
                graph_edges[cell].push_back(cell + 1);
                graph_edges[cell + 1].push_back(cell);
            }
            if (row + 1 < n && horizontal_walls[row][column] == '0') {
                graph_edges[cell].push_back(cell + n);
                graph_edges[cell + n].push_back(cell);
            }
        }
    }

    graph_distance.assign(cell_count, vector<int>(cell_count, cell_count));
    for (int start = 0; start < cell_count; ++start) {
        queue<int> que;
        graph_distance[start][start] = 0;
        que.push(start);
        while (!que.empty()) {
            const int cell = que.front();
            que.pop();
            for (const int next : graph_edges[cell]) {
                if (graph_distance[start][next] != cell_count) continue;
                graph_distance[start][next] = graph_distance[start][cell] + 1;
                que.push(next);
            }
        }
    }

    active_cell.assign(cell_count, true);
    answer.reserve(100000);

#if defined(SIMPLE_BASELINE) || defined(BASELINE)
    finish_with_adjacent_swaps();
#else
    const vector<int> initial_board = board;
    vector<Operation> best_answer;
    const auto search_start = chrono::steady_clock::now();
    const auto deadline = search_start + chrono::milliseconds(1750);

    // まず全40順序を試し、余った時間で最短経路の同長タイも変える。
    for (int attempt = 0; attempt < 120; ++attempt) {
        int style = attempt % 40;
        int route_variant = 0;
        if (attempt >= 80) {
            route_variant = 1;
            if (style == 0) style = 28;
            else if (style == 28) style = 0;
            if (style == 1) style = 3;
            else if (style == 3) style = 1;
        } else if (attempt >= 40) {
            route_variant = 2;
        }
        board = initial_board;
        for (int cell = 0; cell < cell_count; ++cell) {
            position_of_card[board[cell]] = cell;
        }
        active_cell.assign(cell_count, true);
        answer.clear();
        solve_advanced(style, route_variant, deadline);
        answer = compress_operations(answer);
        if (best_answer.empty() || answer.size() < best_answer.size()) {
            best_answer = answer;
#ifdef LOCAL_REPORT
            cerr << "attempt=" << attempt << " style=" << style
                 << " route=" << route_variant
                 << " operations=" << best_answer.size() << '\n';
#endif
        }
        if (chrono::steady_clock::now() >= deadline) break;
    }
    // 圧縮後に実際に出力する列を、初期盤面からもう一度そのまま再生する。
    // これで圧縮規則の誤りも、最後の assert で検出できる。
    board = initial_board;
    for (int cell = 0; cell < cell_count; ++cell) {
        position_of_card[board[cell]] = cell;
    }
    answer.clear();
    for (const Operation& operation : best_answer) {
        assert(valid_rectangle(operation));
        apply_operation(operation);
    }
#endif

    for (int cell = 0; cell < cell_count; ++cell) {
        assert(board[cell] == cell);
    }
    assert(answer.size() <= 100000U);

    for (const Operation& operation : answer) {
        cout << operation.direction << ' ' << operation.row << ' '
             << operation.column << ' ' << operation.height << ' '
             << operation.width << '\n';
    }
    return 0;
}
