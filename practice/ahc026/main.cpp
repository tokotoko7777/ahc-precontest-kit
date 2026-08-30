#include <bits/stdc++.h>
using namespace std;

struct Plan {
    vector<pair<int, int>> operations;
    int energy = 0;
};

vector<int> make_location_table(
    const vector<vector<int>>& stacks,
    int n
) {
    vector<int> location(n + 1, -1);
    for (int stack_id = 0;
         stack_id < static_cast<int>(stacks.size());
         ++stack_id) {
        for (int box : stacks[stack_id]) {
            location[box] = stack_id;
        }
    }
    return location;
}

int minimum_box(const vector<int>& stack, int n) {
    if (stack.empty()) return n + 1;
    return *min_element(stack.begin(), stack.end());
}

int choose_bulk_destination(
    const vector<vector<int>>& stacks,
    int from,
    int n
) {
    // A moved block should not cover boxes needed soon.  Therefore choose the
    // stack whose smallest remaining label is as large as possible.
    int best_stack = -1;
    int best_minimum = -1;
    for (int to = 0; to < static_cast<int>(stacks.size()); ++to) {
        if (to == from) continue;
        const int value = minimum_box(stacks[to], n);
        if (value > best_minimum) {
            best_minimum = value;
            best_stack = to;
        }
    }
    return best_stack;
}

int choose_small_box_destination(
    const vector<vector<int>>& stacks,
    int from,
    int box,
    int n
) {
    // If every box already in the destination is at least `box`, the moved
    // box can stay on top until its turn.  Use the tightest such stack.
    int best_stack = -1;
    int best_minimum = n + 2;
    for (int to = 0; to < static_cast<int>(stacks.size()); ++to) {
        if (to == from) continue;
        const int value = minimum_box(stacks[to], n);
        if (value >= box && value < best_minimum) {
            best_minimum = value;
            best_stack = to;
        }
    }
    if (best_stack != -1) return best_stack;

    // A perfect place is not always available.  Fall back to the place that
    // postpones the next conflict for as long as possible.
    return choose_bulk_destination(stacks, from, n);
}

void move_suffix(
    vector<vector<int>>& stacks,
    vector<int>& location,
    int from,
    int first_height,
    int to,
    Plan& plan
) {
    const int moved_count =
        static_cast<int>(stacks[from].size()) - first_height;
    plan.energy += moved_count + 1;
    plan.operations.push_back({stacks[from][first_height], to + 1});
    for (int height = first_height;
         height < static_cast<int>(stacks[from].size());
         ++height) {
        location[stacks[from][height]] = to;
    }
    stacks[to].insert(
        stacks[to].end(),
        stacks[from].begin() + first_height,
        stacks[from].end()
    );
    stacks[from].erase(
        stacks[from].begin() + first_height,
        stacks[from].end()
    );
}

void remove_one_target(
    vector<vector<int>>& stacks,
    vector<int>& location,
    int target,
    int n,
    int special_range,
    Plan& plan
) {
    while (true) {
        const int from = location[target];
        assert(from != -1);
        const auto iterator = find(
            stacks[from].begin(),
            stacks[from].end(),
            target
        );
        assert(iterator != stacks[from].end());
        const int target_height = static_cast<int>(
            iterator - stacks[from].begin()
        );
        const int top_height =
            static_cast<int>(stacks[from].size()) - 1;
        if (target_height == top_height) {
            plan.operations.push_back({target, 0});
            stacks[from].pop_back();
            location[target] = -1;
            return;
        }

        // Search from the top.  A soon-needed box is moved alone to a safe
        // stack instead of being buried inside the large block.
        int special_height = -1;
        for (int height = top_height;
             height > target_height;
             --height) {
            if (stacks[from][height] <= target + special_range) {
                special_height = height;
                break;
            }
        }

        if (special_height == -1) {
            const int to = choose_bulk_destination(stacks, from, n);
            move_suffix(
                stacks,
                location,
                from,
                target_height + 1,
                to,
                plan
            );
            continue;
        }

        if (special_height < top_height) {
            const int to = choose_bulk_destination(stacks, from, n);
            move_suffix(
                stacks,
                location,
                from,
                special_height + 1,
                to,
                plan
            );
        }

        const int box = stacks[from].back();
        assert(box <= target + special_range);
        const int to = choose_small_box_destination(
            stacks,
            from,
            box,
            n
        );
        move_suffix(
            stacks,
            location,
            from,
            static_cast<int>(stacks[from].size()) - 1,
            to,
            plan
        );
    }
}

Plan make_plan(
    vector<vector<int>> stacks,
    int first_target,
    int n,
    int special_range
) {
    Plan plan;
    vector<int> location = make_location_table(stacks, n);
    for (int target = first_target; target <= n; ++target) {
        remove_one_target(
            stacks,
            location,
            target,
            n,
            special_range,
            plan
        );
    }
    return plan;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, m;
    cin >> n >> m;
    vector<vector<int>> initial_stacks(m, vector<int>(n / m));
    for (auto& stack : initial_stacks) {
        for (int& box : stack) cin >> box;
    }

#ifdef BASELINE_POLICY
    const Plan best_plan = make_plan(initial_stacks, 1, n, 0);
#elif defined(FIXED_POLICY)
    // Comparison version: choose one look-ahead width for the whole run.
    Plan best_plan;
    best_plan.energy = numeric_limits<int>::max();
    for (int special_range = 0; special_range <= n; ++special_range) {
        Plan candidate = make_plan(
            initial_stacks,
            1,
            n,
            special_range
        );
        if (candidate.energy < best_plan.energy) {
            best_plan = move(candidate);
        }
    }
#else
    // The exact energy of a completed plan is known locally.  At every target,
    // try all look-ahead widths to the end, then use the best width for this
    // target.  Repeating the comparison adapts the policy as stacks change.
    vector<vector<int>> stacks = initial_stacks;
    vector<int> location = make_location_table(stacks, n);
    Plan best_plan;
    for (int target = 1; target <= n; ++target) {
        int best_range = 0;
        int best_future_energy = numeric_limits<int>::max();
        for (int special_range = 0;
             special_range <= n - target;
             ++special_range) {
            const Plan candidate = make_plan(
                stacks,
                target,
                n,
                special_range
            );
            if (candidate.energy < best_future_energy) {
                best_future_energy = candidate.energy;
                best_range = special_range;
            }
        }
        remove_one_target(
            stacks,
            location,
            target,
            n,
            best_range,
            best_plan
        );
    }
#ifdef LOCAL
    cerr << "energy = " << best_plan.energy << '\n';
#endif
#endif

    for (const auto& [box, destination] : best_plan.operations) {
        cout << box << ' ' << destination << '\n';
    }
    return 0;
}
