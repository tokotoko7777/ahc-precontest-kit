#pragma GCC optimize("O3,unroll-loops")
#include <bits/stdc++.h>
using namespace std;

// library/timer.hpp と同じ、経過時間を測るだけのタイマー。
struct Timer {
    chrono::steady_clock::time_point start = chrono::steady_clock::now();

    double elapsed_ms() const {
        return chrono::duration<double, milli>(
                   chrono::steady_clock::now() - start)
            .count();
    }
};

int cell_id(int row, int column, int n) {
    return row * n + column;
}

bool adjacent8(int a, int b, int n) {
    int ar = a / n;
    int ac = a % n;
    int br = b / n;
    int bc = b % n;
    return max(abs(ar - br), abs(ac - bc)) == 1;
}

long long tour_value(const vector<int>& path, const vector<int>& population) {
    long long value = 0;
    for (int day = 0; day < static_cast<int>(path.size()); ++day) {
        value += 1LL * day * population[path[day]];
    }
    return value;
}

// Nが偶数なら、全マスを通るHamilton閉路を簡単に作れる。
// path.back() と path.front() も隣接している。
vector<int> make_hamilton_cycle(int n, bool transpose) {
    vector<pair<int, int>> cells;
    cells.reserve(n * n);

    // 左端を下り、残りを縦に蛇行し、最後に上端を戻る。
    cells.push_back({0, 0});
    for (int row = 1; row < n; ++row) cells.push_back({row, 0});

    for (int column = 1; column < n; ++column) {
        if (column & 1) {
            for (int row = n - 1; row >= 1; --row) {
                cells.push_back({row, column});
            }
        } else {
            for (int row = 1; row < n; ++row) {
                cells.push_back({row, column});
            }
        }
    }
    for (int column = n - 1; column >= 1; --column) {
        cells.push_back({0, column});
    }

    vector<int> path;
    path.reserve(n * n);
    for (auto [row, column] : cells) {
        if (transpose) swap(row, column);
        path.push_back(cell_id(row, column, n));
    }
    return path;
}

// 4列ずつ組み合わせた、別の形のHamilton閉路。
// 離れた訪問日のセル同士を安全に交換できる場所が多い。
vector<int> make_four_column_cycle(int n) {
    if (n % 4 != 0) return make_hamilton_cycle(n, false);

    vector<int> path;
    path.reserve(n * n);

    for (int base = 0; base < n; base += 4) {
        path.push_back(cell_id(0, base, n));
        path.push_back(cell_id(0, base + 1, n));
        path.push_back(cell_id(1, base + 1, n));
        for (int row = 2; row < n; ++row) {
            path.push_back(cell_id(row, base, n));
        }
        path.push_back(cell_id(n - 1, base + 1, n));
        path.push_back(cell_id(n - 1, base + 2, n));
        path.push_back(cell_id(n - 1, base + 3, n));
        path.push_back(cell_id(n - 2, base + 3, n));
        for (int row = n - 3; row >= 0; --row) {
            path.push_back(cell_id(row, base + 2, n));
        }
        path.push_back(cell_id(0, base + 3, n));
    }

    for (int base = n - 1; base >= 3; base -= 4) {
        for (int row = 1; row <= n - 3; ++row) {
            path.push_back(cell_id(row, base, n));
        }
        path.push_back(cell_id(n - 2, base - 1, n));
        for (int row = n - 2; row >= 2; --row) {
            path.push_back(cell_id(row, base - 2, n));
        }
        path.push_back(cell_id(1, base - 3, n));
    }
    return path;
}

int rotate_cell(int v, int n, int rotation) {
    int row = v / n;
    int column = v % n;
    if (rotation == 0) return cell_id(row, column, n);
    if (rotation == 1) return cell_id(column, n - 1 - row, n);
    if (rotation == 2) return cell_id(n - 1 - row, n - 1 - column, n);
    return cell_id(n - 1 - column, row, n);
}

vector<int> rotate_cycle(const vector<int>& cycle, int n, int rotation) {
    vector<int> answer(cycle.size());
    for (int i = 0; i < static_cast<int>(cycle.size()); ++i) {
        answer[i] = rotate_cell(cycle[i], n, rotation);
    }
    return answer;
}

// 同じ閉路でも、どの辺を切って0日目にするかで得点が変わる。
// 全ての切り方をO(N^2)で調べ、最良の向き・開始位置を返す。
vector<int> best_cut_of_cycle(
    const vector<int>& cycle,
    const vector<int>& population
) {
    int m = static_cast<int>(cycle.size());
    long long total_population = 0;
    for (int value : population) total_population += value;

    long long answer_value = -1;
    int answer_start = 0;
    vector<int> answer_order;

    for (int direction = 0; direction < 2; ++direction) {
        vector<int> order = cycle;
        if (direction == 1) reverse(order.begin(), order.end());

        long long value = tour_value(order, population);
        long long direction_value = -1;
        int direction_start = 0;
        for (int start = 0; start < m; ++start) {
            if (value > direction_value) {
                direction_value = value;
                direction_start = start;
            }

            // startを1つ進めると、先頭要素だけが末尾へ移る。
            value += 1LL * m * population[order[start]] - total_population;
        }

        if (direction_value > answer_value) {
            answer_value = direction_value;
            answer_start = direction_start;
            answer_order = move(order);
        }
    }

    vector<int> answer(m);
    for (int k = 0; k < m; ++k) {
        answer[k] = answer_order[(answer_start + k) % m];
    }
    return answer;
}

struct NeighborTable {
    vector<array<int, 8>> to;
    vector<unsigned char> size;

    explicit NeighborTable(int n) : to(n * n), size(n * n, 0) {
        for (int row = 0; row < n; ++row) {
            for (int column = 0; column < n; ++column) {
                int v = cell_id(row, column, n);
                for (int dr = -1; dr <= 1; ++dr) {
                    for (int dc = -1; dc <= 1; ++dc) {
                        if (dr == 0 && dc == 0) continue;
                        int nr = row + dr;
                        int nc = column + dc;
                        if (0 <= nr && nr < n && 0 <= nc && nc < n) {
                            to[v][size[v]++] = cell_id(nr, nc, n);
                        }
                    }
                }
            }
        }
    }
};

// path[i] と path[j] を交換した後も、変化する辺が全部合法か調べる。
bool can_swap_cells(const vector<int>& path, int i, int j, int n) {
    if (i == j) return false;
    int m = static_cast<int>(path.size());
    int x = path[i];
    int y = path[j];

    auto after_swap = [&](int position) {
        if (position == i) return y;
        if (position == j) return x;
        return path[position];
    };

    int changed_edges[4] = {i - 1, i, j - 1, j};
    for (int a = 0; a < 4; ++a) {
        int edge = changed_edges[a];
        if (edge < 0 || edge + 1 >= m) continue;
        bool already_checked = false;
        for (int b = 0; b < a; ++b) {
            if (changed_edges[b] == edge) already_checked = true;
        }
        if (already_checked) continue;
        if (!adjacent8(after_swap(edge), after_swap(edge + 1), n)) {
            return false;
        }
    }
    return true;
}

void apply_template_switches(
    vector<int>& path,
    const vector<int>& population,
    int n,
    bool vertical_pairs
) {
    int m = static_cast<int>(path.size());
    vector<int> position(m);
    for (int i = 0; i < m; ++i) position[path[i]] = i;

    auto improve_pair = [&](int x, int y) {
        int i = position[x];
        int j = position[y];
        long long delta = 1LL * (j - i) *
                          (population[x] - population[y]);
        if (delta <= 0) return;

        // この経路テンプレートでは、対象ペアを全て選び終えた状態が合法。
        // 途中状態は使わず、一括した複合近傍として扱う。
        swap(path[i], path[j]);
        position[x] = j;
        position[y] = i;
    };

    if (!vertical_pairs) {
        for (int row = 3; row <= n - 5; ++row) {
            for (int column = 0; column + 1 < n; column += 2) {
                improve_pair(
                    cell_id(row, column, n),
                    cell_id(row, column + 1, n)
                );
            }
        }
    } else {
        for (int row = 0; row + 1 < n; row += 2) {
            for (int column = 3; column <= n - 5; ++column) {
                improve_pair(
                    cell_id(row, column, n),
                    cell_id(row + 1, column, n)
                );
            }
        }
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n;
    cin >> n;
    int m = n * n;
    vector<int> population(m);
    for (int& value : population) cin >> value;

    // 制約ではN=200。念のため4の倍数以外では合法な横蛇行を返す。
    if (n % 4 != 0) {
        for (int row = 0; row < n; ++row) {
            if (row % 2 == 0) {
                for (int column = 0; column < n; ++column) {
                    cout << row << ' ' << column << '\n';
                }
            } else {
                for (int column = n - 1; column >= 0; --column) {
                    cout << row << ' ' << column << '\n';
                }
            }
        }
        return 0;
    }

    NeighborTable neighbors(n);

    // 盤面上の向きと巡回方向を変えた8候補を比べる。
    // テンプレートが用意した独立な二択を全て決めてから、閉路を切る。
    vector<vector<int>> cycles;
    vector<int> four_columns = make_four_column_cycle(n);
    for (int rotation = 0; rotation < 4; ++rotation) {
        cycles.push_back(rotate_cycle(four_columns, n, rotation));
    }

    vector<int> path;
    long long current_value = -1;
    for (int rotation = 0; rotation < 4; ++rotation) {
        for (int direction = 0; direction < 2; ++direction) {
            vector<int> candidate = cycles[rotation];
            if (direction == 1) reverse(candidate.begin(), candidate.end());
            // 二択の決定と閉路の切り位置を交互に3回ずつ改善する。
            for (int iteration = 0; iteration < 3; ++iteration) {
                apply_template_switches(
                    candidate, population, n, rotation % 2 == 1
                );
                candidate = best_cut_of_cycle(candidate, population);
            }
            long long value = tour_value(candidate, population);
            if (value > current_value) {
                current_value = value;
                path.swap(candidate);
            }
        }
    }

    vector<int> position(m);
    for (int i = 0; i < m; ++i) position[path[i]] = i;

    vector<long long> prefix_population(m + 1);
    vector<long long> prefix_weighted(m + 1);

    auto rebuild_prefix = [&]() {
        prefix_population[0] = 0;
        prefix_weighted[0] = 0;
        for (int i = 0; i < m; ++i) {
            long long value = population[path[i]];
            prefix_population[i + 1] = prefix_population[i] + value;
            prefix_weighted[i + 1] =
                prefix_weighted[i] + 1LL * i * value;
        }
    };

    auto reverse_delta = [&](int left, int right) {
        long long sum =
            prefix_population[right + 1] - prefix_population[left];
        long long weighted =
            prefix_weighted[right + 1] - prefix_weighted[left];
        return 1LL * (left + right) * sum - 2LL * weighted;
    };

    // 2-opt: 2本の辺をつなぎ替え、間の区間を反転する。
    // 内部の辺は向きが逆になるだけなので、境界2辺だけ調べればよい。
    // 1回の採用ごとにprefixを作り直し、候補delta自体はO(1)で見る。
    Timer search_timer;
    int pass_id = 0;
    int swap_pass = 0;
    bool neighborhood_changed = true;
    while (neighborhood_changed && search_timer.elapsed_ms() < 1870.0) {
        neighborhood_changed = false;

        while (search_timer.elapsed_ms() < 1730.0) {
            rebuild_prefix();
            bool changed = false;
            int first_edge = (pass_id * 977) % (m - 2);
            ++pass_id;

            for (int trial = 0; trial < m - 2; ++trial) {
                if ((trial & 127) == 0 &&
                    search_timer.elapsed_ms() >= 1730.0) {
                    break;
                }

                int edge1 = (first_edge + trial) % (m - 2);
                int a = path[edge1];
                int b = path[edge1 + 1];

                long long best_delta = 0;
                int best_right = -1;

                for (int t = 0; t < neighbors.size[a]; ++t) {
                    int c = neighbors.to[a][t];
                    int edge2 = position[c];
                    if (edge2 <= edge1 + 1 || edge2 + 1 >= m) continue;
                    int d = path[edge2 + 1];
                    if (!adjacent8(b, d, n)) continue;

                    int left = edge1 + 1;
                    long long delta = reverse_delta(left, edge2);
                    if (delta > best_delta) {
                        best_delta = delta;
                        best_right = edge2;
                    }
                }

                if (best_right != -1) {
                    int left = edge1 + 1;
                    reverse(
                        path.begin() + left,
                        path.begin() + best_right + 1
                    );
                    for (int i = left; i <= best_right; ++i) {
                        position[path[i]] = i;
                    }
                    changed = true;
                    rebuild_prefix();
                }
            }

            if (!changed) break;
            neighborhood_changed = true;
        }

        // 2-optで動きにくくなった後は、交換で取りこぼしを拾う。
        while (search_timer.elapsed_ms() < 1870.0) {
            bool changed = false;
            int first_position = (swap_pass * 7919) % m;
            ++swap_pass;

            for (int trial = 0; trial < m; ++trial) {
                if ((trial & 255) == 0 &&
                    search_timer.elapsed_ms() >= 1870.0) {
                    break;
                }

                int i = (first_position + trial) % m;
                int anchor;
                int other_neighbor = -1;
                if (i == 0) {
                    anchor = path[1];
                } else if (i == m - 1) {
                    anchor = path[m - 2];
                } else {
                    anchor = path[i - 1];
                    other_neighbor = path[i + 1];
                }

                long long best_delta = 0;
                int best_j = -1;
                for (int t = 0; t < neighbors.size[anchor]; ++t) {
                    int y = neighbors.to[anchor][t];
                    if (other_neighbor != -1 &&
                        !adjacent8(y, other_neighbor, n)) {
                        continue;
                    }
                    int j = position[y];
                    if (!can_swap_cells(path, i, j, n)) continue;

                    int x = path[i];
                    long long delta = 1LL * (j - i) *
                                      (population[x] - population[y]);
                    if (delta > best_delta) {
                        best_delta = delta;
                        best_j = j;
                    }
                }

                if (best_j != -1) {
                    int x = path[i];
                    int y = path[best_j];
                    swap(path[i], path[best_j]);
                    position[x] = best_j;
                    position[y] = i;
                    changed = true;
                }
            }

            if (!changed) break;
            neighborhood_changed = true;
        }
    }

    for (int v : path) {
        cout << v / n << ' ' << v % n << '\n';
    }
    return 0;
}
