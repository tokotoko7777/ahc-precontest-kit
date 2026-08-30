#include <bits/stdc++.h>
using namespace std;

// Compile with -DAHC034_SNAKE_BASELINE for the simple two-phase route.
// Compile with -DAHC034_SIMPLE_GREEDY for only the nearest supply route.
#ifndef AHC034_ORDER_SEARCH_ITERATIONS
#define AHC034_ORDER_SEARCH_ITERATIONS 300000
#endif

struct Random {
    uint64_t state = 0x123456789abcdef0ULL;

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
        return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0);
    }
};

struct Event {
    int cell;
    int amount;  // positive: load, negative: unload
};

struct Trip {
    int supply;
    int demand;
    int amount;
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int N;
    cin >> N;
    const int cell_count = N * N;
    vector<int> height(cell_count);
    for (int& value : height) cin >> value;

    auto distance = [&](int left, int right) {
        return abs(left / N - right / N) + abs(left % N - right % N);
    };

    vector<int> supply_cells;
    vector<int> demand_cells;
    for (int cell = 0; cell < cell_count; ++cell) {
        if (height[cell] > 0) supply_cells.push_back(cell);
        if (height[cell] < 0) demand_cells.push_back(cell);
    }

    auto evaluate = [&](const vector<Event>& events) {
        long long cost = 0;
        long long load = 0;
        int current = 0;
        vector<int> remaining = height;
        for (const Event& event : events) {
            cost += static_cast<long long>(distance(current, event.cell))
                  * (100 + load);
            current = event.cell;
            cost += abs(event.amount);
            load += event.amount;
            remaining[event.cell] -= event.amount;
            if (load < 0) return numeric_limits<long long>::max();
        }
        if (load != 0) return numeric_limits<long long>::max();
        for (int value : remaining) {
            if (value != 0) return numeric_limits<long long>::max();
        }
        return cost;
    };

    auto make_two_phase = [&]() {
        vector<Event> events;
        vector<int> supply_order = supply_cells;
        vector<int> demand_order = demand_cells;
        auto snake_key = [&](int cell) {
            const int row = cell / N;
            const int column = cell % N;
            return row * N + (row % 2 == 0 ? column : N - 1 - column);
        };
        sort(supply_order.begin(), supply_order.end(), [&](int left, int right) {
            return snake_key(left) < snake_key(right);
        });
        sort(demand_order.begin(), demand_order.end(), [&](int left, int right) {
            return snake_key(left) < snake_key(right);
        });
        for (int cell : supply_order) events.push_back({cell, height[cell]});
        for (int cell : demand_order) events.push_back({cell, height[cell]});
        return events;
    };

    const vector<Event> baseline = make_two_phase();
#ifdef AHC034_SNAKE_BASELINE
    auto output_events = [&](const vector<Event>& events) {
        int current = 0;
        for (const Event& event : events) {
            while (current / N < event.cell / N) {
                cout << "D\n";
                current += N;
            }
            while (current / N > event.cell / N) {
                cout << "U\n";
                current -= N;
            }
            while (current % N < event.cell % N) {
                cout << "R\n";
                ++current;
            }
            while (current % N > event.cell % N) {
                cout << "L\n";
                --current;
            }
            if (event.amount > 0) cout << '+' << event.amount << '\n';
            else cout << event.amount << '\n';
        }
    };
    output_events(baseline);
    return 0;
#endif

    auto make_nearest_supply_route = [&](int supply_mode) {
        vector<int> supply_left = height;
        vector<int> demand_left(cell_count, 0);
        for (int cell : demand_cells) demand_left[cell] = -height[cell];
        vector<Event> events;
        int current = 0;

        int remaining_supply_count = static_cast<int>(supply_cells.size());
        while (remaining_supply_count > 0) {
            int best_supply = -1;
            long long best_value = numeric_limits<long long>::max();
            for (int supply : supply_cells) {
                if (supply_left[supply] <= 0) continue;
                int nearest_demand = 1000000;
                for (int demand : demand_cells) {
                    if (demand_left[demand] > 0) {
                        nearest_demand = min(
                            nearest_demand, distance(supply, demand));
                    }
                }
                const long long value =
                    100LL * distance(current, supply)
                    + static_cast<long long>(supply_mode)
                      * 100LL * nearest_demand;
                if (value < best_value) {
                    best_value = value;
                    best_supply = supply;
                }
            }

            int load = supply_left[best_supply];
            events.push_back({best_supply, load});
            supply_left[best_supply] = 0;
            --remaining_supply_count;
            current = best_supply;

            while (load > 0) {
                int best_demand = -1;
                int best_distance = 1000000;
                for (int demand : demand_cells) {
                    if (demand_left[demand] <= 0) continue;
                    const int move_distance = distance(current, demand);
                    if (move_distance < best_distance) {
                        best_distance = move_distance;
                        best_demand = demand;
                    }
                }
                const int amount = min(load, demand_left[best_demand]);
                events.push_back({best_demand, -amount});
                demand_left[best_demand] -= amount;
                load -= amount;
                current = best_demand;
            }
        }
        return events;
    };

    [[maybe_unused]] auto make_scan_route = [&](const vector<int>& order) {
        vector<int> demand_left(cell_count, 0);
        for (int cell : demand_cells) demand_left[cell] = -height[cell];
        vector<Event> events;
        int load = 0;
        int current = 0;
        for (int cell : order) {
            if (height[cell] > 0) {
                events.push_back({cell, height[cell]});
                load += height[cell];
                current = cell;
            } else if (height[cell] < 0 && load > 0) {
                const int amount = min(load, demand_left[cell]);
                events.push_back({cell, -amount});
                demand_left[cell] -= amount;
                load -= amount;
                current = cell;
            }
        }
        while (load > 0) {
            int best_demand = -1;
            int best_distance = 1000000;
            for (int demand : demand_cells) {
                if (demand_left[demand] <= 0) continue;
                const int move_distance = distance(current, demand);
                if (move_distance < best_distance) {
                    best_distance = move_distance;
                    best_demand = demand;
                }
            }
            const int amount = min(load, demand_left[best_demand]);
            events.push_back({best_demand, -amount});
            demand_left[best_demand] -= amount;
            load -= amount;
            current = best_demand;
        }
        return events;
    };

    [[maybe_unused]] auto make_ordered_scan_route =
        [&](const vector<int>& order) {
        vector<int> demand_left(cell_count, 0);
        for (int cell : demand_cells) demand_left[cell] = -height[cell];
        vector<Event> events;
        int load = 0;
        for (int cell : order) {
            if (height[cell] > 0) {
                events.push_back({cell, height[cell]});
                load += height[cell];
            } else if (height[cell] < 0 && load > 0) {
                const int amount = min(load, demand_left[cell]);
                events.push_back({cell, -amount});
                demand_left[cell] -= amount;
                load -= amount;
            }
        }
        for (int cell : order) {
            if (demand_left[cell] > 0) {
                events.push_back({cell, -demand_left[cell]});
                load -= demand_left[cell];
                demand_left[cell] = 0;
            }
        }
        return events;
    };

    // The local search calls this often.  It computes the same exact cost as
    // evaluate(make_ordered_scan_route(order)), without constructing events.
    [[maybe_unused]] auto evaluate_ordered_scan =
        [&](const vector<int>& order) {
        array<int, 400> demand_left{};
        for (int cell : demand_cells) demand_left[cell] = -height[cell];
        long long cost = 0;
        int load = 0;
        int current = 0;
        for (int cell : order) {
            int amount = 0;
            if (height[cell] > 0) {
                amount = height[cell];
            } else if (height[cell] < 0 && load > 0) {
                amount = -min(load, demand_left[cell]);
                demand_left[cell] += amount;
            }
            if (amount == 0) continue;
            cost += static_cast<long long>(distance(current, cell))
                  * (100 + load);
            cost += abs(amount);
            load += amount;
            current = cell;
        }
        for (int cell : order) {
            if (demand_left[cell] == 0) continue;
            cost += static_cast<long long>(distance(current, cell))
                  * (100 + load);
            cost += demand_left[cell];
            load -= demand_left[cell];
            demand_left[cell] = 0;
            current = cell;
        }
        if (load != 0) return numeric_limits<long long>::max();
        return cost;
    };

    [[maybe_unused]] auto make_snake_order =
        [&](bool transpose, bool reverse_outer, bool reverse_first_inner) {
        vector<int> order;
        order.reserve(cell_count);
        for (int outer_step = 0; outer_step < N; ++outer_step) {
            const int outer = reverse_outer ? N - 1 - outer_step : outer_step;
            const bool reverse_inner =
                reverse_first_inner ^ (outer_step % 2 == 1);
            for (int inner_step = 0; inner_step < N; ++inner_step) {
                const int inner = reverse_inner ? N - 1 - inner_step : inner_step;
                const int row = transpose ? inner : outer;
                const int column = transpose ? outer : inner;
                order.push_back(row * N + column);
            }
        }
        return order;
    };

    [[maybe_unused]] auto make_integrated_trips = [&](bool compare_per_unit) {
        vector<int> supply_left(cell_count, 0);
        vector<int> demand_left(cell_count, 0);
        for (int cell : supply_cells) supply_left[cell] = height[cell];
        for (int cell : demand_cells) demand_left[cell] = -height[cell];
        vector<Event> events;
        int current = 0;
        int remaining_supply_count = static_cast<int>(supply_cells.size());

        while (remaining_supply_count > 0) {
            int best_supply = -1;
            int best_demand = -1;
            int best_amount = 0;
            long double best_value = numeric_limits<long double>::infinity();
            for (int supply : supply_cells) {
                if (supply_left[supply] <= 0) continue;
                for (int demand : demand_cells) {
                    if (demand_left[demand] <= 0) continue;
                    const int amount =
                        min(supply_left[supply], demand_left[demand]);
                    long double value =
                        100.0L * distance(current, supply)
                        + static_cast<long double>(100 + amount)
                          * distance(supply, demand);
                    if (compare_per_unit) value /= amount;
                    if (value < best_value) {
                        best_value = value;
                        best_supply = supply;
                        best_demand = demand;
                        best_amount = amount;
                    }
                }
            }
            events.push_back({best_supply, best_amount});
            events.push_back({best_demand, -best_amount});
            supply_left[best_supply] -= best_amount;
            demand_left[best_demand] -= best_amount;
            if (supply_left[best_supply] == 0) --remaining_supply_count;
            current = best_demand;
        }
        return events;
    };

    [[maybe_unused]] auto make_distance_trips = [&]() {
        vector<int> supply_left(cell_count, 0);
        vector<int> demand_left(cell_count, 0);
        for (int cell : supply_cells) supply_left[cell] = height[cell];
        for (int cell : demand_cells) demand_left[cell] = -height[cell];
        vector<Trip> trips;
        int remaining_supply_count = static_cast<int>(supply_cells.size());
        while (remaining_supply_count > 0) {
            int best_supply = -1;
            int best_demand = -1;
            int best_distance = 1000000;
            int best_amount = -1;
            for (int supply : supply_cells) {
                if (supply_left[supply] <= 0) continue;
                for (int demand : demand_cells) {
                    if (demand_left[demand] <= 0) continue;
                    const int amount =
                        min(supply_left[supply], demand_left[demand]);
                    const int move_distance = distance(supply, demand);
                    if (move_distance < best_distance
                        || (move_distance == best_distance && amount > best_amount)) {
                        best_distance = move_distance;
                        best_amount = amount;
                        best_supply = supply;
                        best_demand = demand;
                    }
                }
            }
            trips.push_back({best_supply, best_demand, best_amount});
            supply_left[best_supply] -= best_amount;
            demand_left[best_demand] -= best_amount;
            if (supply_left[best_supply] == 0) --remaining_supply_count;
        }
        return trips;
    };

    [[maybe_unused]] auto order_trips = [&](const vector<Trip>& trips) {
        const int trip_count = static_cast<int>(trips.size());
        vector<int> order;
        order.reserve(trip_count);
        vector<char> used(trip_count, false);
        int current = 0;
        for (int position = 0; position < trip_count; ++position) {
            int best = -1;
            int best_distance = 1000000;
            for (int index = 0; index < trip_count; ++index) {
                if (used[index]) continue;
                const int move_distance = distance(current, trips[index].supply);
                if (move_distance < best_distance) {
                    best_distance = move_distance;
                    best = index;
                }
            }
            used[best] = true;
            order.push_back(best);
            current = trips[best].demand;
        }

        auto empty_distance = [&](const vector<int>& sequence) {
            int result = 0;
            int previous = 0;
            for (int index : sequence) {
                result += distance(previous, trips[index].supply);
                previous = trips[index].demand;
            }
            return result;
        };

        Random random;
        int current_distance = empty_distance(order);
        vector<int> best_order = order;
        int best_distance = current_distance;
        for (int iteration = 0; iteration < 50000 && trip_count >= 2;
             ++iteration) {
            int left = random.next_int(trip_count);
            int right = random.next_int(trip_count - 1);
            if (right >= left) ++right;
            swap(order[left], order[right]);
            const int new_distance = empty_distance(order);
            if (new_distance <= current_distance) {
                current_distance = new_distance;
                if (new_distance < best_distance) {
                    best_distance = new_distance;
                    best_order = order;
                }
            } else {
                swap(order[left], order[right]);
            }
        }

        vector<Event> events;
        for (int index : best_order) {
            events.push_back({trips[index].supply, trips[index].amount});
            events.push_back({trips[index].demand, -trips[index].amount});
        }
        return events;
    };

    vector<Event> answer = baseline;
    long long best_cost = evaluate(answer);
    auto consider = [&](vector<Event> candidate) {
        const long long cost = evaluate(candidate);
        if (cost < best_cost) {
            best_cost = cost;
            answer = move(candidate);
        }
    };

    consider(make_nearest_supply_route(0));
#ifndef AHC034_SIMPLE_GREEDY
    vector<int> best_scan_order;
    long long best_scan_cost = numeric_limits<long long>::max();
    for (int transpose = 0; transpose < 2; ++transpose) {
        for (int reverse_outer = 0; reverse_outer < 2; ++reverse_outer) {
            for (int reverse_inner = 0; reverse_inner < 2; ++reverse_inner) {
                vector<int> order = make_snake_order(
                    transpose, reverse_outer, reverse_inner);
                vector<int> nonzero_order;
                for (int cell : order) {
                    if (height[cell] != 0) nonzero_order.push_back(cell);
                }
                const int order_size = static_cast<int>(nonzero_order.size());
                for (int shift = 0; shift < max(1, order_size); ++shift) {
                    vector<int> rotated_order;
                    rotated_order.reserve(order_size);
                    for (int index = 0; index < order_size; ++index) {
                        rotated_order.push_back(
                            nonzero_order[(shift + index) % order_size]);
                    }

                    vector<Event> ordered_candidate =
                        make_ordered_scan_route(rotated_order);
                    const long long ordered_cost =
                        evaluate_ordered_scan(rotated_order);
                    if (ordered_cost < best_scan_cost) {
                        best_scan_cost = ordered_cost;
                        best_scan_order = rotated_order;
                    }
                    consider(move(ordered_candidate));
                    consider(make_scan_route(rotated_order));
                }
            }
        }
    }

    // Every permutation remains feasible: demands that appear before enough
    // supply are simply completed during the second pass.
    Random order_random;
    vector<int> current_order = best_scan_order;
    vector<int> best_order = current_order;
    long long current_order_cost = evaluate_ordered_scan(current_order);
    long long best_order_cost = current_order_cost;
    const int order_size = static_cast<int>(current_order.size());
    for (int iteration = 0;
         iteration < AHC034_ORDER_SEARCH_ITERATIONS && order_size >= 2;
         ++iteration) {
        const int first = order_random.next_int(order_size);
        const int lower = max(0, first - 12);
        const int upper = min(order_size - 1, first + 12);
        int second = lower + order_random.next_int(upper - lower);
        if (second >= first) ++second;
        const int left = min(first, second);
        const int right = max(first, second);

        const int move_type = order_random.next_int(3);
        if (move_type == 0) {
            reverse(current_order.begin() + left,
                    current_order.begin() + right + 1);
        } else if (move_type == 1) {
            swap(current_order[first], current_order[second]);
        } else {
            const int cell = current_order[first];
            current_order.erase(current_order.begin() + first);
            current_order.insert(current_order.begin() + second, cell);
        }

        const long long new_cost = evaluate_ordered_scan(current_order);
        const double progress = static_cast<double>(iteration)
            / max(1, AHC034_ORDER_SEARCH_ITERATIONS);
        const double temperature = 10000.0 * pow(10.0 / 10000.0, progress);
        const bool accept = new_cost <= current_order_cost
            || order_random.next_double()
               < exp(static_cast<double>(current_order_cost - new_cost)
                     / temperature);
        if (accept) {
            current_order_cost = new_cost;
            if (new_cost < best_order_cost) {
                best_order_cost = new_cost;
                best_order = current_order;
            }
        } else if (move_type == 0) {
            reverse(current_order.begin() + left,
                    current_order.begin() + right + 1);
        } else if (move_type == 1) {
            swap(current_order[first], current_order[second]);
        } else {
            const int cell = current_order[second];
            current_order.erase(current_order.begin() + second);
            current_order.insert(current_order.begin() + first, cell);
        }
    }
    consider(make_ordered_scan_route(best_order));
    consider(make_scan_route(best_order));

    consider(make_nearest_supply_route(1));
    consider(make_nearest_supply_route(2));
    consider(make_integrated_trips(false));
    consider(make_integrated_trips(true));
    consider(order_trips(make_distance_trips()));
#endif

    int current = 0;
    for (const Event& event : answer) {
        while (current / N < event.cell / N) {
            cout << "D\n";
            current += N;
        }
        while (current / N > event.cell / N) {
            cout << "U\n";
            current -= N;
        }
        while (current % N < event.cell % N) {
            cout << "R\n";
            ++current;
        }
        while (current % N > event.cell % N) {
            cout << "L\n";
            --current;
        }
        if (event.amount > 0) cout << '+' << event.amount << '\n';
        else cout << event.amount << '\n';
    }
}
