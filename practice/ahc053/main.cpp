#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

using namespace std;

using int64 = long long;

constexpr int BIT_COUNT = 15;
constexpr int RANGE_NUMERATOR = 10365;
constexpr int RANGE_DENOMINATOR = 10000;
constexpr int64 INF_COST = numeric_limits<int64>::max() / 4;

// Smaller denominations are cheap to replace during shortage repair, so more
// copies are reserved for the larger and harder-to-replace denominations.
constexpr array<int, BIT_COUNT> BIT_CAPACITY = {
    27, 27, 27, 29, 29, 29, 29, 29, 32, 32, 32, 32, 32, 32, 32,
};
static_assert(27 * 3 + 29 * 5 + 32 * 7 == 450);

struct DigitAnswer {
    int64 linear_cost = INF_COST;
    int code = 0;
};

class NearestSubset {
   public:
    explicit NearestSubset(vector<int64> bit_value)
        : bit_value_(move(bit_value)),
          max_code_((1 << BIT_COUNT) - 1),
          subset_sum_(max_code_ + 1, 0) {
        for (int code = 1; code <= max_code_; ++code) {
            int bit = __builtin_ctz(static_cast<unsigned>(code));
            subset_sum_[code] =
                subset_sum_[code ^ (1 << bit)] + bit_value_[bit];
        }
        assert(is_sorted(subset_sum_.begin(), subset_sum_.end()));
    }

    int nearest(int64 target, int forbidden_mask) {
        int best_code = 0;
        int64 best_error = llabs(target);

        int floor_code = static_cast<int>(
                             upper_bound(subset_sum_.begin(),
                                         subset_sum_.end(), target) -
                             subset_sum_.begin()) -
                         1;
        if (floor_code >= 0) {
            DigitAnswer answer = search_between(
                0, floor_code, -1, forbidden_mask);
            consider(target, answer.code, answer.linear_cost, best_code,
                     best_error);
        }

        int ceil_code = static_cast<int>(
            lower_bound(subset_sum_.begin(), subset_sum_.end(), target) -
            subset_sum_.begin());
        if (ceil_code <= max_code_) {
            DigitAnswer answer = search_between(
                ceil_code, max_code_, 1, forbidden_mask);
            consider(target, answer.code, answer.linear_cost, best_code,
                     best_error);
        }
        return best_code;
    }

    int64 sum(int code) const { return subset_sum_[code]; }

   private:
    vector<int64> bit_value_;
    int max_code_;
    vector<int64> subset_sum_;

    int lower_code_ = 0;
    int upper_code_ = 0;
    int coefficient_ = 0;
    int forbidden_mask_ = 0;
    array<array<DigitAnswer, 4>, BIT_COUNT> memo_{};
    array<array<bool, 4>, BIT_COUNT> seen_{};

    void consider(int64 target, int code, int64 linear_cost, int& best_code,
                  int64& best_error) const {
        if (linear_cost >= INF_COST) return;
        int64 error = llabs(subset_sum_[code] - target);
        if (error < best_error ||
            (error == best_error && code < best_code)) {
            best_error = error;
            best_code = code;
        }
    }

    DigitAnswer search_between(int lower_code, int upper_code,
                               int coefficient, int forbidden_mask) {
        lower_code_ = lower_code;
        upper_code_ = upper_code;
        coefficient_ = coefficient;
        forbidden_mask_ = forbidden_mask;
        for (auto& row : seen_) row.fill(false);
        return search(BIT_COUNT - 1, 3);
    }

    DigitAnswer search(int bit, int tight_mask) {
        if (bit < 0) return {0, 0};
        if (seen_[bit][tight_mask]) return memo_[bit][tight_mask];
        seen_[bit][tight_mask] = true;

        DigitAnswer best;
        int lower_bit = (lower_code_ >> bit) & 1;
        int upper_bit = (upper_code_ >> bit) & 1;
        for (int digit = 0; digit <= 1; ++digit) {
            if (digit == 1 && ((forbidden_mask_ >> bit) & 1) != 0) {
                continue;
            }
            if ((tight_mask & 1) != 0 && digit < lower_bit) continue;
            if ((tight_mask & 2) != 0 && digit > upper_bit) continue;

            int next_tight = 0;
            if ((tight_mask & 1) != 0 && digit == lower_bit) {
                next_tight |= 1;
            }
            if ((tight_mask & 2) != 0 && digit == upper_bit) {
                next_tight |= 2;
            }

            DigitAnswer candidate = search(bit - 1, next_tight);
            if (candidate.linear_cost >= INF_COST) continue;
            candidate.linear_cost +=
                static_cast<int64>(coefficient_) * digit * bit_value_[bit];
            candidate.code |= digit << bit;
            if (candidate.linear_cost < best.linear_cost ||
                (candidate.linear_cost == best.linear_cost &&
                 candidate.code < best.code)) {
                best = candidate;
            }
        }
        memo_[bit][tight_mask] = best;
        return best;
    }
};

int total_excess(const array<int, BIT_COUNT>& used) {
    int answer = 0;
    for (int bit = 0; bit < BIT_COUNT; ++bit) {
        answer += max(0, used[bit] - BIT_CAPACITY[bit]);
    }
    return answer;
}

void apply_code_change(int old_code, int new_code,
                       array<int, BIT_COUNT>& used) {
    for (int bit = 0; bit < BIT_COUNT; ++bit) {
        used[bit] += ((new_code >> bit) & 1) - ((old_code >> bit) & 1);
    }
}

[[maybe_unused]] void repair_card_shortages(
    const vector<int64>& target, NearestSubset& subset, vector<int>& code,
    vector<int64>& error, array<int, BIT_COUNT>& used) {
    while (total_excess(used) > 0) {
        int old_excess = total_excess(used);
        int overloaded_mask = 0;
        for (int bit = 0; bit < BIT_COUNT; ++bit) {
            if (used[bit] > BIT_CAPACITY[bit]) {
                overloaded_mask |= 1 << bit;
            }
        }

        int best_pile = -1;
        int best_code = 0;
        int best_reduction = 0;
        int64 best_increase = INF_COST;

        for (int pile = 0; pile < static_cast<int>(code.size()); ++pile) {
            vector<int> candidates;
            candidates.push_back(code[pile] & ~overloaded_mask);
            candidates.push_back(
                subset.nearest(target[pile], overloaded_mask));
            for (int bit = 0; bit < BIT_COUNT; ++bit) {
                if (((overloaded_mask >> bit) & 1) != 0 &&
                    ((code[pile] >> bit) & 1) != 0) {
                    candidates.push_back(
                        subset.nearest(target[pile], 1 << bit));
                }
            }
            sort(candidates.begin(), candidates.end());
            candidates.erase(unique(candidates.begin(), candidates.end()),
                             candidates.end());

            for (int candidate_code : candidates) {
                if (candidate_code == code[pile]) continue;
                array<int, BIT_COUNT> next_used = used;
                apply_code_change(code[pile], candidate_code, next_used);
                int reduction = old_excess - total_excess(next_used);
                if (reduction <= 0) continue;

                int64 next_error =
                    llabs(subset.sum(candidate_code) - target[pile]);
                int64 increase = next_error - error[pile];
                bool better_ratio =
                    best_pile < 0 ||
                    increase * best_reduction < best_increase * reduction;
                bool same_ratio =
                    best_pile >= 0 &&
                    increase * best_reduction == best_increase * reduction;
                if (better_ratio ||
                    (same_ratio && increase < best_increase)) {
                    best_pile = pile;
                    best_code = candidate_code;
                    best_reduction = reduction;
                    best_increase = increase;
                }
            }
        }

        assert(best_pile >= 0);
        apply_code_change(code[best_pile], best_code, used);
        code[best_pile] = best_code;
        error[best_pile] =
            llabs(subset.sum(best_code) - target[best_pile]);
    }
}

[[maybe_unused]] void improve_one_pile_at_a_time(
    const vector<int64>& target, NearestSubset& subset, vector<int>& code,
    vector<int64>& error, array<int, BIT_COUNT>& used) {
    for (int pass = 0; pass < 5; ++pass) {
        bool changed = false;
        for (int pile = 0; pile < static_cast<int>(code.size()); ++pile) {
            int forbidden_mask = 0;
            for (int bit = 0; bit < BIT_COUNT; ++bit) {
                bool pile_already_uses_card = ((code[pile] >> bit) & 1) != 0;
                if (used[bit] >= BIT_CAPACITY[bit] &&
                    !pile_already_uses_card) {
                    forbidden_mask |= 1 << bit;
                }
            }
            int next_code = subset.nearest(target[pile], forbidden_mask);
            int64 next_error = llabs(subset.sum(next_code) - target[pile]);
            if (next_error < error[pile]) {
                apply_code_change(code[pile], next_code, used);
                code[pile] = next_code;
                error[pile] = next_error;
                changed = true;
            }
        }
        if (!changed) break;
    }
}

[[maybe_unused]] void improve_two_piles_at_a_time(
    const vector<int64>& target, NearestSubset& subset, vector<int>& code,
    vector<int64>& error, array<int, BIT_COUNT>& used) {
    int pile_count = static_cast<int>(code.size());
    vector<vector<int>> alternative(pile_count);
    for (int pile = 0; pile < pile_count; ++pile) {
        alternative[pile].push_back(code[pile]);
        alternative[pile].push_back(subset.nearest(target[pile], 0));
        for (int bit = 0; bit < BIT_COUNT; ++bit) {
            alternative[pile].push_back(
                subset.nearest(target[pile], 1 << bit));
        }
        sort(alternative[pile].begin(), alternative[pile].end());
        alternative[pile].erase(
            unique(alternative[pile].begin(), alternative[pile].end()),
            alternative[pile].end());
    }

    for (int round = 0; round < 30; ++round) {
        int best_left = -1;
        int best_right = -1;
        int best_left_code = 0;
        int best_right_code = 0;
        int64 best_gain = 0;

        for (int left = 0; left < pile_count; ++left) {
            for (int right = left + 1; right < pile_count; ++right) {
                for (int left_code : alternative[left]) {
                    int64 left_error =
                        llabs(subset.sum(left_code) - target[left]);
                    for (int right_code : alternative[right]) {
                        int64 right_error =
                            llabs(subset.sum(right_code) - target[right]);
                        int64 gain = error[left] + error[right] - left_error -
                                     right_error;
                        if (gain <= best_gain) continue;

                        bool feasible = true;
                        for (int bit = 0; bit < BIT_COUNT; ++bit) {
                            int next_used =
                                used[bit] +
                                ((left_code >> bit) & 1) -
                                ((code[left] >> bit) & 1) +
                                ((right_code >> bit) & 1) -
                                ((code[right] >> bit) & 1);
                            if (next_used > BIT_CAPACITY[bit]) {
                                feasible = false;
                                break;
                            }
                        }
                        if (feasible) {
                            best_gain = gain;
                            best_left = left;
                            best_right = right;
                            best_left_code = left_code;
                            best_right_code = right_code;
                        }
                    }
                }
            }
        }

        if (best_left < 0) break;
        apply_code_change(code[best_left], best_left_code, used);
        apply_code_change(code[best_right], best_right_code, used);
        code[best_left] = best_left_code;
        code[best_right] = best_right_code;
        error[best_left] =
            llabs(subset.sum(best_left_code) - target[best_left]);
        error[best_right] =
            llabs(subset.sum(best_right_code) - target[best_right]);
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int card_count;
    int pile_count;
    int64 lower_target;
    int64 upper_target;
    cin >> card_count >> pile_count >> lower_target >> upper_target;

#ifdef AHC053_BASELINE
    int64 card_value = (lower_target + upper_target) / 20;
    for (int card = 0; card < card_count; ++card) {
        if (card != 0) cout << ' ';
        cout << card_value;
    }
    cout << endl;

    int64 ignored_target;
    for (int pile = 0; pile < pile_count; ++pile) cin >> ignored_target;
    int cards_per_pile = card_count / pile_count;
    for (int card = 0; card < card_count; ++card) {
        if (card != 0) cout << ' ';
        cout << card / cards_per_pile + 1;
    }
    cout << endl;
    return 0;
#else
    assert(card_count == 500);
    assert(pile_count == 50);

    int64 target_width = upper_target - lower_target;
    int64 represented_range =
        target_width * RANGE_NUMERATOR / RANGE_DENOMINATOR;
    int max_code = (1 << BIT_COUNT) - 1;
    vector<int64> bit_value(BIT_COUNT);
    for (int bit = 0; bit < BIT_COUNT; ++bit) {
        int64 numerator = represented_range * (1LL << bit);
        bit_value[bit] = (numerator + max_code / 2) / max_code;
    }

    bool first = true;
    for (int pile = 0; pile < pile_count; ++pile) {
        if (!first) cout << ' ';
        first = false;
        cout << lower_target;
    }
    for (int bit = 0; bit < BIT_COUNT; ++bit) {
        for (int copy = 0; copy < BIT_CAPACITY[bit]; ++copy) {
            cout << ' ' << bit_value[bit];
        }
    }
    cout << endl;

    vector<int64> target(pile_count);
    for (int pile = 0; pile < pile_count; ++pile) {
        cin >> target[pile];
        target[pile] -= lower_target;
    }

    NearestSubset subset(bit_value);
    vector<int> code(pile_count);
    vector<int64> error(pile_count);
    array<int, BIT_COUNT> used{};
    for (int pile = 0; pile < pile_count; ++pile) {
        code[pile] = subset.nearest(target[pile], 0);
        error[pile] = llabs(subset.sum(code[pile]) - target[pile]);
        for (int bit = 0; bit < BIT_COUNT; ++bit) {
            used[bit] += (code[pile] >> bit) & 1;
        }
    }

#ifdef LOCAL
    int initial_excess = total_excess(used);
#endif
    repair_card_shortages(target, subset, code, error, used);
    improve_one_pile_at_a_time(target, subset, code, error, used);
    improve_two_piles_at_a_time(target, subset, code, error, used);

    vector<int> assignment(card_count, 0);
    for (int pile = 0; pile < pile_count; ++pile) {
        assignment[pile] = pile + 1;
    }
    int card_index = pile_count;
    for (int bit = 0; bit < BIT_COUNT; ++bit) {
        vector<int> users;
        for (int pile = 0; pile < pile_count; ++pile) {
            if (((code[pile] >> bit) & 1) != 0) {
                users.push_back(pile + 1);
            }
        }
        assert(static_cast<int>(users.size()) <= BIT_CAPACITY[bit]);
        for (int copy = 0; copy < BIT_CAPACITY[bit]; ++copy) {
            if (copy < static_cast<int>(users.size())) {
                assignment[card_index] = users[copy];
            }
            ++card_index;
        }
    }
    assert(card_index == card_count);

#ifdef LOCAL
    cerr << "initial_excess=" << initial_excess
         << " total_error=" << accumulate(error.begin(), error.end(), 0LL)
         << '\n';
#endif

    for (int card = 0; card < card_count; ++card) {
        if (card != 0) cout << ' ';
        cout << assignment[card];
    }
    cout << endl;
    return 0;
#endif
}
