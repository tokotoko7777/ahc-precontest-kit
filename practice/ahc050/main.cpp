#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace std;

struct Problem {
    int n = 0;
    int initial_rocks = 0;
    vector<unsigned char> rock;
    vector<int> empty_cells;
};

struct Answer {
    vector<int> order;
    double expected_prize = -1.0;
};

// Move the whole probability distribution by one random skating move.
vector<double> move_robot(const Problem& problem,
                          const vector<unsigned char>& rock,
                          const vector<double>& probability) {
    const int n = problem.n;
    vector<double> next(n * n, 0.0);

    // In one horizontal empty segment, every left move stops at its left end,
    // and every right move stops at its right end.  We only need the sum of
    // probability in the segment, instead of sliding from every cell.
    for (int row = 0; row < n; ++row) {
        int column = 0;
        while (column < n) {
            if (rock[row * n + column] != 0) {
                ++column;
                continue;
            }
            const int left = column;
            double sum = 0.0;
            while (column < n && rock[row * n + column] == 0) {
                sum += probability[row * n + column];
                ++column;
            }
            const int right = column - 1;
            next[row * n + left] += 0.25 * sum;
            next[row * n + right] += 0.25 * sum;
        }
    }

    // The same observation works for vertical empty segments.
    for (int column = 0; column < n; ++column) {
        int row = 0;
        while (row < n) {
            if (rock[row * n + column] != 0) {
                ++row;
                continue;
            }
            const int top = row;
            double sum = 0.0;
            while (row < n && rock[row * n + column] == 0) {
                sum += probability[row * n + column];
                ++row;
            }
            const int bottom = row - 1;
            next[top * n + column] += 0.25 * sum;
            next[bottom * n + column] += 0.25 * sum;
        }
    }
    return next;
}

// Exact expected prize of one complete order.  This is the same probability
// update used by the official scorer.
double evaluate_order(const Problem& problem, const vector<int>& order) {
    const int empty_count = static_cast<int>(problem.empty_cells.size());
    vector<unsigned char> rock = problem.rock;
    vector<double> probability(problem.n * problem.n, 0.0);
    for (int cell : problem.empty_cells) {
        probability[cell] = 1.0 / empty_count;
    }

    double life = 1.0;
    double expected_prize = 0.0;
    for (int cell : order) {
        probability = move_robot(problem, rock, probability);
        life -= probability[cell];
        probability[cell] = 0.0;
        rock[cell] = 1;
        expected_prize += life;
    }
    return expected_prize;
}

// Greedy construction: place the next rock where the robot probability is
// minimum.  Many cells usually tie at probability zero.  Picking different
// tied cells creates very different future boards, so seed gives one restart.
Answer build_greedy_order(const Problem& problem, uint64_t seed) {
    const int empty_count = static_cast<int>(problem.empty_cells.size());
    vector<unsigned char> rock = problem.rock;
    vector<double> probability(problem.n * problem.n, 0.0);
    for (int cell : problem.empty_cells) {
        probability[cell] = 1.0 / empty_count;
    }

    mt19937_64 random(seed);
    Answer answer;
    answer.order.reserve(empty_count);
    answer.expected_prize = 0.0;
    double life = 1.0;

    for (int turn = 0; turn < empty_count; ++turn) {
        vector<double> moved = move_robot(problem, rock, probability);

        double minimum_death = numeric_limits<double>::infinity();
        for (int cell : problem.empty_cells) {
            if (rock[cell] == 0 && moved[cell] < minimum_death) {
                minimum_death = moved[cell];
            }
        }

        vector<int> tied;
        for (int cell : problem.empty_cells) {
            if (rock[cell] == 0 && moved[cell] <= minimum_death + 1e-15) {
                tied.push_back(cell);
            }
        }
        const int chosen = tied[random() % static_cast<uint64_t>(tied.size())];

        life -= moved[chosen];
        if (life < 0.0 && life > -1e-12) {
            life = 0.0;
        }
        moved[chosen] = 0.0;
        rock[chosen] = 1;
        probability = move(moved);
        answer.order.push_back(chosen);
        answer.expected_prize += life;
    }
    return answer;
}

void print_order(const Problem& problem, const vector<int>& order) {
    for (int cell : order) {
        cout << cell / problem.n << ' ' << cell % problem.n << '\n';
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Problem problem;
    cin >> problem.n >> problem.initial_rocks;
    problem.rock.assign(problem.n * problem.n, 0);
    for (int row = 0; row < problem.n; ++row) {
        string line;
        cin >> line;
        for (int column = 0; column < problem.n; ++column) {
            const int cell = row * problem.n + column;
            problem.rock[cell] = static_cast<unsigned char>(line[column] == '#');
            if (line[column] == '.') {
                problem.empty_cells.push_back(cell);
            }
        }
    }

    // The row-major answer is both a beginner baseline and a safety candidate.
    const vector<int> baseline = problem.empty_cells;
#ifdef SIMPLE_BASELINE
    print_order(problem, baseline);
    return 0;
#endif

    Answer best{baseline, evaluate_order(problem, baseline)};
    for (uint64_t seed = 10; seed < 42; ++seed) {
        Answer candidate = build_greedy_order(problem, seed);
        if (candidate.expected_prize > best.expected_prize) {
            best = move(candidate);
        }
    }

    print_order(problem, best.order);
    return 0;
}
