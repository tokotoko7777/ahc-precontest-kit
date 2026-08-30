#include <bits/stdc++.h>
using namespace std;

// AHC018: Excavation
//
// The final submitted file must be this file alone.  It talks directly to the
// interactive judge, so remember to flush after every query (endl does that).

constexpr int GRID_SIZE = 200;
constexpr int CELL_COUNT = GRID_SIZE * GRID_SIZE;
constexpr int UNKNOWN_DISTANCE = 1'000'000;
constexpr int DEFAULT_HARDNESS = 1000;
constexpr int REPLAN_INTERVAL = 8;

struct Point {
    int row;
    int column;
};

int water_source_count;
int house_count;
int fixed_cost;
vector<Point> water_sources;
vector<Point> houses;

array<unsigned char, CELL_COUNT> broken{};
array<unsigned char, CELL_COUNT> connected_to_water{};
array<unsigned char, CELL_COUNT> house_cell{};
array<int, CELL_COUNT> nearest_observation_distance{};
array<int, CELL_COUNT> nearest_observation_hardness{};

int cell_id(int row, int column) {
    return row * GRID_SIZE + column;
}

Point point_of(int id) {
    return {id / GRID_SIZE, id % GRID_SIZE};
}

int excavation_unit(int expected_hardness) {
    // If the true hardness were known to be H and every query used P,
    // the extra cost is roughly P/2 + C*H/P.  Its minimum is near
    // sqrt(2*C*H).  The clamps keep the interaction robust at both ends.
    const double value = sqrt(
        2.0 * static_cast<double>(fixed_cost) *
        static_cast<double>(max(expected_hardness, 10))
    );
    return clamp(static_cast<int>(lround(value)), 8, 1200);
}

void add_observation(Point point, int estimated_hardness) {
    // AHC018's hardness field is spatially smooth.  For each cell we retain
    // the nearest measured value.  40,000 cells times the number of dug cells
    // is small enough, and this deliberately avoids a complicated data type.
    for (int row = 0; row < GRID_SIZE; ++row) {
        const int row_distance = abs(row - point.row);
        for (int column = 0; column < GRID_SIZE; ++column) {
            const int distance = row_distance + abs(column - point.column);
            const int id = cell_id(row, column);
            if (distance < nearest_observation_distance[id]) {
                nearest_observation_distance[id] = distance;
                nearest_observation_hardness[id] = estimated_hardness;
            }
        }
    }
}

int predicted_hardness(int id) {
    if (broken[id] != 0U) return 0;
    const int distance = nearest_observation_distance[id];
    if (distance == UNKNOWN_DISTANCE) return DEFAULT_HARDNESS;

    // Nearby measurements dominate.  Far away, shrink the estimate back to a
    // safe global prior; terminal cells are unusually soft in the generator.
    constexpr int CORRELATION_LENGTH = 12;
    const long long numerator =
        static_cast<long long>(CORRELATION_LENGTH) *
            nearest_observation_hardness[id] +
        static_cast<long long>(distance) * DEFAULT_HARDNESS;
    const int estimate = static_cast<int>(
        numerator / (CORRELATION_LENGTH + distance)
    );
    return clamp(estimate, 10, 5000);
}

[[noreturn]] void finish_now() {
    exit(0);
}

void excavate(Point point, bool use_nearby_prediction) {
    const int id = cell_id(point.row, point.column);
    if (broken[id] != 0U) return;

    const int prediction = predicted_hardness(id);
    const int observation_distance = nearest_observation_distance[id];
    const int unit = excavation_unit(prediction);
    int accumulated_power = 0;
    int lower_bound = 0;
    bool first_query = true;

    while (true) {
        int power = unit;

        // At high C, one extra query is expensive.  An adjacent observation is
        // then useful as a direct first guess.  For low C, small repeated digs
        // are safer against the occasional steep local change.
        if (
            first_query && use_nearby_prediction &&
            observation_distance <= 1 && fixed_cost == 128
        ) {
            power = max(unit, prediction);
        }
        power = clamp(power, 1, 5000);

        cout << point.row << ' ' << point.column << ' ' << power << endl;
        int response = -1;
        if (!(cin >> response)) finish_now();
        if (response == -1 || response == 2) finish_now();

        accumulated_power += power;
        if (response == 0) {
            lower_bound = accumulated_power;
            first_query = false;
            continue;
        }

        broken[id] = 1U;
        const int upper_bound = accumulated_power;
        const int estimate = (lower_bound + 1 + upper_bound) / 2;
        add_observation(point, estimate);
        return;
    }
}

bool every_house_has_water() {
    for (const Point house : houses) {
        if (connected_to_water[cell_id(house.row, house.column)] == 0U) {
            return false;
        }
    }
    return true;
}

long long estimated_cell_cost(int id) {
    if (broken[id] != 0U) return 0;
    const int hardness = predicted_hardness(id);
    const int overhead = static_cast<int>(lround(sqrt(
        2.0 * static_cast<double>(fixed_cost) *
        static_cast<double>(hardness)
    )));
    return static_cast<long long>(hardness + overhead);
}

vector<int> shortest_route_to_a_house() {
    constexpr long long INF = numeric_limits<long long>::max() / 4;
    vector<long long> distance(CELL_COUNT, INF);
    vector<int> parent(CELL_COUNT, -1);
    priority_queue<
        pair<long long, int>,
        vector<pair<long long, int>>,
        greater<pair<long long, int>>
    > queue;

    for (int id = 0; id < CELL_COUNT; ++id) {
        if (connected_to_water[id] != 0U) {
            distance[id] = 0;
            queue.emplace(0, id);
        }
    }

    static constexpr int DR[4] = {-1, 1, 0, 0};
    static constexpr int DC[4] = {0, 0, -1, 1};
    int goal = -1;

    while (!queue.empty()) {
        const auto [current_distance, id] = queue.top();
        queue.pop();
        if (current_distance != distance[id]) continue;
        if (
            house_cell[id] != 0U &&
            connected_to_water[id] == 0U
        ) {
            goal = id;
            break;
        }

        const Point point = point_of(id);
        for (int direction = 0; direction < 4; ++direction) {
            const int next_row = point.row + DR[direction];
            const int next_column = point.column + DC[direction];
            if (
                next_row < 0 || next_row >= GRID_SIZE ||
                next_column < 0 || next_column >= GRID_SIZE
            ) {
                continue;
            }
            const int next = cell_id(next_row, next_column);
            const long long next_distance =
                current_distance + estimated_cell_cost(next);
            if (next_distance < distance[next]) {
                distance[next] = next_distance;
                parent[next] = id;
                queue.emplace(next_distance, next);
            }
        }
    }

    vector<int> route;
    if (goal == -1) return route;
    int id = goal;
    while (id != -1 && connected_to_water[id] == 0U) {
        route.push_back(id);
        id = parent[id];
    }
    if (id == -1) {
        route.clear();
        return route;
    }
    reverse(route.begin(), route.end());
    return route;
}

void solve_adaptive_network() {
    // All terminal cells must eventually be broken.  Measuring them first
    // gives Dijkstra some information before it chooses a watercourse.
    for (const Point source : water_sources) {
        excavate(source, false);
        connected_to_water[cell_id(source.row, source.column)] = 1U;
    }
    for (const Point house : houses) {
        excavate(house, false);
    }

    while (!every_house_has_water()) {
        const vector<int> route = shortest_route_to_a_house();
        if (route.empty()) finish_now();

        int newly_dug = 0;
        for (const int id : route) {
            const Point point = point_of(id);
            if (broken[id] == 0U) {
                excavate(point, true);
                ++newly_dug;
            }
            // The route is traversed from an already wet cell, so every cell
            // processed here is connected to water.
            connected_to_water[id] = 1U;
            if (newly_dug >= REPLAN_INTERVAL) break;
        }
    }
}

void dig_direct_path(Point from, Point to) {
    Point current = from;
    excavate(current, false);
    while (current.row != to.row) {
        current.row += current.row < to.row ? 1 : -1;
        excavate(current, false);
    }
    while (current.column != to.column) {
        current.column += current.column < to.column ? 1 : -1;
        excavate(current, false);
    }
}

void solve_simple_baseline() {
    // Compile with -DSIMPLE_POLICY to reproduce the intentionally simple
    // comparison: connect every house directly to the first water source.
    for (const Point house : houses) {
        dig_direct_path(water_sources.front(), house);
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int input_grid_size = 0;
    cin >> input_grid_size >> water_source_count >> house_count >> fixed_cost;
    if (input_grid_size != GRID_SIZE) return 0;

    water_sources.resize(water_source_count);
    houses.resize(house_count);
    for (Point& point : water_sources) cin >> point.row >> point.column;
    for (Point& point : houses) cin >> point.row >> point.column;

    nearest_observation_distance.fill(UNKNOWN_DISTANCE);
    nearest_observation_hardness.fill(DEFAULT_HARDNESS);
    for (const Point house : houses) {
        house_cell[cell_id(house.row, house.column)] = 1U;
    }

#ifdef SIMPLE_POLICY
    solve_simple_baseline();
#else
    solve_adaptive_network();
#endif
    return 0;
}
