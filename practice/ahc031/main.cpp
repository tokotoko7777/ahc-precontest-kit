#include <bits/stdc++.h>
using namespace std;

struct Rectangle {
    int top;
    int left;
    int bottom;
    int right;
};

struct BandOption {
    int height = 0;
    long long wall_cost = 0;
    bool fixed_width = false;
    vector<int> items;
};

constexpr long long INF = (1LL << 60);

int ceil_division(int value, int divisor) {
    return (value + divisor - 1) / divisor;
}

vector<int> make_widths(
    const vector<vector<int>>& required_area,
    const vector<int>& items,
    int height,
    int day,
    bool fixed_width,
    int hall_width
) {
    vector<int> width(items.size());
    int used = 0;
    for (int index = 0; index < static_cast<int>(items.size()); ++index) {
        const int item = items[index];
        int area = required_area[day][item];
        if (fixed_width) {
            for (const auto& one_day : required_area) {
                area = max(area, one_day[item]);
            }
        }
        width[index] = ceil_division(area, height);
        used += width[index];
    }
    if (used > hall_width) return {};
    width.back() += hall_width - used;
    return width;
}

long long boundary_change_cost(
    const vector<vector<int>>& required_area,
    const vector<int>& items,
    int height,
    bool fixed_width,
    int hall_width
) {
    if (fixed_width || items.size() <= 1) return 0;
    const int day_count = static_cast<int>(required_area.size());
    long long result = 0;
    vector<int> previous;
    for (int day = 0; day < day_count; ++day) {
        const vector<int> width = make_widths(
            required_area, items, height, day, false, hall_width);
        if (width.empty()) return INF;
        vector<int> boundary;
        int position = 0;
        for (int index = 0; index + 1 < static_cast<int>(width.size()); ++index) {
            position += width[index];
            boundary.push_back(position);
        }
        if (day > 0) {
            int left = 0;
            int right = 0;
            int different = 0;
            while (left < static_cast<int>(previous.size())
                   || right < static_cast<int>(boundary.size())) {
                if (right == static_cast<int>(boundary.size())
                    || (left < static_cast<int>(previous.size())
                        && previous[left] < boundary[right])) {
                    ++left;
                    ++different;
                } else if (left == static_cast<int>(previous.size())
                           || boundary[right] < previous[left]) {
                    ++right;
                    ++different;
                } else {
                    ++left;
                    ++right;
                }
            }
            result += 1LL * different * height;
        }
        previous = move(boundary);
    }
    return result;
}

int minimum_band_height(
    const vector<vector<int>>& required_area,
    const vector<int>& items,
    bool fixed_width,
    int hall_width
) {
    auto fits = [&](int height) {
        if (fixed_width) {
            int total_width = 0;
            for (int item : items) {
                int maximum_area = 0;
                for (const auto& one_day : required_area) {
                    maximum_area = max(maximum_area, one_day[item]);
                }
                total_width += ceil_division(maximum_area, height);
                if (total_width > hall_width) return false;
            }
            return true;
        }
        for (const auto& one_day : required_area) {
            int total_width = 0;
            for (int item : items) {
                total_width += ceil_division(one_day[item], height);
                if (total_width > hall_width) return false;
            }
        }
        return true;
    };

    if (!fits(hall_width)) return hall_width + 1;
    int low = 0;
    int high = hall_width;
    while (high - low > 1) {
        const int middle = (low + high) / 2;
        if (fits(middle)) {
            high = middle;
        } else {
            low = middle;
        }
    }
    return high;
}

int minimum_day_band_height(
    const vector<int>& one_day,
    const vector<int>& items,
    int hall_width
) {
    auto fits = [&](int height) {
        int used_width = 0;
        for (int item : items) {
            used_width += ceil_division(one_day[item], height);
            if (used_width > hall_width) return false;
        }
        return true;
    };
    if (!fits(hall_width)) return hall_width + 1;
    int low = 0;
    int high = hall_width;
    while (high - low > 1) {
        const int middle = (low + high) / 2;
        if (fits(middle)) {
            high = middle;
        } else {
            low = middle;
        }
    }
    return high;
}

vector<vector<Rectangle>> make_strip_baseline(
    const vector<vector<int>>& required_area,
    int hall_width
) {
    const int day_count = static_cast<int>(required_area.size());
    const int item_count = static_cast<int>(required_area[0].size());
    vector<vector<Rectangle>> answer(
        day_count, vector<Rectangle>(item_count));

    for (int day = 0; day < day_count; ++day) {
        vector<int> height(item_count);
        int total_height = 0;
        for (int item = 0; item < item_count; ++item) {
            height[item] = ceil_division(required_area[day][item], hall_width);
            total_height += height[item];
        }

        if (total_height > hall_width) {
            struct Removal {
                int lost_area;
                int item;
                bool operator>(const Removal& other) const {
                    if (lost_area != other.lost_area) {
                        return lost_area > other.lost_area;
                    }
                    return item > other.item;
                }
            };
            priority_queue<Removal, vector<Removal>, greater<Removal>> removable;
            for (int item = 0; item < item_count; ++item) {
                if (height[item] <= 1) continue;
                const int area_after = hall_width * (height[item] - 1);
                removable.push({required_area[day][item] - area_after, item});
            }
            while (total_height > hall_width) {
                const Removal removal = removable.top();
                removable.pop();
                --height[removal.item];
                --total_height;
                if (height[removal.item] > 1) {
                    const int area_after = hall_width * (height[removal.item] - 1);
                    const int current_shortage = max(
                        0,
                        required_area[day][removal.item]
                            - hall_width * height[removal.item]);
                    const int next_shortage = max(
                        0, required_area[day][removal.item] - area_after);
                    removable.push(
                        {next_shortage - current_shortage, removal.item});
                }
            }
        } else {
            height.back() += hall_width - total_height;
        }

        int top = 0;
        for (int item = 0; item < item_count; ++item) {
            answer[day][item] = {top, 0, top + height[item], hall_width};
            top += height[item];
        }
    }
    return answer;
}

vector<vector<Rectangle>> make_day_shelf_answer(
    const vector<vector<int>>& required_area,
    const vector<int>& global_order,
    int hall_width
) {
    const int day_count = static_cast<int>(required_area.size());
    const int item_count = static_cast<int>(global_order.size());
    vector<vector<Rectangle>> answer(
        day_count, vector<Rectangle>(item_count));

    for (int day = 0; day < day_count; ++day) {
        vector<vector<int>> minimum_height(
            item_count, vector<int>(item_count + 1, hall_width + 1));
        for (int begin = 0; begin < item_count; ++begin) {
            vector<int> items;
            for (int end = begin + 1; end <= item_count; ++end) {
                items.push_back(global_order[end - 1]);
                minimum_height[begin][end] = minimum_day_band_height(
                    required_area[day], items, hall_width);
            }
        }

        vector<int> best_total(item_count + 1, hall_width + 1);
        vector<int> previous(item_count + 1, -1);
        best_total[0] = 0;
        for (int end = 1; end <= item_count; ++end) {
            for (int begin = 0; begin < end; ++begin) {
                if (best_total[begin] > hall_width) continue;
                const int candidate =
                    best_total[begin] + minimum_height[begin][end];
                if (candidate < best_total[end]) {
                    best_total[end] = candidate;
                    previous[end] = begin;
                }
            }
        }
        if (best_total[item_count] > hall_width) return {};

        vector<pair<int, int>> bands;
        int position = item_count;
        while (position > 0) {
            bands.push_back({previous[position], position});
            position = previous[position];
        }
        reverse(bands.begin(), bands.end());

        int top = 0;
        for (const auto& [begin, end] : bands) {
            const int height = minimum_height[begin][end];
            vector<int> items;
            for (int index = begin; index < end; ++index) {
                items.push_back(global_order[index]);
            }
            vector<int> width = make_widths(
                required_area, items, height, day, false, hall_width);
            int left = 0;
            for (int index = 0; index < static_cast<int>(items.size()); ++index) {
                const int item = items[index];
                answer[day][item] = {
                    top,
                    left,
                    top + height,
                    left + width[index],
                };
                left += width[index];
            }
            top += height;
        }
    }
    return answer;
}

vector<pair<int, int>> extract_band_partition(
    const vector<Rectangle>& one_day_answer,
    const vector<int>& global_order
) {
    vector<pair<int, int>> bands;
    int begin = 0;
    while (begin < static_cast<int>(global_order.size())) {
        const Rectangle first = one_day_answer[global_order[begin]];
        int end = begin + 1;
        while (end < static_cast<int>(global_order.size())) {
            const Rectangle next = one_day_answer[global_order[end]];
            if (next.top != first.top || next.bottom != first.bottom) break;
            ++end;
        }
        bands.push_back({begin, end});
        begin = end;
    }
    return bands;
}

vector<vector<Rectangle>> make_variable_band_answer(
    const vector<vector<int>>& required_area,
    const vector<int>& global_order,
    const vector<pair<int, int>>& bands,
    int hall_width
) {
    const int day_count = static_cast<int>(required_area.size());
    const int item_count = static_cast<int>(global_order.size());
    vector<vector<Rectangle>> answer(
        day_count, vector<Rectangle>(item_count));

    for (int day = 0; day < day_count; ++day) {
        vector<int> band_height;
        int total_height = 0;
        for (const auto& [begin, end] : bands) {
            vector<int> items;
            for (int index = begin; index < end; ++index) {
                items.push_back(global_order[index]);
            }
            const int height = minimum_day_band_height(
                required_area[day], items, hall_width);
            if (height > hall_width) return {};
            band_height.push_back(height);
            total_height += height;
        }
        if (total_height > hall_width) return {};

        int top = 0;
        for (int band_index = 0;
             band_index < static_cast<int>(bands.size()); ++band_index) {
            const auto [begin, end] = bands[band_index];
            vector<int> items;
            for (int index = begin; index < end; ++index) {
                items.push_back(global_order[index]);
            }
            const int height = band_height[band_index];
            const vector<int> width = make_widths(
                required_area, items, height, day, false, hall_width);
            int left = 0;
            for (int index = 0; index < static_cast<int>(items.size()); ++index) {
                const int item = items[index];
                answer[day][item] = {
                    top,
                    left,
                    top + height,
                    left + width[index],
                };
                left += width[index];
            }
            top += height;
        }
    }
    return answer;
}

vector<vector<Rectangle>> make_guillotine_answer(
    const vector<vector<int>>& required_area,
    const vector<int>& global_order,
    int hall_width
) {
    const int day_count = static_cast<int>(required_area.size());
    const int item_count = static_cast<int>(global_order.size());
    vector<vector<Rectangle>> answer(
        day_count, vector<Rectangle>(item_count));

    struct SplitCandidate {
        long double score;
        int middle;
        int cut;
        bool horizontal;
    };

    for (int day = 0; day < day_count; ++day) {
        vector<long long> prefix(item_count + 1, 0);
        for (int index = 0; index < item_count; ++index) {
            prefix[index + 1] = prefix[index]
                              + required_area[day][global_order[index]];
        }
        unordered_set<uint64_t> failed;

        function<bool(int, int, int, int, int, int)> pack =
            [&](int begin, int end, int top, int left,
                int bottom, int right) -> bool {
                const int height = bottom - top;
                const int width = right - left;
                const long long required = prefix[end] - prefix[begin];
                if (1LL * height * width < required) return false;
                if (end - begin == 1) {
                    answer[day][global_order[begin]] =
                        {top, left, bottom, right};
                    return true;
                }

                const uint64_t key = static_cast<uint64_t>(begin)
                    | (static_cast<uint64_t>(end) << 6)
                    | (static_cast<uint64_t>(height) << 12)
                    | (static_cast<uint64_t>(width) << 22);
                if (failed.count(key)) return false;

                vector<SplitCandidate> candidates;
                for (int middle = begin + 1; middle < end; ++middle) {
                    const long long first_required =
                        prefix[middle] - prefix[begin];
                    const long long second_required =
                        prefix[end] - prefix[middle];

                    const int minimum_first_width =
                        ceil_division(static_cast<int>(first_required), height);
                    const int minimum_second_width =
                        ceil_division(static_cast<int>(second_required), height);
                    if (minimum_first_width + minimum_second_width <= width) {
                        const int proportional = static_cast<int>(
                            (1.0L * width * first_required / required) + 0.5L);
                        const int cut_width = clamp(
                            proportional,
                            minimum_first_width,
                            width - minimum_second_width);
                        const long double first_aspect = max(
                            1.0L * cut_width / height,
                            1.0L * height / cut_width);
                        const long double second_aspect = max(
                            1.0L * (width - cut_width) / height,
                            1.0L * height / (width - cut_width));
                        const long double imbalance = abs(
                            1.0L * cut_width / width
                            - 1.0L * first_required / required);
                        candidates.push_back({
                            imbalance + 0.002L * (first_aspect + second_aspect),
                            middle,
                            left + cut_width,
                            false,
                        });
                    }

                    const int minimum_first_height =
                        ceil_division(static_cast<int>(first_required), width);
                    const int minimum_second_height =
                        ceil_division(static_cast<int>(second_required), width);
                    if (minimum_first_height + minimum_second_height <= height) {
                        const int proportional = static_cast<int>(
                            (1.0L * height * first_required / required) + 0.5L);
                        const int cut_height = clamp(
                            proportional,
                            minimum_first_height,
                            height - minimum_second_height);
                        const long double first_aspect = max(
                            1.0L * width / cut_height,
                            1.0L * cut_height / width);
                        const long double second_aspect = max(
                            1.0L * width / (height - cut_height),
                            1.0L * (height - cut_height) / width);
                        const long double imbalance = abs(
                            1.0L * cut_height / height
                            - 1.0L * first_required / required);
                        candidates.push_back({
                            imbalance + 0.002L * (first_aspect + second_aspect),
                            middle,
                            top + cut_height,
                            true,
                        });
                    }
                }
                sort(candidates.begin(), candidates.end(),
                     [](const SplitCandidate& first,
                        const SplitCandidate& second) {
                         return first.score < second.score;
                     });

                for (const SplitCandidate& candidate : candidates) {
                    bool first_ok = false;
                    bool second_ok = false;
                    if (candidate.horizontal) {
                        first_ok = pack(
                            begin, candidate.middle,
                            top, left, candidate.cut, right);
                        if (first_ok) {
                            second_ok = pack(
                                candidate.middle, end,
                                candidate.cut, left, bottom, right);
                        }
                    } else {
                        first_ok = pack(
                            begin, candidate.middle,
                            top, left, bottom, candidate.cut);
                        if (first_ok) {
                            second_ok = pack(
                                candidate.middle, end,
                                top, candidate.cut, bottom, right);
                        }
                    }
                    if (first_ok && second_ok) return true;
                }
                failed.insert(key);
                return false;
            };

        if (!pack(0, item_count, 0, 0, hall_width, hall_width)) return {};
    }
    return answer;
}

long long calculate_cost(
    const vector<vector<Rectangle>>& answer,
    const vector<vector<int>>& required_area,
    int hall_width
) {
    const int day_count = static_cast<int>(answer.size());
    const int item_count = static_cast<int>(answer[0].size());
    using Interval = pair<int, int>;
    using Lines = vector<vector<Interval>>;

    auto normalize = [](Lines& lines) {
        for (auto& intervals : lines) {
            if (intervals.empty()) continue;
            sort(intervals.begin(), intervals.end());
            vector<Interval> merged;
            for (const Interval& interval : intervals) {
                if (merged.empty() || merged.back().second < interval.first) {
                    merged.push_back(interval);
                } else {
                    merged.back().second = max(
                        merged.back().second, interval.second);
                }
            }
            intervals = move(merged);
        }
    };

    auto symmetric_difference_length = [](const Lines& first,
                                           const Lines& second) {
        long long result = 0;
        for (int line = 1; line + 1 < static_cast<int>(first.size()); ++line) {
            for (const Interval& interval : first[line]) {
                result += interval.second - interval.first;
            }
            for (const Interval& interval : second[line]) {
                result += interval.second - interval.first;
            }
            int left = 0;
            int right = 0;
            while (left < static_cast<int>(first[line].size())
                   && right < static_cast<int>(second[line].size())) {
                const Interval a = first[line][left];
                const Interval b = second[line][right];
                const int overlap = min(a.second, b.second)
                                  - max(a.first, b.first);
                if (overlap > 0) result -= 2LL * overlap;
                if (a.second < b.second) {
                    ++left;
                } else {
                    ++right;
                }
            }
        }
        return result;
    };

    Lines previous_horizontal(hall_width + 1);
    Lines previous_vertical(hall_width + 1);
    long long cost = 0;

    for (int day = 0; day < day_count; ++day) {
        Lines current_horizontal(hall_width + 1);
        Lines current_vertical(hall_width + 1);
        for (int item = 0; item < item_count; ++item) {
            const Rectangle rectangle = answer[day][item];
            const int area = (rectangle.bottom - rectangle.top)
                           * (rectangle.right - rectangle.left);
            if (area < required_area[day][item]) {
                cost += 100LL * (required_area[day][item] - area);
            }
            if (rectangle.top > 0) {
                current_horizontal[rectangle.top].push_back(
                    {rectangle.left, rectangle.right});
            }
            if (rectangle.bottom < hall_width) {
                current_horizontal[rectangle.bottom].push_back(
                    {rectangle.left, rectangle.right});
            }
            if (rectangle.left > 0) {
                current_vertical[rectangle.left].push_back(
                    {rectangle.top, rectangle.bottom});
            }
            if (rectangle.right < hall_width) {
                current_vertical[rectangle.right].push_back(
                    {rectangle.top, rectangle.bottom});
            }
        }
        normalize(current_horizontal);
        normalize(current_vertical);
        if (day > 0) {
            cost += symmetric_difference_length(
                previous_horizontal, current_horizontal);
            cost += symmetric_difference_length(
                previous_vertical, current_vertical);
        }
        previous_horizontal = move(current_horizontal);
        previous_vertical = move(current_vertical);
    }
    return cost;
}

vector<vector<int>> make_global_orders(
    const vector<vector<int>>& required_area
) {
    const int item_count = static_cast<int>(required_area[0].size());
    const int day_count = static_cast<int>(required_area.size());
    vector<vector<int>> orders;

    vector<int> natural(item_count);
    iota(natural.begin(), natural.end(), 0);
    orders.push_back(natural);

    vector<int> alternating;
    alternating.reserve(item_count);
    int left = 0;
    int right = item_count - 1;
    while (left <= right) {
        alternating.push_back(left++);
        if (left <= right) alternating.push_back(right--);
    }
    orders.push_back(alternating);

    vector<int> by_peak_day = natural;
    sort(by_peak_day.begin(), by_peak_day.end(), [&](int first, int second) {
        int first_day = 0;
        int second_day = 0;
        for (int day = 1; day < day_count; ++day) {
            if (required_area[day][first] > required_area[first_day][first]) {
                first_day = day;
            }
            if (required_area[day][second] > required_area[second_day][second]) {
                second_day = day;
            }
        }
        if (first_day != second_day) return first_day < second_day;
        return first < second;
    });
    orders.push_back(by_peak_day);

    return orders;
}

vector<vector<Rectangle>> make_common_band_answer(
    const vector<vector<int>>& required_area,
    const vector<int>& global_order,
    int hall_width
) {
    const int item_count = static_cast<int>(global_order.size());
    const int day_count = static_cast<int>(required_area.size());
    vector<int> area_range(item_count, 0);
    for (int item = 0; item < item_count; ++item) {
        int minimum_area = required_area[0][item];
        int maximum_area = required_area[0][item];
        for (int day = 1; day < day_count; ++day) {
            minimum_area = min(minimum_area, required_area[day][item]);
            maximum_area = max(maximum_area, required_area[day][item]);
        }
        area_range[item] = maximum_area - minimum_area;
    }
    vector<vector<vector<BandOption>>> options(
        item_count, vector<vector<BandOption>>(item_count + 1));

    for (int begin = 0; begin < item_count; ++begin) {
        vector<int> items;
        for (int end = begin + 1; end <= item_count; ++end) {
            items.push_back(global_order[end - 1]);
            const int dynamic_height = minimum_band_height(
                required_area, items, false, hall_width);
            const int fixed_height = minimum_band_height(
                required_area, items, true, hall_width);
            if (dynamic_height > hall_width) continue;

            set<int> height_candidates;
            height_candidates.insert(dynamic_height);
            const int dynamic_limit = min(hall_width, fixed_height - 1);
            for (int addition : {1, 2, 4, 8, 16, 32, 64}) {
                if (dynamic_height + addition <= dynamic_limit) {
                    height_candidates.insert(dynamic_height + addition);
                }
            }
            for (int numerator = 1; numerator <= 3; ++numerator) {
                const int candidate = dynamic_height
                    + (dynamic_limit - dynamic_height) * numerator / 4;
                if (candidate >= dynamic_height && candidate <= dynamic_limit) {
                    height_candidates.insert(candidate);
                }
            }

            vector<vector<int>> item_orders;
            item_orders.push_back(items);
            vector<int> reversed = items;
            reverse(reversed.begin(), reversed.end());
            item_orders.push_back(reversed);

            vector<int> by_variation = items;
            sort(by_variation.begin(), by_variation.end(), [&](int first,
                                                               int second) {
                if (area_range[first] != area_range[second]) {
                    return area_range[first] < area_range[second];
                }
                return first < second;
            });
            item_orders.push_back(by_variation);

            for (int height : height_candidates) {
                BandOption best;
                best.wall_cost = INF;
                for (const vector<int>& order : item_orders) {
                    const long long cost = boundary_change_cost(
                        required_area, order, height, false, hall_width);
                    if (cost < best.wall_cost) {
                        best = {height, cost, false, order};
                    }
                }
                if (best.wall_cost < INF) options[begin][end].push_back(best);
            }
            if (fixed_height <= hall_width) {
                options[begin][end].push_back(
                    {fixed_height, 0, true, items});
            }
        }
    }

    vector<vector<long long>> dp(
        item_count + 1, vector<long long>(hall_width + 1, INF));
    struct Parent {
        int previous_position = -1;
        int previous_height = -1;
        int option_index = -1;
    };
    vector<vector<Parent>> parent(
        item_count + 1, vector<Parent>(hall_width + 1));
    dp[0][0] = 0;

    for (int begin = 0; begin < item_count; ++begin) {
        for (int used_height = 0; used_height <= hall_width; ++used_height) {
            if (dp[begin][used_height] == INF) continue;
            for (int end = begin + 1; end <= item_count; ++end) {
                for (int option_index = 0;
                     option_index < static_cast<int>(options[begin][end].size());
                     ++option_index) {
                    const BandOption& option = options[begin][end][option_index];
                    const int next_height = used_height + option.height;
                    if (next_height > hall_width) continue;
                    const long long next_cost =
                        dp[begin][used_height] + option.wall_cost;
                    if (next_cost < dp[end][next_height]) {
                        dp[end][next_height] = next_cost;
                        parent[end][next_height] =
                            {begin, used_height, option_index};
                    }
                }
            }
        }
    }

    int best_height = -1;
    for (int height = 1; height <= hall_width; ++height) {
        if (best_height == -1
            || dp[item_count][height] < dp[item_count][best_height]) {
            best_height = height;
        }
    }
    if (best_height == -1 || dp[item_count][best_height] == INF) return {};

    vector<BandOption> bands;
    int position = item_count;
    int used_height = best_height;
    while (position > 0) {
        const Parent step = parent[position][used_height];
        bands.push_back(
            options[step.previous_position][position][step.option_index]);
        position = step.previous_position;
        used_height = step.previous_height;
    }
    reverse(bands.begin(), bands.end());

    vector<vector<Rectangle>> answer(
        day_count, vector<Rectangle>(item_count));
    int top = 0;
    for (const BandOption& band : bands) {
        for (int day = 0; day < day_count; ++day) {
            const vector<int> width = make_widths(
                required_area,
                band.items,
                band.height,
                day,
                band.fixed_width,
                hall_width);
            int left_position = 0;
            for (int index = 0; index < static_cast<int>(band.items.size());
                 ++index) {
                const int item = band.items[index];
                answer[day][item] = {
                    top,
                    left_position,
                    top + band.height,
                    left_position + width[index],
                };
                left_position += width[index];
            }
        }
        top += band.height;
    }
    return answer;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int W, D, N;
    cin >> W >> D >> N;
    vector<vector<int>> required_area(D, vector<int>(N));
    for (auto& one_day : required_area) {
        for (int& area : one_day) cin >> area;
    }

    vector<vector<Rectangle>> best = make_strip_baseline(required_area, W);
    long long best_cost = calculate_cost(best, required_area, W);

#ifndef AHC031_STRIP_BASELINE
    for (const vector<int>& order : make_global_orders(required_area)) {
        vector<vector<Rectangle>> guillotine =
            make_guillotine_answer(required_area, order, W);
        if (!guillotine.empty()) {
            const long long guillotine_cost =
                calculate_cost(guillotine, required_area, W);
            if (guillotine_cost < best_cost) {
                best_cost = guillotine_cost;
                best = move(guillotine);
            }
        }

        vector<vector<Rectangle>> day_shelf =
            make_day_shelf_answer(required_area, order, W);
        set<uint64_t> partition_masks;
        if (!day_shelf.empty()) {
            for (int day = 0; day < D; ++day) {
                const vector<pair<int, int>> bands =
                    extract_band_partition(day_shelf[day], order);
                uint64_t mask = 0;
                for (const auto& [begin, end] : bands) {
                    static_cast<void>(begin);
                    if (end < N) mask |= 1ULL << (end - 1);
                }
                partition_masks.insert(mask);
            }
            const long long day_shelf_cost =
                calculate_cost(day_shelf, required_area, W);
            if (day_shelf_cost < best_cost) {
                best_cost = day_shelf_cost;
                best = move(day_shelf);
            }
        }

        for (int block_size = 1; block_size <= N; ++block_size) {
            uint64_t mask = 0;
            for (int end = block_size; end < N; end += block_size) {
                mask |= 1ULL << (end - 1);
            }
            partition_masks.insert(mask);
        }

        unordered_map<uint64_t, long long> partition_cost_cache;
        uint64_t best_partition_mask = 0;
        long long best_partition_cost = INF;
        auto evaluate_partition = [&](uint64_t mask) {
            const auto known = partition_cost_cache.find(mask);
            if (known != partition_cost_cache.end()) return known->second;
            vector<pair<int, int>> bands;
            int begin = 0;
            for (int position = 0; position + 1 < N; ++position) {
                if ((mask >> position & 1ULL) == 0) continue;
                bands.push_back({begin, position + 1});
                begin = position + 1;
            }
            bands.push_back({begin, N});
            vector<vector<Rectangle>> variable =
                make_variable_band_answer(required_area, order, bands, W);
            if (variable.empty()) {
                partition_cost_cache.emplace(mask, INF);
                return INF;
            }
            const long long variable_cost =
                calculate_cost(variable, required_area, W);
            partition_cost_cache.emplace(mask, variable_cost);
            if (variable_cost < best_cost) {
                best_cost = variable_cost;
                best = move(variable);
            }
            return variable_cost;
        };

        for (uint64_t mask : partition_masks) {
            const long long cost = evaluate_partition(mask);
            if (cost < best_partition_cost) {
                best_partition_cost = cost;
                best_partition_mask = mask;
            }
        }

        // Add or remove one band boundary at a time.  Every candidate is
        // rebuilt from scratch, so an infeasible intermediate layout cannot
        // corrupt the current best state.
        for (int pass = 0; pass < 4 && best_partition_cost < INF; ++pass) {
            uint64_t next_mask = best_partition_mask;
            long long next_cost = best_partition_cost;
            for (int position = 0; position + 1 < N; ++position) {
                const uint64_t candidate_mask =
                    best_partition_mask ^ (1ULL << position);
                const long long candidate_cost =
                    evaluate_partition(candidate_mask);
                if (candidate_cost < next_cost) {
                    next_cost = candidate_cost;
                    next_mask = candidate_mask;
                }
            }
            if (next_mask == best_partition_mask) break;
            best_partition_mask = next_mask;
            best_partition_cost = next_cost;
        }

        vector<vector<Rectangle>> candidate =
            make_common_band_answer(required_area, order, W);
        if (candidate.empty()) continue;
        const long long cost = calculate_cost(candidate, required_area, W);
        if (cost < best_cost) {
            best_cost = cost;
            best = move(candidate);
        }
    }
#endif

#ifdef LOCAL
    cerr << "cost = " << best_cost << '\n';
#else
    static_cast<void>(best_cost);
#endif

    for (int day = 0; day < D; ++day) {
        for (int item = 0; item < N; ++item) {
            const Rectangle rectangle = best[day][item];
            cout << rectangle.top << ' ' << rectangle.left << ' '
                 << rectangle.bottom << ' ' << rectangle.right << '\n';
        }
    }
}
