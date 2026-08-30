#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

using namespace std;

// Deterministic and fast random numbers, used only for making noisy templates.
struct Rng {
    uint64_t state;

    explicit Rng(uint64_t seed) : state(seed) {}

    uint64_t next() {
        state += 0x9e3779b97f4a7c15ULL;
        uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    bool bernoulli(double probability) {
        constexpr double TO_UNIT = 1.0 / 9007199254740992.0;
        return static_cast<double>(next() >> 11) * TO_UNIT < probability;
    }
};

// A small graph has five vertex types.  Its 15 bits are:
//   - bits 0..4: whether each type is a clique (otherwise an independent set)
//   - the other 10 bits: whether every edge between two types exists
static int bit_index(int a, int b) {
    if (a > b) swap(a, b);
    if (a == b) return a;

    int index = 5;
    for (int i = 0; i < a; ++i) index += 4 - i;
    index += b - a - 1;
    return index;
}

static uint16_t permuted_mask(uint16_t mask, const array<int, 5>& permutation) {
    uint16_t result = 0;
    for (int a = 0; a < 5; ++a) {
        for (int b = a; b < 5; ++b) {
            if ((mask >> bit_index(permutation[a], permutation[b])) & 1U) {
                result |= static_cast<uint16_t>(1U << bit_index(a, b));
            }
        }
    }
    return result;
}

// Vertex-type names do not matter.  The smallest of all 5! renamings is the
// representative of an isomorphism class.
static uint16_t canonical_mask(uint16_t mask) {
    array<int, 5> permutation{0, 1, 2, 3, 4};
    uint16_t best = mask;
    do {
        best = min(best, permuted_mask(mask, permutation));
    } while (next_permutation(permutation.begin(), permutation.end()));
    return best;
}

struct Graph {
    int n = 0;
    vector<vector<unsigned char>> edge;
    string bits;
};

// Replace every one of the five vertex types by `copies` equal vertices.
// Repetition makes a 15-bit graph pattern survive edge-flip noise.
static Graph expand(uint16_t mask, int copies) {
    const int n = 5 * copies;
    Graph graph;
    graph.n = n;
    graph.edge.assign(n, vector<unsigned char>(n, 0));
    graph.bits.reserve(n * (n - 1) / 2);

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            const int type_i = i / copies;
            const int type_j = j / copies;
            const unsigned char value = static_cast<unsigned char>(
                (mask >> bit_index(type_i, type_j)) & 1U
            );
            graph.edge[i][j] = graph.edge[j][i] = value;
            graph.bits.push_back(value ? '1' : '0');
        }
    }
    return graph;
}

static Graph parse_graph(const string& bits, int n) {
    Graph graph;
    graph.n = n;
    graph.bits = bits;
    graph.edge.assign(n, vector<unsigned char>(n, 0));

    int position = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            const unsigned char value = static_cast<unsigned char>(bits[position++] == '1');
            graph.edge[i][j] = graph.edge[j][i] = value;
        }
    }
    return graph;
}

// Every value below is unchanged by a permutation of vertex numbers.
// That is essential because the judge shuffles the vertices.
static vector<double> make_features(const Graph& graph) {
    const int n = graph.n;
    const int pair_count = n * (n - 1) / 2;

    vector<int> degree(n, 0);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) degree[i] += graph.edge[i][j];
    }

    vector<double> sorted_degree(degree.begin(), degree.end());
    sort(sorted_degree.begin(), sorted_degree.end());

    vector<double> neighbor_average(n, 0.0);
    vector<double> signed_message(n, 0.0);
    const double mean_degree = accumulate(degree.begin(), degree.end(), 0.0) / n;
    for (int i = 0; i < n; ++i) {
        double neighbor_degree_sum = 0.0;
        double centered_sum = 0.0;
        for (int j = 0; j < n; ++j) {
            if (graph.edge[i][j]) neighbor_degree_sum += degree[j];
            if (i != j) {
                centered_sum += (graph.edge[i][j] ? 1.0 : -1.0)
                              * (degree[j] - mean_degree);
            }
        }
        if (degree[i] != 0) neighbor_average[i] = neighbor_degree_sum / degree[i];
        signed_message[i] = centered_sum / sqrt(static_cast<double>(n));
    }
    sort(neighbor_average.begin(), neighbor_average.end());
    sort(signed_message.begin(), signed_message.end());

    // Store each adjacency row in two 64-bit words.  N is at most 100.
    const array<uint64_t, 2> empty_row{0, 0};
    vector<array<uint64_t, 2>> rows(n, empty_row);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (graph.edge[i][j]) rows[i][j / 64] |= 1ULL << (j % 64);
        }
    }

    vector<int> common_neighbors;
    vector<int> row_differences;
    common_neighbors.reserve(pair_count);
    row_differences.reserve(pair_count);

    // The cumulative histograms keep the relation with whether (i,j) is an
    // edge.  They add useful detail when two graphs have the same degrees.
    vector<array<int, 2>> common_counts(n + 1, array<int, 2>{0, 0});
    vector<array<int, 2>> difference_counts(n + 1, array<int, 2>{0, 0});

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            int common = 0;
            int difference = 0;
            for (int block = 0; block < 2; ++block) {
                common += __builtin_popcountll(rows[i][block] & rows[j][block]);
                difference += __builtin_popcountll(rows[i][block] ^ rows[j][block]);
            }
            common_neighbors.push_back(common);
            row_differences.push_back(difference);

            const int edge_kind = graph.edge[i][j] ? 1 : 0;
            ++common_counts[common][edge_kind];
            ++difference_counts[difference][edge_kind];
        }
    }
    sort(common_neighbors.begin(), common_neighbors.end());
    sort(row_differences.begin(), row_differences.end());

    vector<double> feature;
    feature.reserve(3 * n + 2 * pair_count + 4 * (n + 1));
    for (double value : sorted_degree) feature.push_back(value);
    for (double value : neighbor_average) feature.push_back(value);
    for (double value : signed_message) feature.push_back(value);
    for (int value : common_neighbors) feature.push_back(value);
    for (int value : row_differences) feature.push_back(value);

    for (int kind = 0; kind < 2; ++kind) {
        int cumulative = 0;
        for (int value = 0; value <= n; ++value) {
            cumulative += common_counts[value][kind];
            feature.push_back(cumulative);
        }
    }
    for (int kind = 0; kind < 2; ++kind) {
        int cumulative = 0;
        for (int value = 0; value <= n; ++value) {
            cumulative += difference_counts[value][kind];
            feature.push_back(cumulative);
        }
    }
    return feature;
}

static Graph add_noise(const Graph& clean, double epsilon, Rng& rng) {
    Graph noisy;
    noisy.n = clean.n;
    noisy.edge.assign(noisy.n, vector<unsigned char>(noisy.n, 0));

    for (int i = 0; i < noisy.n; ++i) {
        for (int j = i + 1; j < noisy.n; ++j) {
            const unsigned char value = static_cast<unsigned char>(
                clean.edge[i][j] ^ rng.bernoulli(epsilon)
            );
            noisy.edge[i][j] = noisy.edge[j][i] = value;
        }
    }
    return noisy;
}

// Distance used only while choosing a well-spread set of clean codewords.
static double clean_distance(const vector<double>& a, const vector<double>& b, int n) {
    double result = 0.0;
    for (int i = 0; i < n; ++i) {
        const double difference = a[i] - b[i];
        result += difference * difference;
    }
    for (int i = n; i < 2 * n; ++i) {
        const double difference = a[i] - b[i];
        result += 0.35 * difference * difference;
    }
    for (int i = 2 * n; i < 3 * n; ++i) {
        const double difference = a[i] - b[i];
        result += 0.12 * difference * difference;
    }

    const int pair_count = n * (n - 1) / 2;
    for (int i = 3 * n; i < 3 * n + 2 * pair_count; ++i) {
        const double difference = a[i] - b[i];
        result += 0.05 * difference * difference;
    }
    return result;
}

static void solve_without_noise(int graph_count) {
    int n = 4;
    while (n * (n - 1) / 2 + 1 < graph_count) ++n;
    const int pair_count = n * (n - 1) / 2;

    cout << n << '\n';
    for (int index = 0; index < graph_count; ++index) {
        cout << string(index, '1') << string(pair_count - index, '0') << '\n';
    }
    cout.flush();

    // Vertex shuffling does not change the number of edges.
    for (int query = 0; query < 100; ++query) {
        string received;
        cin >> received;
        const int edge_count = static_cast<int>(count(received.begin(), received.end(), '1'));
        cout << min(edge_count, graph_count - 1) << '\n' << flush;
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int graph_count;
    double epsilon;
    if (!(cin >> graph_count >> epsilon)) return 0;

    if (epsilon == 0.0) {
        solve_without_noise(graph_count);
        return 0;
    }

    // More graph choices and more noise require more repetition.  The formula
    // was fitted only to the official generator's public seeds, then clamped to
    // the legal range N=15..100.
    const double signal = 1.0 - 2.0 * epsilon;
    const double estimated_copies = 0.49 * log2(static_cast<double>(graph_count))
                                  / (signal * signal);
    const int copies = clamp(static_cast<int>(lround(estimated_copies)), 3, 20);
    const int n = 5 * copies;
    const int pair_count = n * (n - 1) / 2;
    const int feature_count = 3 * n + 2 * pair_count + 4 * (n + 1);

    // Enumerating all 2^15 colored graphs and removing type renamings leaves
    // 544 candidates.  This is tiny enough to do at program start.
    vector<uint16_t> representatives;
    representatives.reserve(544);
    for (uint32_t mask = 0; mask < (1U << 15); ++mask) {
        const uint16_t small_graph = static_cast<uint16_t>(mask);
        if (canonical_mask(small_graph) == small_graph) {
            representatives.push_back(small_graph);
        }
    }

    vector<Graph> all_graphs;
    vector<vector<double>> all_clean_features;
    all_graphs.reserve(representatives.size());
    all_clean_features.reserve(representatives.size());
    for (uint16_t mask : representatives) {
        all_graphs.push_back(expand(mask, copies));
        all_clean_features.push_back(make_features(all_graphs.back()));
    }

    // Farthest-point sampling: repeatedly add the candidate whose nearest
    // already-selected codeword is as far away as possible.
    vector<int> selected{0};
    vector<unsigned char> used(representatives.size(), 0);
    vector<double> nearest(representatives.size(), numeric_limits<double>::infinity());
    used[0] = 1;

    while (static_cast<int>(selected.size()) < graph_count) {
        const int last = selected.back();
        int best_index = -1;
        double best_distance = -1.0;
        for (int candidate = 0; candidate < static_cast<int>(representatives.size()); ++candidate) {
            if (used[candidate]) continue;
            const double distance = clean_distance(
                all_clean_features[candidate], all_clean_features[last], n
            );
            nearest[candidate] = min(nearest[candidate], distance);
            if (nearest[candidate] > best_distance) {
                best_distance = nearest[candidate];
                best_index = candidate;
            }
        }
        selected.push_back(best_index);
        used[best_index] = 1;
    }

    vector<Graph> codewords;
    codewords.reserve(graph_count);
    for (int index : selected) codewords.push_back(move(all_graphs[index]));

    // Build the decoder's reference data inside the submitted program.  Since
    // the features ignore vertex numbers, simulating the judge's permutation is
    // unnecessary.  Mean and variance also correct the bias caused by sorting.
    constexpr int SAMPLE_COUNT = 60;
    Rng rng(0x16016a5eULL
            ^ (static_cast<uint64_t>(graph_count) << 20)
            ^ static_cast<uint64_t>(lround(epsilon * 100.0)));
    vector<vector<double>> means(
        graph_count, vector<double>(feature_count, 0.0)
    );
    vector<vector<double>> variances(
        graph_count, vector<double>(feature_count, 0.0)
    );

    for (int code = 0; code < graph_count; ++code) {
        for (int sample = 0; sample < SAMPLE_COUNT; ++sample) {
            const vector<double> feature = make_features(add_noise(codewords[code], epsilon, rng));
            for (int i = 0; i < feature_count; ++i) {
                means[code][i] += feature[i];
                variances[code][i] += feature[i] * feature[i];
            }
        }
        for (int i = 0; i < feature_count; ++i) {
            means[code][i] /= SAMPLE_COUNT;
            variances[code][i] = max(
                1.0,
                variances[code][i] / SAMPLE_COUNT - means[code][i] * means[code][i]
            );
        }
    }

    cout << n << '\n';
    for (const Graph& codeword : codewords) cout << codeword.bits << '\n';
    cout.flush();

    // Nearest noisy template, using a diagonal variance normalization.
    for (int query = 0; query < 100; ++query) {
        string received;
        cin >> received;
        const vector<double> observed = make_features(parse_graph(received, n));

        int answer = 0;
        double best_distance = numeric_limits<double>::infinity();
        for (int code = 0; code < graph_count; ++code) {
            double distance = 0.0;
            for (int i = 0; i < n; ++i) {
                const double difference = observed[i] - means[code][i];
                distance += difference * difference / (variances[code][i] + 2.0);
            }
            for (int i = n; i < 2 * n; ++i) {
                const double difference = observed[i] - means[code][i];
                distance += 0.30 * difference * difference / (variances[code][i] + 2.0);
            }
            for (int i = 2 * n; i < 3 * n; ++i) {
                const double difference = observed[i] - means[code][i];
                distance += 0.10 * difference * difference / (variances[code][i] + 4.0);
            }

            // Sorted common-neighbor counts are retained for codebook design,
            // but row differences were the more useful feature for decoding.
            for (int i = 3 * n + pair_count; i < 3 * n + 2 * pair_count; ++i) {
                const double difference = observed[i] - means[code][i];
                distance += 0.10 * difference * difference / (variances[code][i] + 2.0);
            }
            for (int i = 3 * n + 2 * pair_count; i < feature_count; ++i) {
                const double difference = observed[i] - means[code][i];
                distance += 0.02 * difference * difference / (variances[code][i] + 8.0);
            }

            if (distance < best_distance) {
                best_distance = distance;
                answer = code;
            }
        }
        cout << answer << '\n' << flush;
    }
}
