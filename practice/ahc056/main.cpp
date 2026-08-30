#include <bits/stdc++.h>
using namespace std;

// AHC056 "Grid Turing Robot"
//
// 1. Connect consecutive destinations by shortest paths.
// 2. Give every time t a unique pair (color[t], state[t]).
// 3. When leaving a cell, paint it with the color needed on the next visit.
//
// Because every (color, state) pair is unique, its transition rule can never
// conflict with another time step.

struct Point {
    int row;
    int col;
};

int n, destination_count, turn_limit;
vector<string> vertical_wall;
vector<string> horizontal_wall;

bool can_move(int row, int col, int direction) {
    // direction: 0=U, 1=D, 2=L, 3=R
    if (direction == 0) {
        return row > 0 && horizontal_wall[row - 1][col] == '0';
    }
    if (direction == 1) {
        return row + 1 < n && horizontal_wall[row][col] == '0';
    }
    if (direction == 2) {
        return col > 0 && vertical_wall[row][col - 1] == '0';
    }
    return col + 1 < n && vertical_wall[row][col] == '0';
}

vector<char> shortest_path(Point start, Point goal) {
    static const int DR[4] = {-1, 1, 0, 0};
    static const int DC[4] = {0, 0, -1, 1};
    static const char COMMAND[4] = {'U', 'D', 'L', 'R'};

    int start_id = start.row * n + start.col;
    int goal_id = goal.row * n + goal.col;

    vector<int> previous(n * n, -1);
    vector<char> command_from_previous(n * n);
    queue<int> que;

    previous[start_id] = start_id;
    que.push(start_id);

    while (!que.empty() && previous[goal_id] == -1) {
        int id = que.front();
        que.pop();
        int row = id / n;
        int col = id % n;

        for (int d = 0; d < 4; ++d) {
            if (!can_move(row, col, d)) continue;
            int next_row = row + DR[d];
            int next_col = col + DC[d];
            int next_id = next_row * n + next_col;
            if (previous[next_id] != -1) continue;

            previous[next_id] = id;
            command_from_previous[next_id] = COMMAND[d];
            que.push(next_id);
        }
    }

    vector<char> path;
    for (int id = goal_id; id != start_id; id = previous[id]) {
        path.push_back(command_from_previous[id]);
    }
    reverse(path.begin(), path.end());
    return path;
}

Point moved(Point p, char command) {
    if (command == 'U') --p.row;
    if (command == 'D') ++p.row;
    if (command == 'L') --p.col;
    if (command == 'R') ++p.col;
    return p;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cin >> n >> destination_count >> turn_limit;

    vertical_wall.resize(n);
    for (string& row : vertical_wall) cin >> row;

    horizontal_wall.resize(n - 1);
    for (string& row : horizontal_wall) cin >> row;

    vector<Point> destination(destination_count);
    for (Point& p : destination) cin >> p.row >> p.col;

    // route_cell[t] is the robot's cell immediately before move t.
    // There is one extra cell after the final move.
    vector<int> route_cell;
    vector<char> move_command;
    route_cell.reserve(turn_limit + 1);
    move_command.reserve(turn_limit);

    Point current = destination[0];
    route_cell.push_back(current.row * n + current.col);

    for (int k = 0; k + 1 < destination_count; ++k) {
        vector<char> path = shortest_path(current, destination[k + 1]);
        for (char command : path) {
            move_command.push_back(command);
            current = moved(current, command);
            route_cell.push_back(current.row * n + current.col);
        }
    }

    int route_length = int(move_command.size());

    // The destinations are distinct, so official inputs always have a
    // nonempty route. Keep this branch to make the construction self-contained.
    if (route_length == 0) {
        cout << "1 1 0\n";
        for (int row = 0; row < n; ++row) {
            for (int col = 0; col < n; ++col) {
                if (col) cout << ' ';
                cout << 0;
            }
            cout << '\n';
        }
        return 0;
    }

    // We need at least route_length distinct (color, state) pairs.
    // Try every state count and choose the smallest C + Q.
    int color_count = route_length;
    int state_count = 1;
    for (int q = 1; q <= route_length; ++q) {
        int c = (route_length + q - 1) / q;
        if (c + q < color_count + state_count) {
            color_count = c;
            state_count = q;
        }
    }

    auto color_at = [&](int time) { return time / state_count; };
    auto state_at = [&](int time) { return time % state_count; };

    // next_visit[t] is the next time the cell of step t is used again.
    // A backward scan obtains both next visits and each cell's first visit.
    vector<int> next_visit(route_length, -1);
    vector<int> first_visit(n * n, -1);
    for (int time = route_length - 1; time >= 0; --time) {
        int cell = route_cell[time];
        next_visit[time] = first_visit[cell];
        first_visit[cell] = time;
    }

    vector<vector<int>> initial_color(n, vector<int>(n, 0));
    for (int cell = 0; cell < n * n; ++cell) {
        if (first_visit[cell] != -1) {
            initial_color[cell / n][cell % n] = color_at(first_visit[cell]);
        }
    }

    cout << color_count << ' ' << state_count << ' ' << route_length << '\n';
    for (int row = 0; row < n; ++row) {
        for (int col = 0; col < n; ++col) {
            if (col) cout << ' ';
            cout << initial_color[row][col];
        }
        cout << '\n';
    }

    for (int time = 0; time < route_length; ++time) {
        int input_color = color_at(time);
        int input_state = state_at(time);

        int paint_color = 0;
        if (next_visit[time] != -1) {
            paint_color = color_at(next_visit[time]);
        }

        int next_state = 0;
        if (time + 1 < route_length) {
            next_state = state_at(time + 1);
        }

        cout << input_color << ' ' << input_state << ' '
             << paint_color << ' ' << next_state << ' '
             << move_command[time] << '\n';
    }

    return 0;
}
