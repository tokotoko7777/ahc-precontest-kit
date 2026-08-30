#include <bits/stdc++.h>
using namespace std;

// Define AHC039_RECTANGLE_BASELINE to output only the best grid rectangle.

constexpr int BOARD_SIZE = 100000;
constexpr int MAX_GRID = 200;
constexpr long long MAX_PERIMETER = 400000;

struct Point {
    int x;
    int y;
    int weight;  // mackerel: +1, sardine: -1
};

struct Random {
    uint64_t state;

    explicit Random(uint64_t seed) : state(seed) {}

    uint64_t next() {
        state += 0x9e3779b97f4a7c15ULL;
        uint64_t value = state;
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }

    int next_int(int upper) {
        return static_cast<int>(next() % static_cast<uint64_t>(upper));
    }

    double next_double() {
        return static_cast<double>(next() >> 11)
             * (1.0 / 9007199254740992.0);
    }
};

struct Grid {
    int size = 0;
    vector<int> xs;
    vector<int> ys;
    vector<vector<int>> value;
    vector<vector<int>> column_prefix;
};

// In every active x-column, [low[x], high[x]) is inside the polygon.
struct State {
    int grid_size = 0;
    int left = 0;
    int right = 0;
    array<int, MAX_GRID> low{};
    array<int, MAX_GRID> high{};
    int score = numeric_limits<int>::min();
    long long perimeter = numeric_limits<long long>::max();
};

vector<int> make_fine_lines(const vector<Point>& points, bool use_x) {
    vector<char> forbidden(BOARD_SIZE + 1, false);
    for (const Point& point : points) {
        forbidden[use_x ? point.x : point.y] = true;
    }

    vector<int> lines;
    lines.reserve(MAX_GRID + 1);
    lines.push_back(0);
    for (int index = 1; index < MAX_GRID; ++index) {
        const int target = BOARD_SIZE * index / MAX_GRID;
        const int largest = BOARD_SIZE - (MAX_GRID - index);
        int selected = -1;
        for (int difference = 0; selected < 0; ++difference) {
            const int upper = target + difference;
            if (upper <= largest && upper > lines.back()
                && !forbidden[upper]) {
                selected = upper;
                break;
            }
            const int lower = target - difference;
            if (lower > lines.back() && lower <= largest
                && !forbidden[lower]) {
                selected = lower;
            }
        }
        lines.push_back(selected);
    }
    lines.push_back(BOARD_SIZE);
    return lines;
}

Grid make_grid(const vector<Point>& points, const vector<int>& fine_xs,
               const vector<int>& fine_ys, int size) {
    Grid grid;
    grid.size = size;
    const int step = MAX_GRID / size;
    for (int index = 0; index <= size; ++index) {
        grid.xs.push_back(fine_xs[index * step]);
        grid.ys.push_back(fine_ys[index * step]);
    }

    grid.value.assign(size, vector<int>(size, 0));
    for (const Point& point : points) {
        int column = static_cast<int>(
            upper_bound(grid.xs.begin(), grid.xs.end(), point.x)
            - grid.xs.begin()) - 1;
        int row = static_cast<int>(
            upper_bound(grid.ys.begin(), grid.ys.end(), point.y)
            - grid.ys.begin()) - 1;
        column = clamp(column, 0, size - 1);
        row = clamp(row, 0, size - 1);
        grid.value[column][row] += point.weight;
    }

    grid.column_prefix.assign(size, vector<int>(size + 1, 0));
    for (int column = 0; column < size; ++column) {
        for (int row = 0; row < size; ++row) {
            grid.column_prefix[column][row + 1] =
                grid.column_prefix[column][row] + grid.value[column][row];
        }
    }
    return grid;
}

bool evaluate(State& state, const Grid& grid) {
    if (state.left < 0 || state.right >= grid.size
        || state.left > state.right) {
        return false;
    }

    int score = 0;
    for (int column = state.left; column <= state.right; ++column) {
        if (state.low[column] < 0
            || state.low[column] >= state.high[column]
            || state.high[column] > grid.size) {
            return false;
        }
        if (column > state.left
            && max(state.low[column - 1], state.low[column])
               >= min(state.high[column - 1], state.high[column])) {
            return false;
        }
        score += grid.column_prefix[column][state.high[column]]
               - grid.column_prefix[column][state.low[column]];
    }

    long long perimeter =
        2LL * (grid.xs[state.right + 1] - grid.xs[state.left]);
    perimeter += grid.ys[state.high[state.left]]
               - grid.ys[state.low[state.left]];
    perimeter += grid.ys[state.high[state.right]]
               - grid.ys[state.low[state.right]];
    for (int column = state.left + 1; column <= state.right; ++column) {
        perimeter += abs(grid.ys[state.low[column]]
                         - grid.ys[state.low[column - 1]]);
        perimeter += abs(grid.ys[state.high[column]]
                         - grid.ys[state.high[column - 1]]);
    }
    if (perimeter > MAX_PERIMETER) return false;

    state.grid_size = grid.size;
    state.score = score;
    state.perimeter = perimeter;
    return true;
}

bool is_better(const State& left, const State& right) {
    if (left.score != right.score) return left.score > right.score;
    return left.perimeter < right.perimeter;
}

double search_value(const State& state) {
    // One extra fish is always more important than any perimeter tie-break.
    return static_cast<double>(state.score)
         - static_cast<double>(state.perimeter) / 400001.0;
}

State best_rectangle(const Grid& grid) {
    int best_score = numeric_limits<int>::min();
    long long best_perimeter = numeric_limits<long long>::max();
    int best_left = 0;
    int best_right = 0;
    int best_low = 0;
    int best_high = 1;
    vector<int> column_sum(grid.size, 0);

    for (int low = 0; low < grid.size; ++low) {
        fill(column_sum.begin(), column_sum.end(), 0);
        for (int high = low; high < grid.size; ++high) {
            for (int column = 0; column < grid.size; ++column) {
                column_sum[column] += grid.value[column][high];
            }

            int current_sum = 0;
            int current_left = 0;
            for (int right = 0; right < grid.size; ++right) {
                if (current_sum <= 0) {
                    current_sum = column_sum[right];
                    current_left = right;
                } else {
                    current_sum += column_sum[right];
                }
                const long long perimeter = 2LL
                    * (grid.xs[right + 1] - grid.xs[current_left]
                       + grid.ys[high + 1] - grid.ys[low]);
                if (current_sum > best_score
                    || (current_sum == best_score
                        && perimeter < best_perimeter)) {
                    best_score = current_sum;
                    best_perimeter = perimeter;
                    best_left = current_left;
                    best_right = right;
                    best_low = low;
                    best_high = high + 1;
                }
            }
        }
    }

    State state;
    state.grid_size = grid.size;
    state.left = best_left;
    state.right = best_right;
    for (int column = best_left; column <= best_right; ++column) {
        state.low[column] = best_low;
        state.high[column] = best_high;
    }
    const bool valid = evaluate(state, grid);
    assert(valid);
    return state;
}

State refine(const State& coarse, const Grid& fine) {
    const int ratio = fine.size / coarse.grid_size;
    State result;
    result.grid_size = fine.size;
    result.left = coarse.left * ratio;
    result.right = (coarse.right + 1) * ratio - 1;
    for (int old_column = coarse.left;
         old_column <= coarse.right; ++old_column) {
        for (int offset = 0; offset < ratio; ++offset) {
            const int column = old_column * ratio + offset;
            result.low[column] = coarse.low[old_column] * ratio;
            result.high[column] = coarse.high[old_column] * ratio;
        }
    }
    const bool valid = evaluate(result, fine);
    assert(valid);
    return result;
}

State anneal(State initial, const Grid& grid, int iterations,
             uint64_t seed) {
    Random random(seed);
    State current = initial;
    State best = initial;

    for (int iteration = 0; iteration < iterations; ++iteration) {
        State candidate = current;
        const int width = candidate.right - candidate.left + 1;
        const int move_type = random.next_int(6);

        if (move_type <= 2) {
            const int first = candidate.left + random.next_int(width);
            const int maximum_length = min(8, candidate.right - first + 1);
            const int last = first + random.next_int(maximum_length);
            const int difference = random.next_int(2) == 0 ? -1 : 1;
            for (int column = first; column <= last; ++column) {
                if (move_type == 0 || move_type == 2) {
                    candidate.low[column] += difference;
                }
                if (move_type == 1 || move_type == 2) {
                    candidate.high[column] += difference;
                }
            }
        } else if (move_type == 3) {
            bool extend_left = random.next_int(2) == 0;
            if (extend_left && candidate.left == 0) extend_left = false;
            if (!extend_left && candidate.right + 1 == grid.size) {
                extend_left = true;
            }
            if (extend_left && candidate.left > 0) {
                --candidate.left;
                candidate.low[candidate.left] = candidate.low[candidate.left + 1];
                candidate.high[candidate.left] =
                    candidate.high[candidate.left + 1];
            } else if (!extend_left && candidate.right + 1 < grid.size) {
                ++candidate.right;
                candidate.low[candidate.right] =
                    candidate.low[candidate.right - 1];
                candidate.high[candidate.right] =
                    candidate.high[candidate.right - 1];
            } else {
                continue;
            }
        } else if (move_type == 4) {
            if (width == 1) continue;
            if (random.next_int(2) == 0) ++candidate.left;
            else --candidate.right;
        } else {
            const int difference = random.next_int(2) == 0 ? -1 : 1;
            const bool change_low = random.next_int(2) == 0;
            for (int column = candidate.left;
                 column <= candidate.right; ++column) {
                if (change_low) candidate.low[column] += difference;
                else candidate.high[column] += difference;
            }
        }

        if (!evaluate(candidate, grid)) continue;

        const double progress = static_cast<double>(iteration)
                              / static_cast<double>(iterations);
        const double temperature = 2.0 * pow(0.02 / 2.0, progress);
        const double difference =
            search_value(candidate) - search_value(current);
        if (difference >= 0.0
            || random.next_double() < exp(difference / temperature)) {
            current = candidate;
            if (is_better(current, best)) best = current;
        }
    }
    return best;
}

struct Solution {
    State state;
    Grid grid;
    bool swapped = false;
};

Solution solve_orientation(const vector<Point>& points, bool swapped) {
    vector<Point> oriented = points;
    if (swapped) {
        for (Point& point : oriented) swap(point.x, point.y);
    }
    const vector<int> fine_xs = make_fine_lines(oriented, true);
    const vector<int> fine_ys = make_fine_lines(oriented, false);

    Grid grid25 = make_grid(oriented, fine_xs, fine_ys, 25);
    State state = best_rectangle(grid25);
    state = anneal(state, grid25, 100000,
                   swapped ? 0x391100ULL : 0x390100ULL);

    Grid grid50 = make_grid(oriented, fine_xs, fine_ys, 50);
    State lifted50 = refine(state, grid50);
    State rectangle50 = best_rectangle(grid50);
    state = is_better(rectangle50, lifted50) ? rectangle50 : lifted50;
    state = anneal(state, grid50, 150000,
                   swapped ? 0x392200ULL : 0x390200ULL);

    Grid grid100 = make_grid(oriented, fine_xs, fine_ys, 100);
    State lifted100 = refine(state, grid100);
    State rectangle100 = best_rectangle(grid100);
    state = is_better(rectangle100, lifted100) ? rectangle100 : lifted100;
    state = anneal(state, grid100, 300000,
                   swapped ? 0x393300ULL : 0x390300ULL);

    Grid grid200 = make_grid(oriented, fine_xs, fine_ys, 200);
    State lifted200 = refine(state, grid200);
    State rectangle200 = best_rectangle(grid200);
    State start200 =
        is_better(rectangle200, lifted200) ? rectangle200 : lifted200;
    State first200 = anneal(start200, grid200, 400000,
                            swapped ? 0x394400ULL : 0x390400ULL);
    State second200 = anneal(start200, grid200, 300000,
                             swapped ? 0x395500ULL : 0x390500ULL);
    state = is_better(second200, first200) ? second200 : first200;

    return {state, move(grid200), swapped};
}

vector<pair<int, int>> make_polygon(const Solution& solution) {
    const State& state = solution.state;
    const Grid& grid = solution.grid;
    vector<pair<int, int>> polygon;
    auto add = [&](int x, int y) {
        if (solution.swapped) swap(x, y);
        if (polygon.empty() || polygon.back() != pair<int, int>{x, y}) {
            polygon.push_back({x, y});
        }
    };

    add(grid.xs[state.left], grid.ys[state.low[state.left]]);
    for (int column = state.left; column <= state.right; ++column) {
        add(grid.xs[column + 1], grid.ys[state.low[column]]);
        if (column < state.right
            && state.low[column + 1] != state.low[column]) {
            add(grid.xs[column + 1], grid.ys[state.low[column + 1]]);
        }
    }
    add(grid.xs[state.right + 1], grid.ys[state.high[state.right]]);
    for (int column = state.right; column >= state.left; --column) {
        add(grid.xs[column], grid.ys[state.high[column]]);
        if (column > state.left
            && state.high[column - 1] != state.high[column]) {
            add(grid.xs[column], grid.ys[state.high[column - 1]]);
        }
    }
    return polygon;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int N;
    cin >> N;
    vector<Point> points;
    points.reserve(2 * N);
    for (int index = 0; index < 2 * N; ++index) {
        int x, y;
        cin >> x >> y;
        points.push_back({x, y, index < N ? 1 : -1});
    }

#ifdef AHC039_RECTANGLE_BASELINE
    const vector<int> fine_xs = make_fine_lines(points, true);
    const vector<int> fine_ys = make_fine_lines(points, false);
    Grid grid = make_grid(points, fine_xs, fine_ys, 200);
    Solution answer{best_rectangle(grid), move(grid), false};
#else
    Solution answer = solve_orientation(points, false);
    Solution transposed = solve_orientation(points, true);
    if (is_better(transposed.state, answer.state)) {
        answer = move(transposed);
    }
#endif

    const vector<pair<int, int>> polygon = make_polygon(answer);
    assert(4 <= static_cast<int>(polygon.size()));
    assert(static_cast<int>(polygon.size()) <= 1000);
    long long output_perimeter = 0;
    for (int index = 0; index < static_cast<int>(polygon.size()); ++index) {
        const auto [x1, y1] = polygon[index];
        const auto [x2, y2] =
            polygon[(index + 1) % static_cast<int>(polygon.size())];
        output_perimeter += abs(x1 - x2) + abs(y1 - y2);
    }
    assert(output_perimeter == answer.state.perimeter);
    assert(output_perimeter <= MAX_PERIMETER);
    cout << polygon.size() << '\n';
    for (const auto& [x, y] : polygon) cout << x << ' ' << y << '\n';
}
