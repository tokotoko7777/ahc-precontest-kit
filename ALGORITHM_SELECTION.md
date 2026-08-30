# アルゴリズム早見表

同じ目的のパーツが複数ある場合は、入力の条件が一番狭く、その条件を確実に
満たしているものを選ぶと高速です。

## 最短距離

| 辺の条件 | 始点 | パーツ | 計算量 |
|---|---:|---|---:|
| 全辺のコストが同じ | 1個 | `graph-bfs.hpp` | `O(N + M)` |
| コストが0か1 | 1個 | `zero-one-bfs.hpp` | `O(N + M)` |
| コストが0以上 | 1個 | `dijkstra.hpp` | `O((N + M) log N)` |
| 負辺を含んでもよい、小さなグラフ | 全頂点間 | `floyd-warshall.hpp` | `O(N^3)` |
| 4方向の文字盤面 | 1マス | `grid-bfs.hpp` | `O(HW)` |

負の辺を含む大きなグラフ用の単一始点最短路は、現在まだ収録していません。
その条件でDijkstraを使ってはいけません。

## 連結性と順序

| 知りたいこと | パーツ | 計算量 |
|---|---|---:|
| 無向辺を追加しながら同じ成分か判定 | `dsu.hpp` | ほぼ `O(1)` / 回 |
| 無向グラフを最小コストで連結 | `kruskal.hpp` | `O(M log M)` |
| 有向グラフで互いに行き来できる集合 | `strongly-connected-components.hpp` | `O(N + M)` |
| 有向グラフを依存関係順に並べる | `topological-sort.hpp` | `O(N + M)` |

## 区間の値

| 更新 | 問い合わせ | パーツ | 計算量 |
|---|---|---|---:|
| なし | 区間和 | `cumulative-sum.hpp` | 構築 `O(N)`、質問 `O(1)` |
| 1点加算 | 区間和 | `fenwick-tree.hpp` | `O(log N)` |
| 1点代入 | 和・min・maxなど | `segment-tree.hpp` | `O(log N)` |
| 区間加算 | 区間和 | `range-add-range-sum.hpp` | `O(log N)` |
| 更新を全部先に処理 | 各点の値 | `difference-array.hpp` | 更新 `O(1)`、構築 `O(N)` |
| なし | min・max・gcd | `sparse-table.hpp` | 構築 `O(N log N)`、質問 `O(1)` |
| なし、幅が固定 | 全区間のmin・max | `sliding-window-minimum.hpp` | 全体 `O(N)` |

区間代入、区間minなど、別の種類のLazy Segment Treeは今後個別部品として追加します。

## 木

| 知りたいこと | パーツ | 計算量 |
|---|---|---:|
| 2頂点の共通祖先・距離・パス上の頂点 | `lowest-common-ancestor.hpp` | 構築 `O(N log N)`、質問 `O(log N)` |
| 辺を追加しながら連結性を管理 | `dsu.hpp` | ほぼ `O(1)` / 回 |

## 流量・対応付け・論理条件

| 問題の形 | パーツ | 主な計算量 |
|---|---|---:|
| 有向辺の容量を守って最大量を運ぶ | `max-flow.hpp` | Dinic法 `O(N^2 M)` |
| 決めた量を最小コストで運ぶ | `min-cost-flow.hpp` | `O(F M log N)` |
| 左集合と右集合から1対1の組を最大数選ぶ | `bipartite-matching.hpp` | `O(M sqrt(N))` |
| true/falseの「AまたはB」条件をすべて満たす | `two-sat.hpp` | `O(N + M)` |

二部グラフの単純な最大マッチングは、最大流でも解けますが
`bipartite-matching.hpp` の方がグラフを作りやすく高速です。
`min-cost-flow.hpp` に追加できるのはコスト0以上の辺だけで、同じインスタンスの
`flow` は1回だけ呼びます。

## 探索候補の保存

| 状況 | パーツ |
|---|---|
| Kが小さく、候補を1件ずつ追加 | `top-k.hpp` |
| 状態をコピーしても軽い | `simple-beam-search.hpp` |
| 状態コピーが重く、1手を正確に戻せる | `tree-beam-search.hpp` |
| 同じ状態へ何度も到達する | `best-by-key.hpp` または `step_with_key` |

高速版ほど使える条件が狭くなります。まず条件を確認し、迷う場合は単純なパーツから
始めてください。
