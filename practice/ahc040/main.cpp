#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <tuple>
#include <vector>
using namespace std;

#ifndef AHC040_MEASUREMENT_LIMIT
#define AHC040_MEASUREMENT_LIMIT 20
#endif

struct Operation {
    int rectangle;
    int rotate;
    char direction;
    int base;
};

struct Position {
    double left = 0;
    double top = 0;
    double width = 0;
    double height = 0;
};

struct Layout {
    vector<Operation> operations;
    double predicted_width = 0;
    double predicted_height = 0;
};

bool overlap(double left1, double right1, double left2, double right2) {
    return max(left1, left2) < min(right1, right2);
}

Position place_rectangle(const vector<Position>& placed, double width,
                         double height, char direction, int base) {
    Position result;
    result.width = width;
    result.height = height;

    if (direction == 'U') {
        result.left =
            base == -1 ? 0 : placed[base].left + placed[base].width;
        result.top = 0;
        for (const Position& other : placed) {
            if (overlap(result.left, result.left + width, other.left,
                        other.left + other.width)) {
                result.top = max(result.top, other.top + other.height);
            }
        }
    } else {
        result.top =
            base == -1 ? 0 : placed[base].top + placed[base].height;
        result.left = 0;
        for (const Position& other : placed) {
            if (overlap(result.top, result.top + height, other.top,
                        other.top + other.height)) {
                result.left = max(result.left, other.left + other.width);
            }
        }
    }
    return result;
}

// Greedily place every next rectangle at one of the x/y coordinates already
// present on the skyline.  target_width controls the aspect ratio.
Layout make_compact_layout(const vector<double>& estimated_width,
                           const vector<double>& estimated_height,
                           double target_width, double overflow_weight,
                           double waste_weight, double random_strength,
                           uint64_t random_seed) {
    const int n = (int)estimated_width.size();
    mt19937_64 random(random_seed);
    uniform_real_distribution<double> unit(0.0, 1.0);
    vector<Position> placed;
    vector<Operation> operations;
    placed.reserve(n);
    operations.reserve(n);

    double bounding_width = 0;
    double bounding_height = 0;
    double used_area = 0;
    const double typical_length =
        sqrt(inner_product(estimated_width.begin(), estimated_width.end(),
                           estimated_height.begin(), 0.0) /
             n);

    for (int i = 0; i < n; ++i) {
        double best_value = numeric_limits<double>::infinity();
        Position best_position;
        Operation best_operation{i, 0, 'U', -1};

        for (int rotate = 0; rotate < 2; ++rotate) {
            double width = rotate ? estimated_height[i] : estimated_width[i];
            double height = rotate ? estimated_width[i] : estimated_height[i];
            for (char direction : {'U', 'L'}) {
                for (int base = -1; base < i; ++base) {
                    Position position =
                        place_rectangle(placed, width, height, direction, base);
                    double next_width =
                        max(bounding_width, position.left + width);
                    double next_height =
                        max(bounding_height, position.top + height);
                    double next_area = used_area + width * height;
                    double empty_area =
                        max(0.0, next_width * next_height - next_area);

                    double value = next_height + 0.025 * next_width;
                    value += overflow_weight *
                             max(0.0, next_width - target_width);
                    value += waste_weight * empty_area /
                             max(target_width, 1.0);
                    value += random_strength * typical_length * unit(random);

                    if (value < best_value) {
                        best_value = value;
                        best_position = position;
                        best_operation = {i, rotate, direction, base};
                    }
                }
            }
        }

        placed.push_back(best_position);
        operations.push_back(best_operation);
        bounding_width =
            max(bounding_width, best_position.left + best_position.width);
        bounding_height =
            max(bounding_height, best_position.top + best_position.height);
        used_area += best_position.width * best_position.height;
    }

    return {operations, bounding_width, bounding_height};
}

vector<Operation> make_measurement_layout(int n, int query, bool horizontal) {
    vector<Operation> operations;
    operations.reserve(n);
    for (int i = 0; i < n; ++i) {
        // A deterministic random-looking binary measurement matrix.
        uint64_t value = uint64_t(i + 1) * 0x9e3779b97f4a7c15ULL;
        value ^= uint64_t(query + 11) * 0xbf58476d1ce4e5b9ULL;
        value ^= value >> 30;
        int rotate = int(value & 1);
        operations.push_back(
            {i, rotate, horizontal ? 'U' : 'L', i == 0 ? -1 : i - 1});
    }
    return operations;
}

pair<long long, long long> ask(const vector<Operation>& operations) {
    cout << operations.size() << '\n';
    for (const Operation& operation : operations) {
        cout << operation.rectangle << ' ' << operation.rotate << ' '
             << operation.direction << ' ' << operation.base << '\n';
    }
    cout.flush();

    long long measured_width, measured_height;
    cin >> measured_width >> measured_height;
    return {measured_width, measured_height};
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, turn_count;
    double sigma;
    cin >> n >> turn_count >> sigma;
    vector<double> width(n), height(n);
    for (int i = 0; i < n; ++i) cin >> width[i] >> height[i];

#ifdef AHC040_BASELINE
    vector<Operation> baseline;
    for (int i = 0; i < n; ++i) {
        int rotate = height[i] < width[i];
        baseline.push_back({i, rotate, 'U', i == 0 ? -1 : i - 1});
    }
    for (int turn = 0; turn < turn_count; ++turn) ask(baseline);
    return 0;
#endif

    // The input observation is one noisy sample of every dimension.
    // Keep a diagonal variance and apply a simple Kalman update to exact-sum
    // row/column measurements.
    vector<double> original_width = width;
    vector<double> original_height = height;
    const int dimension_count = 2 * n;
    vector<double> estimate(dimension_count);
    vector<vector<double>> covariance(
        dimension_count, vector<double>(dimension_count));
    for (int i = 0; i < n; ++i) {
        estimate[2 * i] = width[i];
        estimate[2 * i + 1] = height[i];
        covariance[2 * i][2 * i] = sigma * sigma;
        covariance[2 * i + 1][2 * i + 1] = sigma * sigma;
    }

    int measurement_turns =
        min({AHC040_MEASUREMENT_LIMIT, n / 4, turn_count / 4});
#ifdef AHC040_NO_MEASUREMENT
    measurement_turns = 0;
#endif
    for (int turn = 0; turn < measurement_turns; ++turn) {
        bool horizontal = turn % 2 == 0;
        vector<Operation> operations =
            make_measurement_layout(n, turn, horizontal);
        auto [observed_width, observed_height] = ask(operations);

        double prediction = 0;
        vector<int> measured_dimensions;
        measured_dimensions.reserve(n);
        for (const Operation& operation : operations) {
            int i = operation.rectangle;
            int dimension;
            if (horizontal) {
                dimension = 2 * i + operation.rotate;
            } else {
                dimension = 2 * i + 1 - operation.rotate;
            }
            measured_dimensions.push_back(dimension);
            prediction += estimate[dimension];
        }
        double observation = horizontal ? double(observed_width)
                                        : double(observed_height);
        double residual = observation - prediction;

        vector<double> covariance_times_measurement(dimension_count, 0.0);
        for (int row = 0; row < dimension_count; ++row) {
            for (int dimension : measured_dimensions) {
                covariance_times_measurement[row] +=
                    covariance[row][dimension];
            }
        }
        double uncertainty = sigma * sigma;
        for (int dimension : measured_dimensions) {
            uncertainty += covariance_times_measurement[dimension];
        }
        for (int row = 0; row < dimension_count; ++row) {
            estimate[row] +=
                covariance_times_measurement[row] / uncertainty * residual;
        }
        for (int row = 0; row < dimension_count; ++row) {
            for (int column = 0; column < dimension_count; ++column) {
                covariance[row][column] -=
                    covariance_times_measurement[row] *
                    covariance_times_measurement[column] / uncertainty;
            }
        }

        // A very unlikely observation should not be allowed to make an item
        // negative or many sigmas away from its direct measurement.
        for (int i = 0; i < n; ++i) {
            estimate[2 * i] =
                clamp(estimate[2 * i],
                      max(1.0, original_width[i] - 4 * sigma),
                      original_width[i] + 4 * sigma);
            estimate[2 * i + 1] =
                clamp(estimate[2 * i + 1],
                      max(1.0, original_height[i] - 4 * sigma),
                      original_height[i] + 4 * sigma);
            width[i] = estimate[2 * i];
            height[i] = estimate[2 * i + 1];
        }
    }

    double total_area = inner_product(width.begin(), width.end(),
                                      height.begin(), 0.0);
    double square_side = sqrt(total_area);
    double best_factor = 1.0;
    double best_observation = numeric_limits<double>::infinity();
    const int packing_turns = turn_count - measurement_turns;

    mt19937_64 random(20250119);
    normal_distribution<double> normal(0.0, 1.0);
    for (int step = 0; step < packing_turns; ++step) {
        double factor;
        if (step < min(24, packing_turns)) {
            if (packing_turns == 1) {
                factor = 1.0;
            } else {
                factor = 0.62 + 0.92 * step /
                                    max(1, min(24, packing_turns) - 1);
            }
        } else {
            factor = best_factor * exp(0.11 * normal(random));
            factor = clamp(factor, 0.50, 1.80);
        }

        double overflow_weight = 2.5 + 1.5 * (step % 4);
        double waste_weight = 0.04 * ((step / 4) % 4);
        double random_strength = step < 12 ? 0.0 : 0.012 * (step % 5);
        Layout layout = make_compact_layout(
            width, height, square_side * factor, overflow_weight,
            waste_weight, random_strength,
            1000003ULL * uint64_t(step + 1));
        auto [observed_width, observed_height] = ask(layout.operations);
        double observed_score =
            double(observed_width) + double(observed_height);
        if (observed_score < best_observation) {
            best_observation = observed_score;
            best_factor = factor;
        }
    }
}
