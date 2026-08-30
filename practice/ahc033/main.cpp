#include <bits/stdc++.h>
using namespace std;

struct Point {
    int row;
    int column;
};

int manhattan(Point a, Point b) {
    return abs(a.row - b.row) + abs(a.column - b.column);
}

struct Terminal {
    int n;
    vector<vector<int>> arrival_order;
    vector<int> next_arrival;
    vector<vector<int>> board;
    vector<unsigned char> dispatched;
    Point crane{0, 0};
    int held = -1;
    string operations;

    explicit Terminal(const vector<vector<int>>& input)
        : n(static_cast<int>(input.size())),
          arrival_order(input),
          next_arrival(n, 0),
          board(n, vector<int>(n, -1)),
          dispatched(n * n, 0) {
        receive_containers();
    }

    void receive_containers() {
        for (int row = 0; row < n; ++row) {
            if (next_arrival[row] == n || board[row][0] != -1) continue;
            if (held != -1 && crane.row == row && crane.column == 0) {
                continue;
            }
            board[row][0] = arrival_order[row][next_arrival[row]++];
        }
    }

    void dispatch_containers() {
        for (int row = 0; row < n; ++row) {
            const int box = board[row][n - 1];
            if (box == -1) continue;
            dispatched[box] = 1;
            board[row][n - 1] = -1;
        }
    }

    void step(char action) {
        if (action == 'P') {
            assert(held == -1);
            assert(board[crane.row][crane.column] != -1);
            held = board[crane.row][crane.column];
            board[crane.row][crane.column] = -1;
        } else if (action == 'Q') {
            assert(held != -1);
            assert(board[crane.row][crane.column] == -1);
            board[crane.row][crane.column] = held;
            held = -1;
        } else if (action == 'U') {
            assert(crane.row > 0);
            --crane.row;
        } else if (action == 'D') {
            assert(crane.row + 1 < n);
            ++crane.row;
        } else if (action == 'L') {
            assert(crane.column > 0);
            --crane.column;
        } else if (action == 'R') {
            assert(crane.column + 1 < n);
            ++crane.column;
        } else {
            assert(action == '.');
        }

        operations.push_back(action);
        dispatch_containers();
        // This prepares the board at the beginning of the next turn.
        receive_containers();
    }

    void move_to(Point goal) {
        while (crane.row > goal.row) step('U');
        while (crane.row < goal.row) step('D');
        while (crane.column > goal.column) step('L');
        while (crane.column < goal.column) step('R');
    }

    void carry(Point from, Point to) {
        move_to(from);
        step('P');
        move_to(to);
        step('Q');
    }

    Point find_box(int box) const {
        for (int row = 0; row < n; ++row) {
            for (int column = 0; column < n; ++column) {
                if (board[row][column] == box) return {row, column};
            }
        }
        return {-1, -1};
    }

    int next_needed(int dispatch_row) const {
        const int first = dispatch_row * n;
        for (int box = first; box < first + n; ++box) {
            if (!dispatched[box]) return box;
        }
        return -1;
    }

    int dispatched_count() const {
        return accumulate(dispatched.begin(), dispatched.end(), 0);
    }
};

Point choose_visible_target(const Terminal& terminal) {
    Point best{-1, -1};
    int best_distance = numeric_limits<int>::max();

    for (int dispatch_row = 0; dispatch_row < terminal.n; ++dispatch_row) {
        const int box = terminal.next_needed(dispatch_row);
        if (box == -1) continue;
        const Point position = terminal.find_box(box);
        if (position.row == -1) continue;
        const Point exit{dispatch_row, terminal.n - 1};
        const int distance =
            manhattan(terminal.crane, position) +
            manhattan(position, exit);
        if (distance < best_distance) {
            best_distance = distance;
            best = position;
        }
    }
    return best;
}

int choose_gate_to_clear(
    const Terminal& terminal,
    const vector<int>& source_row,
    const vector<int>& source_order
) {
    int best_gate = -1;
    int best_depth = numeric_limits<int>::max();
    int best_crane_distance = numeric_limits<int>::max();

    for (int dispatch_row = 0; dispatch_row < terminal.n; ++dispatch_row) {
        const int box = terminal.next_needed(dispatch_row);
        if (box == -1 || terminal.find_box(box).row != -1) continue;

        const int gate = source_row[box];
        if (terminal.board[gate][0] == -1) continue;
        const int depth =
            source_order[box] - terminal.next_arrival[gate] + 1;
        const int crane_distance =
            manhattan(terminal.crane, Point{gate, 0});
        if (
            depth < best_depth ||
            (depth == best_depth && crane_distance < best_crane_distance)
        ) {
            best_depth = depth;
            best_crane_distance = crane_distance;
            best_gate = gate;
        }
    }

    if (best_gate != -1) return best_gate;
    for (int row = 0; row < terminal.n; ++row) {
        if (terminal.board[row][0] != -1) return row;
    }
    return -1;
}

Point choose_storage(const Terminal& terminal, int box, int source_gate) {
    const int future_dispatch_row = box / terminal.n;
    Point best{-1, -1};
    int best_value = numeric_limits<int>::max();

    for (int row = 0; row < terminal.n; ++row) {
        for (int column = 1; column + 1 < terminal.n; ++column) {
            if (terminal.board[row][column] != -1) continue;
            const int value =
                20 * abs(row - future_dispatch_row) +
                abs(row - source_gate) + column;
            if (value < best_value) {
                best_value = value;
                best = {row, column};
            }
        }
    }
    return best;
}

void solve_in_order(
    Terminal& terminal,
    const vector<int>& source_row,
    const vector<int>& source_order
) {
    while (terminal.dispatched_count() < terminal.n * terminal.n) {
        const Point target = choose_visible_target(terminal);
        if (target.row != -1) {
            const int box = terminal.board[target.row][target.column];
            terminal.carry(
                target,
                Point{box / terminal.n, terminal.n - 1}
            );
            continue;
        }

        const int gate = choose_gate_to_clear(
            terminal,
            source_row,
            source_order
        );
        assert(gate != -1);
        const int box = terminal.board[gate][0];
        Point storage = choose_storage(terminal, box, gate);

        if (storage.row == -1) {
            // This fallback still dispatches every box legally.  It is only
            // needed if all 15 storage cells become full at once.
            terminal.carry(
                Point{gate, 0},
                Point{box / terminal.n, terminal.n - 1}
            );
        } else {
            terminal.carry(Point{gate, 0}, storage);
        }
    }
}

void solve_direct(Terminal& terminal) {
    while (terminal.dispatched_count() < terminal.n * terminal.n) {
        int best_gate = -1;
        int best_distance = numeric_limits<int>::max();
        for (int row = 0; row < terminal.n; ++row) {
            if (terminal.board[row][0] == -1) continue;
            const int distance =
                manhattan(terminal.crane, Point{row, 0});
            if (distance < best_distance) {
                best_distance = distance;
                best_gate = row;
            }
        }
        assert(best_gate != -1);
        const int box = terminal.board[best_gate][0];
        terminal.carry(
            Point{best_gate, 0},
            Point{box / terminal.n, terminal.n - 1}
        );
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n;
    cin >> n;
    vector<vector<int>> arrival_order(n, vector<int>(n));
    vector<int> source_row(n * n);
    vector<int> source_order(n * n);
    for (int row = 0; row < n; ++row) {
        for (int order = 0; order < n; ++order) {
            cin >> arrival_order[row][order];
            const int box = arrival_order[row][order];
            source_row[box] = row;
            source_order[box] = order;
        }
    }

    Terminal terminal(arrival_order);
#ifdef BASELINE_POLICY
    solve_direct(terminal);
#else
    solve_in_order(terminal, source_row, source_order);
#endif

    assert(terminal.held == -1);
    assert(terminal.operations.size() <= 10000);
    cout << terminal.operations << '\n';
    for (int crane = 1; crane < n; ++crane) cout << "B\n";
    return 0;
}
