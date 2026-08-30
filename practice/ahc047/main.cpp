#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace std;

constexpr int STATE_COUNT = 12;
constexpr int ALPHABET_SIZE = 6;

struct Problem {
    int n = 0;
    int m = 0;
    int length = 0;
    vector<string> words;
    vector<int> values;
};

struct Model {
    array<char, STATE_COUNT> letter{};
    array<array<int, STATE_COUNT>, STATE_COUNT> transition{};
};

class Timer {
public:
    Timer() : start_(chrono::steady_clock::now()) {}

    double seconds() const {
        const auto now = chrono::steady_clock::now();
        return chrono::duration<double>(now - start_).count();
    }

private:
    chrono::steady_clock::time_point start_;
};

Model make_uniform_model() {
    Model model;
    for (int i = 0; i < STATE_COUNT; ++i) {
        model.letter[i] = static_cast<char>('a' + i % ALPHABET_SIZE);
        for (int j = 0; j < STATE_COUNT; ++j) {
            model.transition[i][j] = 0;
        }
        // Only states 0..5 are used.  The six next letters are almost uniform.
        for (int j = 0; j < ALPHABET_SIZE; ++j) {
            model.transition[i][j] = (j < 4 ? 17 : 16);
        }
    }
    return model;
}

void print_model(const Model& model) {
    for (int i = 0; i < STATE_COUNT; ++i) {
        cout << model.letter[i];
        for (int j = 0; j < STATE_COUNT; ++j) {
            cout << ' ' << model.transition[i][j];
        }
        cout << '\n';
    }
}

// Assigns an occurrence of a letter to one of its two states.  Different modes
// give different inexpensive guesses for which one-character context matters.
int occurrence_state(const string& word, int position, int mode) {
    const int length = static_cast<int>(word.size());
    const int current = word[position] - 'a';
    const int previous = word[(position + length - 1) % length] - 'a';
    const int next = word[(position + 1) % length] - 'a';

    int copy = 0;
    if (mode == 1) {
        copy = previous & 1;
    } else if (mode == 2) {
        copy = next & 1;
    } else if (mode == 3) {
        copy = (previous + 2 * next) & 1;
    }
    return current + copy * ALPHABET_SIZE;
}

// Converts non-negative real weights into an integer row whose sum is 100.
// Every edge receives at least one percent, which keeps the chain connected.
array<int, STATE_COUNT> rounded_row(const array<double, STATE_COUNT>& weight) {
    array<int, STATE_COUNT> result{};
    result.fill(1);

    const double sum = accumulate(weight.begin(), weight.end(), 0.0);
    if (sum <= 0.0) {
        for (int unit = 0; unit < 100 - STATE_COUNT; ++unit) {
            ++result[unit % STATE_COUNT];
        }
        return result;
    }

    constexpr int remaining = 100 - STATE_COUNT;
    array<double, STATE_COUNT> fraction{};
    int used = 0;
    for (int j = 0; j < STATE_COUNT; ++j) {
        const double exact = remaining * weight[j] / sum;
        const int whole = static_cast<int>(floor(exact));
        result[j] += whole;
        fraction[j] = exact - whole;
        used += whole;
    }

    array<int, STATE_COUNT> order{};
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int lhs, int rhs) {
        return fraction[lhs] > fraction[rhs];
    });
    for (int k = used; k < remaining; ++k) {
        ++result[order[k - used]];
    }
    return result;
}

Model make_learned_model(const Problem& problem, int top_count, int mode,
                         double value_power) {
    Model model;
    for (int i = 0; i < STATE_COUNT; ++i) {
        model.letter[i] = static_cast<char>('a' + i % ALPHABET_SIZE);
    }

    vector<int> order(problem.n);
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int lhs, int rhs) {
        return problem.values[lhs] > problem.values[rhs];
    });
    top_count = min(top_count, problem.n);

    array<array<double, STATE_COUNT>, STATE_COUNT> weight{};
    for (int rank = 0; rank < top_count; ++rank) {
        const int id = order[rank];
        const string& word = problem.words[id];
        const double importance = pow(static_cast<double>(problem.values[id]), value_power);
        const int length = static_cast<int>(word.size());
        for (int p = 0; p < length; ++p) {
            const int from = occurrence_state(word, p, mode);
            const int to = occurrence_state(word, (p + 1) % length, mode);
            weight[from][to] += importance / length;
        }
    }

    for (int i = 0; i < STATE_COUNT; ++i) {
        model.transition[i] = rounded_row(weight[i]);
    }
    return model;
}

bool appears_in_periodic_pattern(const string& word,
                                 const array<char, STATE_COUNT>& pattern) {
    for (int start = 0; start < STATE_COUNT; ++start) {
        bool matches = true;
        for (int k = 0; k < static_cast<int>(word.size()); ++k) {
            if (pattern[(start + k) % STATE_COUNT] != word[k]) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return true;
        }
    }
    return false;
}

long long periodic_score(const Problem& problem,
                         const array<char, STATE_COUNT>& pattern) {
    long long score = 0;
    for (int i = 0; i < problem.n; ++i) {
        if (appears_in_periodic_pattern(problem.words[i], pattern)) {
            score += problem.values[i];
        }
    }
    return score;
}

Model make_best_deterministic_model(const Problem& problem) {
    array<char, STATE_COUNT> best_pattern{};
    long long best_score = -1;

    // Repeating each favorite word is a small but always legal deterministic
    // candidate.  We retain the best of the 36 candidates.
    for (const string& word : problem.words) {
        array<char, STATE_COUNT> pattern{};
        for (int i = 0; i < STATE_COUNT; ++i) {
            pattern[i] = word[i % static_cast<int>(word.size())];
        }
        const long long score = periodic_score(problem, pattern);
        if (score > best_score) {
            best_score = score;
            best_pattern = pattern;
        }
    }

    Model model;
    model.letter = best_pattern;
    for (int i = 0; i < STATE_COUNT; ++i) {
        model.transition[i].fill(0);
        model.transition[i][(i + 1) % STATE_COUNT] = 100;
    }
    return model;
}

array<double, STATE_COUNT> stationary_distribution(const Model& model) {
    array<double, STATE_COUNT> probability{};
    array<double, STATE_COUNT> next{};
    probability.fill(1.0 / STATE_COUNT);

    // All searched matrices have at least 1% on every edge, so 28 iterations
    // are enough for the light-weight search estimate.
    for (int iteration = 0; iteration < 28; ++iteration) {
        next.fill(0.0);
        for (int i = 0; i < STATE_COUNT; ++i) {
            for (int j = 0; j < STATE_COUNT; ++j) {
                next[j] += probability[i] * model.transition[i][j] / 100.0;
            }
        }
        probability = next;
    }
    return probability;
}

// Fast search objective: expected occurrences at a stationary random position,
// followed by the usual Poisson approximation P(at least once)=1-exp(-lambda).
double approximate_score(const Problem& problem, const Model& model) {
    const auto stationary = stationary_distribution(model);
    double total = 0.0;

    for (int word_id = 0; word_id < problem.n; ++word_id) {
        const string& word = problem.words[word_id];
        array<double, STATE_COUNT> probability{};
        for (int state = 0; state < STATE_COUNT; ++state) {
            if (model.letter[state] == word[0]) {
                probability[state] = stationary[state];
            }
        }

        for (int position = 1; position < static_cast<int>(word.size()); ++position) {
            array<double, STATE_COUNT> next{};
            for (int from = 0; from < STATE_COUNT; ++from) {
                if (probability[from] == 0.0) {
                    continue;
                }
                for (int to = 0; to < STATE_COUNT; ++to) {
                    if (model.letter[to] == word[position]) {
                        next[to] += probability[from] * model.transition[from][to] / 100.0;
                    }
                }
            }
            probability = next;
        }

        const double block_probability =
            accumulate(probability.begin(), probability.end(), 0.0);
        const double windows = problem.length - static_cast<int>(word.size()) + 1.0;
        const double hit_probability = -expm1(-block_probability * windows);
        total += problem.values[word_id] * hit_probability;
    }
    return total;
}

vector<double> multiply_matrix(const vector<double>& lhs,
                               const vector<double>& rhs, int size) {
    vector<double> result(size * size, 0.0);
    for (int row = 0; row < size; ++row) {
        for (int middle = 0; middle < size; ++middle) {
            const double left = lhs[row * size + middle];
            if (left == 0.0) {
                continue;
            }
            for (int column = 0; column < size; ++column) {
                result[row * size + column] +=
                    left * rhs[middle * size + column];
            }
        }
    }
    return result;
}

vector<double> multiply_matrix_vector(const vector<double>& matrix,
                                      const vector<double>& vector_value,
                                      int size) {
    vector<double> result(size, 0.0);
    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size; ++column) {
            result[row] += matrix[row * size + column] * vector_value[column];
        }
    }
    return result;
}

// Exact probability used by the official scorer.  A state is
// (matched KMP-prefix length, model state).  Completing the word is absorbing,
// so transitions that complete it are deliberately omitted.
double exact_word_probability(const string& word, int generated_length,
                              const Model& model) {
    const int word_length = static_cast<int>(word.size());
    vector<int> prefix(word_length, 0);
    for (int i = 1; i < word_length; ++i) {
        int j = prefix[i - 1];
        while (j > 0 && word[i] != word[j]) {
            j = prefix[j - 1];
        }
        if (word[i] == word[j]) {
            ++j;
        }
        prefix[i] = j;
    }

    vector<array<int, STATE_COUNT>> id(word_length);
    for (auto& row : id) {
        row.fill(-1);
    }
    vector<pair<int, int>> state;
    for (int model_state = 0; model_state < STATE_COUNT; ++model_state) {
        id[0][model_state] = static_cast<int>(state.size());
        state.emplace_back(0, model_state);
        for (int matched = 1; matched < word_length; ++matched) {
            if (word[matched - 1] == model.letter[model_state]) {
                id[matched][model_state] = static_cast<int>(state.size());
                state.emplace_back(matched, model_state);
            }
        }
    }

    const int size = static_cast<int>(state.size());
    vector<double> matrix(size * size, 0.0);
    for (int from_id = 0; from_id < size; ++from_id) {
        const int matched = state[from_id].first;
        const int from = state[from_id].second;
        for (int to = 0; to < STATE_COUNT; ++to) {
            int next_matched = matched;
            const char next_letter = model.letter[to];
            while (next_matched > 0 && word[next_matched] != next_letter) {
                next_matched = prefix[next_matched - 1];
            }
            if (word[next_matched] == next_letter) {
                ++next_matched;
            }
            if (next_matched == word_length) {
                continue;
            }
            const int to_id = id[next_matched][to];
            matrix[to_id * size + from_id] += model.transition[from][to] / 100.0;
        }
    }

    int initially_matched = (model.letter[0] == word[0] ? 1 : 0);
    if (initially_matched == word_length) {
        return 1.0;
    }
    vector<double> probability(size, 0.0);
    probability[id[initially_matched][0]] = 1.0;

    int power = generated_length - 1;
    while (power > 0) {
        if ((power & 1) != 0) {
            probability = multiply_matrix_vector(matrix, probability, size);
        }
        power >>= 1;
        if (power > 0) {
            matrix = multiply_matrix(matrix, matrix, size);
        }
    }

    double survival_probability =
        accumulate(probability.begin(), probability.end(), 0.0);
    return clamp(1.0 - survival_probability, 0.0, 1.0);
}

double exact_score(const Problem& problem, const Model& model) {
    double total = 0.0;
    for (int i = 0; i < problem.n; ++i) {
        total += problem.values[i] *
                 exact_word_probability(problem.words[i], problem.length, model);
    }
    return total;
}

uint64_t input_seed(const Problem& problem) {
    uint64_t hash = 1469598103934665603ULL;
    for (int i = 0; i < problem.n; ++i) {
        for (char c : problem.words[i]) {
            hash ^= static_cast<unsigned char>(c);
            hash *= 1099511628211ULL;
        }
        hash ^= static_cast<uint64_t>(problem.values[i]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

Model improve_model(const Problem& problem, Model current, const Timer& timer,
                    double end_time) {
    mt19937_64 random(input_seed(problem));
    uniform_real_distribution<double> random_real(0.0, 1.0);
    double current_score = approximate_score(problem, current);
    double best_score = current_score;
    Model best = current;
    const double start_time = timer.seconds();
    const double total_value =
        accumulate(problem.values.begin(), problem.values.end(), 0.0);
    const double start_temperature = max(10.0, total_value * 0.0025);
    constexpr double end_temperature = 0.02;

    int iteration = 0;
    while (true) {
        if ((iteration & 127) == 0 && timer.seconds() >= end_time) {
            break;
        }
        ++iteration;

        const bool swap_letters = random_real(random) < 0.12;
        int first = 0;
        int second = 0;
        int amount = 0;
        if (swap_letters) {
            first = static_cast<int>(random() % STATE_COUNT);
            second = static_cast<int>(random() % STATE_COUNT);
            if (first == second || current.letter[first] == current.letter[second]) {
                continue;
            }
            swap(current.letter[first], current.letter[second]);
        } else {
            const int row = static_cast<int>(random() % STATE_COUNT);
            first = row * STATE_COUNT + static_cast<int>(random() % STATE_COUNT);
            second = row * STATE_COUNT + static_cast<int>(random() % STATE_COUNT);
            if (first == second) {
                continue;
            }
            const int donor = first % STATE_COUNT;
            const int receiver = second % STATE_COUNT;
            amount = 1 + static_cast<int>(random() % 4);
            if (current.transition[row][donor] - amount < 1) {
                continue;
            }
            current.transition[row][donor] -= amount;
            current.transition[row][receiver] += amount;
        }

        const double next_score = approximate_score(problem, current);
        const double elapsed = timer.seconds();
        const double progress = clamp((elapsed - start_time) / (end_time - start_time), 0.0, 1.0);
        const double temperature =
            start_temperature * pow(end_temperature / start_temperature, progress);
        const double difference = next_score - current_score;
        const bool accept = difference >= 0.0 ||
                            random_real(random) < exp(difference / temperature);

        if (accept) {
            current_score = next_score;
            if (current_score > best_score) {
                best_score = current_score;
                best = current;
            }
        } else if (swap_letters) {
            swap(current.letter[first], current.letter[second]);
        } else {
            const int row = first / STATE_COUNT;
            const int donor = first % STATE_COUNT;
            const int receiver = second % STATE_COUNT;
            current.transition[row][donor] += amount;
            current.transition[row][receiver] -= amount;
        }
    }
    return best;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Problem problem;
    cin >> problem.n >> problem.m >> problem.length;
    problem.words.resize(problem.n);
    problem.values.resize(problem.n);
    for (int i = 0; i < problem.n; ++i) {
        cin >> problem.words[i] >> problem.values[i];
    }

    const Model baseline = make_uniform_model();

#ifdef SIMPLE_BASELINE
    print_model(baseline);
    return 0;
#endif

    Timer timer;
    vector<pair<double, Model>> learned;
    const array<int, 4> top_counts = {3, 6, 12, 36};
    const array<double, 2> powers = {0.55, 1.0};
    for (int top_count : top_counts) {
        for (int mode = 0; mode < 4; ++mode) {
            for (double power : powers) {
                Model model = make_learned_model(problem, top_count, mode, power);
                learned.emplace_back(approximate_score(problem, model), model);
            }
        }
    }
    sort(learned.begin(), learned.end(),
         [](const auto& lhs, const auto& rhs) { return lhs.first > rhs.first; });

    Model searched = improve_model(problem, learned.front().second, timer, 0.62);

    vector<Model> finalists;
    finalists.push_back(baseline);
    finalists.push_back(make_best_deterministic_model(problem));
    finalists.push_back(learned[0].second);
    finalists.push_back(learned[1].second);
    finalists.push_back(searched);

    Model answer = finalists.front();
    double answer_score = -numeric_limits<double>::infinity();
    for (const Model& candidate : finalists) {
        const double score = exact_score(problem, candidate);
        if (score > answer_score) {
            answer_score = score;
            answer = candidate;
        }
    }

    print_model(answer);
    return 0;
}
