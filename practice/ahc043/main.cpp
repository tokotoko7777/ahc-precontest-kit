#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <queue>
#include <tuple>
#include <utility>
#include <vector>

using namespace std;

using int64 = long long;

#ifndef INITIAL_CANDIDATES
#define INITIAL_CANDIDATES 96
#endif

const int EMPTY = -1;
const int STATION = 0;
const int COST_RAIL = 100;
const int COST_STATION = 5000;

struct Position {
    int row;
    int col;
};

struct Action {
    int type;
    int row;
    int col;
};

int manhattan(Position left, Position right) {
    return abs(left.row - right.row) + abs(left.col - right.col);
}

struct Problem {
    int n;
    int m;
    int64 initial_money;
    int turn_count;
    vector<Position> home;
    vector<Position> work;
    vector<int> fare;
    vector<vector<int>> cells_near_home;
    vector<vector<int>> cells_near_work;
    vector<vector<int>> people_near_home;
    vector<vector<int>> people_near_work;

    int cell(Position position) const {
        return position.row * n + position.col;
    }

    Position position(int cell_id) const {
        return {cell_id / n, cell_id % n};
    }

    vector<int> nearby_cells(Position center) const {
        vector<int> result;
        for (int dr = -2; dr <= 2; ++dr) {
            for (int dc = -2; dc <= 2; ++dc) {
                if (abs(dr) + abs(dc) > 2) continue;
                int row = center.row + dr;
                int col = center.col + dc;
                if (0 <= row && row < n && 0 <= col && col < n) {
                    result.push_back(row * n + col);
                }
            }
        }
        return result;
    }

    void prepare() {
        int cells = n * n;
        fare.resize(m);
        cells_near_home.resize(m);
        cells_near_work.resize(m);
        people_near_home.assign(cells, {});
        people_near_work.assign(cells, {});

        for (int person = 0; person < m; ++person) {
            fare[person] = manhattan(home[person], work[person]);
            cells_near_home[person] = nearby_cells(home[person]);
            cells_near_work[person] = nearby_cells(work[person]);
            for (int cell_id : cells_near_home[person]) {
                people_near_home[cell_id].push_back(person);
            }
            for (int cell_id : cells_near_work[person]) {
                people_near_work[cell_id].push_back(person);
            }
        }
    }
};

vector<Position> make_l_path(Position start, Position goal, int orientation) {
    vector<Position> path;
    path.push_back(start);
    Position current = start;

    auto move_rows = [&]() {
        while (current.row != goal.row) {
            current.row += (current.row < goal.row ? 1 : -1);
            path.push_back(current);
        }
    };
    auto move_columns = [&]() {
        while (current.col != goal.col) {
            current.col += (current.col < goal.col ? 1 : -1);
            path.push_back(current);
        }
    };

    if (orientation == 0) {
        move_rows();
        move_columns();
    } else {
        move_columns();
        move_rows();
    }
    return path;
}

int rail_type(Position previous, Position current, Position next) {
    bool left = previous.col < current.col || next.col < current.col;
    bool right = previous.col > current.col || next.col > current.col;
    bool up = previous.row < current.row || next.row < current.row;
    bool down = previous.row > current.row || next.row > current.row;

    if (left && right) return 1;
    if (up && down) return 2;
    if (left && down) return 3;
    if (left && up) return 4;
    if (right && up) return 5;
    if (right && down) return 6;
    assert(false);
    return -1;
}

struct Plan {
    const Problem& problem;
    vector<int> grid;
    vector<Action> actions;
    vector<char> home_covered;
    vector<char> work_covered;
    vector<char> served;
    vector<int> stations;
    int64 money;
    int64 income;
    int turn;

    explicit Plan(const Problem& input)
        : problem(input),
          grid(input.n * input.n, EMPTY),
          home_covered(input.m, false),
          work_covered(input.m, false),
          served(input.m, false),
          money(input.initial_money),
          income(0),
          turn(0) {}

    bool wait_until_affordable(int cost) {
        while (money < cost && turn < problem.turn_count) {
            if (income == 0) return false;
            actions.push_back({-1, -1, -1});
            money += income;
            ++turn;
        }
        return money >= cost && turn < problem.turn_count;
    }

    void update_coverage(int cell_id) {
        for (int person : problem.people_near_home[cell_id]) {
            home_covered[person] = true;
        }
        for (int person : problem.people_near_work[cell_id]) {
            work_covered[person] = true;
        }

        income = 0;
        for (int person = 0; person < problem.m; ++person) {
            served[person] = home_covered[person] && work_covered[person];
            if (served[person]) income += problem.fare[person];
        }
    }

    bool build_rail(int type, int cell_id) {
        if (!wait_until_affordable(COST_RAIL)) return false;
        assert(grid[cell_id] == EMPTY);
        Position place = problem.position(cell_id);
        grid[cell_id] = type;
        actions.push_back({type, place.row, place.col});
        money -= COST_RAIL;
        money += income;
        ++turn;
        return true;
    }

    bool build_station(int cell_id) {
        if (!wait_until_affordable(COST_STATION)) return false;
        assert(grid[cell_id] != STATION);
        Position place = problem.position(cell_id);
        grid[cell_id] = STATION;
        stations.push_back(cell_id);
        actions.push_back({STATION, place.row, place.col});
        money -= COST_STATION;
        update_coverage(cell_id);
        money += income;
        ++turn;
        return true;
    }

    bool build_initial_line(int first_cell, int second_cell, int orientation) {
        Position first = problem.position(first_cell);
        Position second = problem.position(second_cell);
        vector<Position> path = make_l_path(first, second, orientation);

        for (int index = 1; index + 1 < static_cast<int>(path.size()); ++index) {
            int type = rail_type(path[index - 1], path[index], path[index + 1]);
            if (!build_rail(type, problem.cell(path[index]))) return false;
        }
        if (!build_station(first_cell)) return false;
        if (!build_station(second_cell)) return false;
        return true;
    }

    // 現在の全駅から、空きセルだけを通る最短路を一度に調べます。
    void shortest_paths(vector<int>& distance, vector<int>& previous) const {
        int cells = problem.n * problem.n;
        distance.assign(cells, -1);
        previous.assign(cells, -1);
        queue<int> pending;
        for (int station : stations) {
            distance[station] = 0;
            pending.push(station);
        }

        const array<int, 4> dr = {-1, 1, 0, 0};
        const array<int, 4> dc = {0, 0, -1, 1};
        while (!pending.empty()) {
            int current = pending.front();
            pending.pop();
            Position place = problem.position(current);
            for (int direction = 0; direction < 4; ++direction) {
                int row = place.row + dr[direction];
                int col = place.col + dc[direction];
                if (row < 0 || problem.n <= row || col < 0 || problem.n <= col) continue;
                int next = row * problem.n + col;
                if (distance[next] != -1 || grid[next] != EMPTY) continue;
                distance[next] = distance[current] + 1;
                previous[next] = current;
                pending.push(next);
            }
        }
    }

    vector<int64> added_income_by_cell() const {
        vector<int64> added(problem.n * problem.n, 0);
        for (int person = 0; person < problem.m; ++person) {
            if (served[person]) continue;
            if (home_covered[person]) {
                for (int cell_id : problem.cells_near_work[person]) {
                    added[cell_id] += problem.fare[person];
                }
            } else if (work_covered[person]) {
                for (int cell_id : problem.cells_near_home[person]) {
                    added[cell_id] += problem.fare[person];
                }
            }
        }
        return added;
    }

    // 延伸中にも現在の収入は入り続けます。資金不足なら必要なターンだけ待ち、
    // 新駅完成後は増えた収入で最後まで待った場合の資金を返します。
    int64 projected_final_money(int rail_count, int64 added_income) const {
        int simulated_turn = turn;
        int64 simulated_money = money;

        auto simulate_action = [&](int cost, bool completes_station) {
            if (simulated_money < cost) {
                if (income == 0) return false;
                int64 shortage = cost - simulated_money;
                int64 waits = (shortage + income - 1) / income;
                if (simulated_turn + waits >= problem.turn_count) return false;
                simulated_turn += static_cast<int>(waits);
                simulated_money += waits * income;
            }
            if (simulated_turn >= problem.turn_count) return false;
            simulated_money -= cost;
            simulated_money += income + (completes_station ? added_income : 0);
            ++simulated_turn;
            return true;
        };

        for (int rail = 0; rail < rail_count; ++rail) {
            if (!simulate_action(COST_RAIL, false)) return -1;
        }
        if (!simulate_action(COST_STATION, true)) return -1;

        return simulated_money +
               (income + added_income) * (problem.turn_count - simulated_turn);
    }

    bool build_best_extension() {
        vector<int> distance;
        vector<int> previous;
        shortest_paths(distance, previous);
        vector<int64> added = added_income_by_cell();

        int64 stop_now = money + income * (problem.turn_count - turn);
        int best_cell = -1;
        int best_rail_count = -1;
        int64 best_final = stop_now;

        for (int cell_id = 0; cell_id < problem.n * problem.n; ++cell_id) {
            if (grid[cell_id] == STATION || added[cell_id] == 0) continue;
            int rail_count;
            if (grid[cell_id] == EMPTY) {
                if (distance[cell_id] <= 0) continue;
                rail_count = distance[cell_id] - 1;
            } else {
                // 既存線路を駅へ置き換えるだけなら、追加線路は不要です。
                rail_count = 0;
            }

            int64 final_money = projected_final_money(rail_count, added[cell_id]);
            if (final_money > best_final ||
                (final_money == best_final && best_cell != -1 &&
                 make_tuple(-added[cell_id], rail_count, cell_id) <
                     make_tuple(-added[best_cell], best_rail_count, best_cell))) {
                best_final = final_money;
                best_cell = cell_id;
                best_rail_count = rail_count;
            }
        }

        if (best_cell == -1) return false;

        if (grid[best_cell] == EMPTY) {
            vector<int> reverse_path;
            int current = best_cell;
            while (distance[current] > 0) {
                reverse_path.push_back(current);
                current = previous[current];
            }
            reverse_path.push_back(current);
            reverse(reverse_path.begin(), reverse_path.end());

            for (int index = 1; index + 1 < static_cast<int>(reverse_path.size()); ++index) {
                Position previous_place = problem.position(reverse_path[index - 1]);
                Position current_place = problem.position(reverse_path[index]);
                Position next_place = problem.position(reverse_path[index + 1]);
                int type = rail_type(previous_place, current_place, next_place);
                if (!build_rail(type, reverse_path[index])) return false;
            }
        }
        return build_station(best_cell);
    }

    void expand_while_profitable() {
        while (turn < problem.turn_count && build_best_extension()) {
        }
    }

    void finish_with_waits() {
        while (turn < problem.turn_count) {
            actions.push_back({-1, -1, -1});
            money += income;
            ++turn;
        }
    }
};

vector<tuple<int64, int, int>> best_initial_pairs(const Problem& problem) {
    int cells = problem.n * problem.n;
    vector<int> pair_income(cells * cells, 0);

    for (int person = 0; person < problem.m; ++person) {
        for (int first : problem.cells_near_home[person]) {
            for (int second : problem.cells_near_work[person]) {
                if (first == second) continue;
                pair_income[first * cells + second] += problem.fare[person];
                pair_income[second * cells + first] += problem.fare[person];
            }
        }
    }

    using Candidate = tuple<int64, int, int>;
    priority_queue<Candidate, vector<Candidate>, greater<Candidate>> best;
    for (int first = 0; first < cells; ++first) {
        for (int second = first + 1; second < cells; ++second) {
            int route_income = pair_income[first * cells + second];
            if (route_income == 0) continue;
            int distance = manhattan(problem.position(first), problem.position(second));
            int64 cost = 2LL * COST_STATION + 1LL * COST_RAIL * (distance - 1);
            if (cost > problem.initial_money) continue;

            // d+1回の建設の最後に開通し、そのターンを含めて T-d 回集金します。
            int64 final_money = problem.initial_money - cost +
                                1LL * route_income * (problem.turn_count - distance);
            Candidate candidate = {final_money, first, second};
            if (static_cast<int>(best.size()) < INITIAL_CANDIDATES) {
                best.push(candidate);
            } else if (candidate > best.top()) {
                best.pop();
                best.push(candidate);
            }
        }
    }

    vector<Candidate> result;
    while (!best.empty()) {
        result.push_back(best.top());
        best.pop();
    }
    reverse(result.begin(), result.end());
    return result;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Problem problem;
    cin >> problem.n >> problem.m >> problem.initial_money >> problem.turn_count;
    problem.home.resize(problem.m);
    problem.work.resize(problem.m);
    for (int person = 0; person < problem.m; ++person) {
        cin >> problem.home[person].row >> problem.home[person].col
            >> problem.work[person].row >> problem.work[person].col;
    }
    problem.prepare();

#ifdef WAIT_BASELINE
    for (int turn = 0; turn < problem.turn_count; ++turn) cout << -1 << '\n';
    return 0;
#endif

    vector<tuple<int64, int, int>> initial_pairs = best_initial_pairs(problem);
    int64 best_money = -1;
    vector<Action> best_actions;

    for (const auto& candidate : initial_pairs) {
        int first = get<1>(candidate);
        int second = get<2>(candidate);
        for (int orientation = 0; orientation < 2; ++orientation) {
            Plan plan(problem);
            if (!plan.build_initial_line(first, second, orientation)) continue;
#ifndef SINGLE_LINE_ONLY
            plan.expand_while_profitable();
#endif
            plan.finish_with_waits();
            if (plan.money > best_money) {
                best_money = plan.money;
                best_actions = plan.actions;
            }
        }
    }

    // 公式生成では必ず初期資金で作れる通勤路があります。
    // 念のため候補が無い場合も、800ターン待機して合法性を保ちます。
    if (best_actions.empty()) {
        best_actions.assign(problem.turn_count, {-1, -1, -1});
    }

    for (const Action& action : best_actions) {
        if (action.type == -1) {
            cout << -1 << '\n';
        } else {
            cout << action.type << ' ' << action.row << ' ' << action.col << '\n';
        }
    }

    return 0;
}
