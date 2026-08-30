#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

using namespace std;

struct Random {
    uint64_t state;

    explicit Random(uint64_t seed) : state(seed) {}

    uint32_t next_u32() {
        state ^= state << 7;
        state ^= state >> 9;
        return static_cast<uint32_t>(state);
    }

    int next_int(int upper) {
        return static_cast<int>(next_u32() % static_cast<uint32_t>(upper));
    }
};

struct GroupStats {
    long long sum_x = 0;
    long long sum_y = 0;
    long long sum_square = 0;
};

class Solver {
  public:
    void run() {
        read_input();

#ifdef AHC045_BASELINE
        make_baseline_groups();
        vector<vector<pair<int, int>>> edges(group_count_);
        for (int group = 0; group < group_count_; ++group) {
            edges[group] = estimated_mst(groups_[group]);
        }
#else
        make_kd_groups();
        improve_groups();
        vector<vector<pair<int, int>>> edges(group_count_);
        if (should_use_queries()) {
            edges = query_group_trees();
        } else {
            for (int group = 0; group < group_count_; ++group) {
                edges[group] = estimated_mst(groups_[group]);
            }
        }
#endif

        cout << "!" << endl;
        for (int group = 0; group < group_count_; ++group) {
            for (int index = 0; index < group_size_[group]; ++index) {
                if (index != 0) cout << ' ';
                cout << groups_[group][index];
            }
            cout << '\n';
            for (const auto& [left, right] : edges[group]) {
                cout << left << ' ' << right << '\n';
            }
        }
    }

  private:
    int city_count_ = 0;
    int group_count_ = 0;
    int query_limit_ = 0;
    int query_size_limit_ = 0;
    int rectangle_limit_ = 0;
    vector<int> group_size_;
    vector<int> x_;  // Twice the center x-coordinate.
    vector<int> y_;  // Twice the center y-coordinate.
    vector<vector<int>> groups_;

    void read_input() {
        cin >> city_count_ >> group_count_ >> query_limit_ >> query_size_limit_ >>
            rectangle_limit_;
        group_size_.resize(group_count_);
        for (int& size : group_size_) cin >> size;

        x_.resize(city_count_);
        y_.resize(city_count_);
        for (int city = 0; city < city_count_; ++city) {
            int left_x, right_x, left_y, right_y;
            cin >> left_x >> right_x >> left_y >> right_y;
            x_[city] = left_x + right_x;
            y_[city] = left_y + right_y;
        }
        groups_.assign(group_count_, {});
    }

    long long distance_square(int left, int right) const {
        const long long dx = x_[left] - x_[right];
        const long long dy = y_[left] - y_[right];
        return dx * dx + dy * dy;
    }

    void make_baseline_groups() {
        vector<int> cities(city_count_);
        iota(cities.begin(), cities.end(), 0);
        sort(cities.begin(), cities.end(), [&](int left, int right) {
            if (x_[left] != x_[right]) return x_[left] < x_[right];
            if (y_[left] != y_[right]) return y_[left] < y_[right];
            return left < right;
        });

        int position = 0;
        for (int group = 0; group < group_count_; ++group) {
            groups_[group].assign(cities.begin() + position,
                                  cities.begin() + position + group_size_[group]);
            position += group_size_[group];
        }
    }

    void divide_by_kd(vector<int> cities, vector<int> group_ids) {
        if (group_ids.size() == 1U) {
            groups_[group_ids[0]] = std::move(cities);
            return;
        }

        // Put large capacities into the currently smaller side.  This makes
        // the two spatial pieces close to half-and-half without knapsack code.
        sort(group_ids.begin(), group_ids.end(), [&](int left, int right) {
            if (group_size_[left] != group_size_[right]) {
                return group_size_[left] > group_size_[right];
            }
            return left < right;
        });
        vector<int> left_groups;
        vector<int> right_groups;
        int left_size = 0;
        int right_size = 0;
        for (int group : group_ids) {
            if (left_size <= right_size) {
                left_groups.push_back(group);
                left_size += group_size_[group];
            } else {
                right_groups.push_back(group);
                right_size += group_size_[group];
            }
        }

        int minimum_x = numeric_limits<int>::max();
        int maximum_x = numeric_limits<int>::min();
        int minimum_y = numeric_limits<int>::max();
        int maximum_y = numeric_limits<int>::min();
        for (int city : cities) {
            minimum_x = min(minimum_x, x_[city]);
            maximum_x = max(maximum_x, x_[city]);
            minimum_y = min(minimum_y, y_[city]);
            maximum_y = max(maximum_y, y_[city]);
        }
        const bool split_by_x = maximum_x - minimum_x >= maximum_y - minimum_y;
        sort(cities.begin(), cities.end(), [&](int left, int right) {
            const int first_left = split_by_x ? x_[left] : y_[left];
            const int first_right = split_by_x ? x_[right] : y_[right];
            if (first_left != first_right) return first_left < first_right;
            const int second_left = split_by_x ? y_[left] : x_[left];
            const int second_right = split_by_x ? y_[right] : x_[right];
            if (second_left != second_right) return second_left < second_right;
            return left < right;
        });

        vector<int> left_cities(cities.begin(), cities.begin() + left_size);
        vector<int> right_cities(cities.begin() + left_size, cities.end());
        divide_by_kd(std::move(left_cities), std::move(left_groups));
        divide_by_kd(std::move(right_cities), std::move(right_groups));
    }

    void make_kd_groups() {
        vector<int> cities(city_count_);
        iota(cities.begin(), cities.end(), 0);
        vector<int> group_ids(group_count_);
        iota(group_ids.begin(), group_ids.end(), 0);
        divide_by_kd(std::move(cities), std::move(group_ids));
    }

    long double group_cost(const GroupStats& stats, int size) const {
        const long double sum_square = stats.sum_square;
        const long double center_term =
            (static_cast<long double>(stats.sum_x) * stats.sum_x +
             static_cast<long double>(stats.sum_y) * stats.sum_y) /
            size;
        return sum_square - center_term;
    }

    vector<vector<int>> make_nearest_lists(int count) const {
        vector<vector<int>> nearest(city_count_);
        for (int city = 0; city < city_count_; ++city) {
            vector<pair<long long, int>> candidates;
            candidates.reserve(city_count_ - 1);
            for (int other = 0; other < city_count_; ++other) {
                if (city == other) continue;
                candidates.push_back({distance_square(city, other), other});
            }
            const int keep = min(count, static_cast<int>(candidates.size()));
            partial_sort(candidates.begin(), candidates.begin() + keep,
                         candidates.end());
            nearest[city].reserve(keep);
            for (int i = 0; i < keep; ++i) {
                nearest[city].push_back(candidates[i].second);
            }
        }
        return nearest;
    }

    void improve_groups() {
        if (group_count_ == 1) return;

        vector<int> group_of(city_count_);
        vector<int> position_in_group(city_count_);
        vector<GroupStats> stats(group_count_);
        for (int group = 0; group < group_count_; ++group) {
            for (int position = 0;
                 position < static_cast<int>(groups_[group].size()); ++position) {
                const int city = groups_[group][position];
                group_of[city] = group;
                position_in_group[city] = position;
                stats[group].sum_x += x_[city];
                stats[group].sum_y += y_[city];
                stats[group].sum_square +=
                    static_cast<long long>(x_[city]) * x_[city] +
                    static_cast<long long>(y_[city]) * y_[city];
            }
        }

        const vector<vector<int>> nearest = make_nearest_lists(24);
        Random random(0x243f6a8885a308d3ULL);
#ifdef LOCAL_SHORT_TIME
        constexpr int ITERATIONS = 20000;
#else
        constexpr int ITERATIONS = 500000;
#endif

        for (int iteration = 0; iteration < ITERATIONS; ++iteration) {
            const int left_city = random.next_int(city_count_);
            int right_city;
            if (iteration % 10 != 0) {
                const vector<int>& choices = nearest[left_city];
                right_city = choices[random.next_int(static_cast<int>(choices.size()))];
            } else {
                right_city = random.next_int(city_count_);
            }

            const int left_group = group_of[left_city];
            const int right_group = group_of[right_city];
            if (left_group == right_group) continue;

            const long double before =
                group_cost(stats[left_group], group_size_[left_group]) +
                group_cost(stats[right_group], group_size_[right_group]);
            GroupStats new_left = stats[left_group];
            GroupStats new_right = stats[right_group];
            new_left.sum_x += x_[right_city] - x_[left_city];
            new_left.sum_y += y_[right_city] - y_[left_city];
            new_right.sum_x += x_[left_city] - x_[right_city];
            new_right.sum_y += y_[left_city] - y_[right_city];
            const long long left_square =
                static_cast<long long>(x_[left_city]) * x_[left_city] +
                static_cast<long long>(y_[left_city]) * y_[left_city];
            const long long right_square =
                static_cast<long long>(x_[right_city]) * x_[right_city] +
                static_cast<long long>(y_[right_city]) * y_[right_city];
            new_left.sum_square += right_square - left_square;
            new_right.sum_square += left_square - right_square;
            const long double after =
                group_cost(new_left, group_size_[left_group]) +
                group_cost(new_right, group_size_[right_group]);
            if (after >= before) continue;

            const int left_position = position_in_group[left_city];
            const int right_position = position_in_group[right_city];
            swap(groups_[left_group][left_position],
                 groups_[right_group][right_position]);
            group_of[left_city] = right_group;
            group_of[right_city] = left_group;
            position_in_group[left_city] = right_position;
            position_in_group[right_city] = left_position;
            stats[left_group] = new_left;
            stats[right_group] = new_right;
        }
    }

    vector<pair<int, int>> estimated_mst(const vector<int>& cities) const {
        const int size = static_cast<int>(cities.size());
        if (size <= 1) return {};

        vector<long long> best_distance(size,
                                        numeric_limits<long long>::max());
        vector<int> best_parent(size, -1);
        vector<bool> used(size, false);
        best_distance[0] = 0;
        vector<pair<int, int>> edges;
        edges.reserve(size - 1);

        for (int step = 0; step < size; ++step) {
            int vertex = -1;
            for (int index = 0; index < size; ++index) {
                if (!used[index] &&
                    (vertex == -1 || best_distance[index] < best_distance[vertex])) {
                    vertex = index;
                }
            }
            used[vertex] = true;
            if (best_parent[vertex] != -1) {
                edges.push_back(
                    {cities[vertex], cities[best_parent[vertex]]});
            }
            for (int next = 0; next < size; ++next) {
                if (used[next]) continue;
                const long long distance =
                    distance_square(cities[vertex], cities[next]);
                if (distance < best_distance[next]) {
                    best_distance[next] = distance;
                    best_parent[next] = vertex;
                }
            }
        }
        return edges;
    }

    bool should_use_queries() const {
        int small_groups = 0;
        for (int size : group_size_) {
            if (size <= query_size_limit_) ++small_groups;
        }

        // With accurate rectangle centers and mostly large groups, dividing a
        // group into many independent query batches can cost more than the
        // information gained.  In that case, keep the global center-based MST.
        const bool centers_are_reliable = rectangle_limit_ <= 820;
        const bool moderate_group_count =
            2 <= group_count_ && group_count_ <= 35;
        const bool mostly_large_groups =
            small_groups * 100 <= group_count_ * 35;
        return !(centers_are_reliable && moderate_group_count &&
                 mostly_large_groups);
    }

    vector<pair<int, int>> ask_mst(const vector<int>& cities) const {
        cout << "? " << cities.size();
        for (int city : cities) cout << ' ' << city;
        cout << endl;

        vector<pair<int, int>> edges(cities.size() - 1);
        for (auto& [left, right] : edges) cin >> left >> right;
        return edges;
    }

    vector<pair<int, int>> query_one_group(const vector<int>& cities,
                                           int& query_count) const {
        const int size = static_cast<int>(cities.size());
        if (size <= 1) return {};

        // Start from the city nearest to the estimated group center.
        long long sum_x = 0;
        long long sum_y = 0;
        for (int city : cities) {
            sum_x += x_[city];
            sum_y += y_[city];
        }
        int start = cities[0];
        long long best_center_distance = numeric_limits<long long>::max();
        for (int city : cities) {
            const long long dx = static_cast<long long>(x_[city]) * size - sum_x;
            const long long dy = static_cast<long long>(y_[city]) * size - sum_y;
            const long long distance = dx * dx + dy * dy;
            if (distance < best_center_distance) {
                best_center_distance = distance;
                start = city;
            }
        }

        vector<bool> remaining(city_count_, false);
        vector<int> connected_list = {start};
        for (int city : cities) {
            if (city != start) remaining[city] = true;
        }
        int remaining_count = size - 1;
        vector<pair<int, int>> result;
        result.reserve(size - 1);

        while (remaining_count > 0) {
            int anchor = -1;
            int seed = -1;
            long long best = numeric_limits<long long>::max();
            for (int old_city : connected_list) {
                for (int new_city : cities) {
                    if (!remaining[new_city]) continue;
                    const long long distance = distance_square(old_city, new_city);
                    if (distance < best) {
                        best = distance;
                        anchor = old_city;
                        seed = new_city;
                    }
                }
            }

            vector<pair<long long, int>> near_seed;
            near_seed.reserve(remaining_count);
            for (int city : cities) {
                if (remaining[city]) {
                    near_seed.push_back({distance_square(seed, city), city});
                }
            }
            sort(near_seed.begin(), near_seed.end());

            vector<int> query = {anchor};
            const int add_count =
                min(query_size_limit_ - 1, remaining_count);
            for (int index = 0; index < add_count; ++index) {
                query.push_back(near_seed[index].second);
            }

            vector<pair<int, int>> new_edges = ask_mst(query);
            ++query_count;
            result.insert(result.end(), new_edges.begin(), new_edges.end());
            for (int index = 1; index < static_cast<int>(query.size()); ++index) {
                const int city = query[index];
                remaining[city] = false;
                connected_list.push_back(city);
                --remaining_count;
            }
        }
        return result;
    }

    vector<vector<pair<int, int>>> query_group_trees() const {
        vector<vector<pair<int, int>>> edges(group_count_);
        int query_count = 0;
        for (int group = 0; group < group_count_; ++group) {
            edges[group] = query_one_group(groups_[group], query_count);
        }
        // For L >= 3, each query introduces at least two new cities except
        // possibly the final query of a group.  Thus at most sum floor(G/2)
        // queries are needed, which is at most 400 for N = 800.
        if (query_count > query_limit_) {
            // This branch is unreachable for official constraints.  Keeping a
            // visible check makes accidental changes fail locally.
            std::abort();
        }
        return edges;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Solver solver;
    solver.run();
    return 0;
}
