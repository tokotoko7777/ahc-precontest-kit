# ahc-precontest-kit

AHC を中心とした競技プログラミングで、自分の `main.cpp` へ直接コピーして使う
C++ パーツ集です。ヒューリスティック探索だけでなく、グラフ・データ構造などの
通常アルゴリズムも同じ形で揃えます。

## 使い方

コンテスト中に編集するファイルは `main.cpp` だけです。

1. 下の一覧から必要なパーツを開く。
2. ファイル全体をコピーする。
3. 自分の `main.cpp` の `main` 関数より上へ貼り付ける。
4. そのまま `main.cpp` だけをコンパイル・提出する。

ローカルファイルを参照する手順、生成スクリプト、実行時ファイルは必要ありません。
各パーツは標準ライブラリの依存も含め、ファイル全体を単独で貼れる形にします。
初心者がそのまま読めるように、カスタム `namespace` や複雑な共通基盤は使わず、
原則として 1 ファイルを 1 機能だけにします。

考え方は [Luzhiled's Library](https://ei1333.github.io/library/) のような、必要な実装を
探して自分のコードへ取り込める競技プログラミング用ライブラリを参考にしています。

## 探索は手法別のフォルダから選ぶ

| フォルダ | 個別に置いている形式 |
|---|---|
| [ビーム](template/search/beam/README.md) | 通常・差分評価・木上・世代飛ばし木上・chokudai・枝刈り設定・多点スタート |
| [局所探索（焼きなまし・山登り）](template/search/local-search/README.md) | 共通の基本形・部分破壊再構築・近傍選択・反復局所探索・途中再生・前後DP |
| [モンテカルロ](template/search/monte-carlo/README.md) | 共通未来rollout・木探索（UCT） |

フォルダ内のcppを1つmain.cppにコピーし、TODOを埋めます。
書くのは状態・候補や近傍・評価・更新・入出力です。
焼きなましと山登りは[basic.cpp](template/search/local-search/basic.cpp)にまとめ、
USE_ANNEALINGで採否だけを切り替えます。

LNSは独立した大分類ではなく、局所探索内の「部分破壊・再構築」として置いています。
[destroy-repair.cpp](template/search/local-search/destroy-repair.cpp)は山登りが初期設定で、
焼きなましにも切り替えられます。各方式のバリエーションは削らず、同じフォルダに並べます。
乱数を使わない[決定的な先読み](template/advanced/README.md)は別に残しています。
既存のhpp・実問題例・計測履歴は維持しています。

[短いガイド](SEARCH_GUIDE.md) / [実問題の完成例](examples/search/README.md) /
[詳細API](SEARCH_REFERENCE.md)。
時間・乱数などの補助部品は下の一覧から必要なものだけ選びます。
短い使用例は各hppの先頭、長い使用例は[USAGE.md](USAGE.md)にあります。

コンテスト前に公開版を固定し、commit固定URL付きでコピーする方法と
オフラインbundleの作り方は [`PRECONTEST.md`](PRECONTEST.md) にあります。
全`library/*.hpp`にも、手動コピー時に出典を落とさないための公開URLを
標準`#include`群の直後へ記載しています。
自作した差分評価や`apply/revert`を全再計算と照合する例は
[`examples/debug/`](examples/debug/README.md) にあります。

## 高速化の方針

探索回数へ直結するため、パーツ追加時は計算量だけでなく、全ソート、動的確保、
間接関数呼び出し、時計取得、メモリ配置も確認します。制約が厳しい高速版は、
使える条件をファイル先頭へ明記します。詳しい判断基準は
[`PERFORMANCE.md`](PERFORMANCE.md) にまとめています。

ビームサーチは`make benchmark-search`で、AHC032「Mod Stamp」相当の固定5ケースを
実際に解き、問題本来の得点を幅1・100・1000・3000・10000で比較できます。
ケース数と幅は`./build/ahc032_score_benchmark 30 10000`のように変更できます。
合成データの速度だけを測る旧ベンチマークは`make benchmark-search-speed`です。
焼きなましはAHC001、Monte CarloはAHC015、apply/revert木上ビームはAHC021を
実際に解くベンチマークもあります。まとめと測定値は
[`REAL_PROBLEM_BENCHMARKS.md`](REAL_PROBLEM_BENCHMARKS.md)、一括実行は
`make benchmark-real-search`です。

## ビームの選び方

| 条件 | パーツ |
|---|---|
| まず使う通常版。状態コピーが軽い | [simple-beam-search.hpp](library/simple-beam-search.hpp) |
| Actionから子の順位を差分計算できる | [action-beam-search.hpp](library/action-beam-search.hpp) |
| 状態コピーが重く、apply/revertを書ける | [tree-beam-search.hpp](library/tree-beam-search.hpp) |
| 行動ごとに到着世代が飛ぶ | [cost-tree-beam-search.hpp](library/cost-tree-beam-search.hpp) |

評価値はビーム内の順位で、焼きなましの「改善量」とは違います。
詳しい戻り値・任意の重複除去や枝刈りは[リファレンス](SEARCH_REFERENCE.md)へ。
基本フォーマットでは、未使用の高速化用関数を埋める必要はありません。

## パーツ一覧

### 時間・乱数・小道具

| ファイル | できること |
|---|---|
| [`timer.hpp`](library/timer.hpp) | 経過時間・残り時間・進捗率 |
| [`batched-timer.hpp`](library/batched-timer.hpp) | 時計を見る回数を間引くタイマー |
| [`scope-profiler.hpp`](library/scope-profiler.hpp) | 処理別の回数・時間。指定したビルドだけ有効、通常は時計取得・出力なし |
| [`random.hpp`](library/random.hpp) | 型を選べる乱数、ランダム選択、重み付き選択 |
| [`alias-table.hpp`](library/alias-table.hpp) | 固定重み分布から前計算後O(1)で抽選 |
| [`fast-io.hpp`](library/fast-io.hpp) | 大量の整数・文字列用のバッファ入出力 |
| [`chmin-chmax.hpp`](library/chmin-chmax.hpp) | 値が良くなる時だけ更新 |
| [`binary-search-answer.hpp`](library/binary-search-answer.hpp) | 単調な条件の整数・実数境界を二分探索 |
| [`schedule.hpp`](library/schedule.hpp) | 時間経過に合わせて値を変化 |
| [`best-keeper.hpp`](library/best-keeper.hpp) | best score と best state を保存 |
| [`top-k.hpp`](library/top-k.hpp) | 良い候補を上位 K 個だけ保存 |
| [`move-statistics.hpp`](library/move-statistics.hpp) | 近傍ごとの採用率・改善率を集計 |
| [`adaptive-operator-selector.hpp`](library/adaptive-operator-selector.hpp) | 複数の近傍を成果に応じて選ぶ。学習OFF・探索確率下限付き |
| [`route-utils.hpp`](library/route-utils.hpp) | 経路長と挿入・削除・移動・交換・区間反転の距離差分 |
| [`ordered-pair-insertion.hpp`](library/ordered-pair-insertion.hpp) | 先行制約のある2点の最良挿入位置をO(n)、追加メモリO(1)で探索 |
| [`debug-state-check.hpp`](library/debug-state-check.hpp) | 差分検査失敗時のseed・Move列・相違項目を記録 |

### 基本の探索

焼きなまし・山登りは同じhpp、ビームは用途別、モンテカルロは共通未来で比較する版から始めます。

| ファイル | できること |
|---|---|
| [`time-based-simulated-annealing.hpp`](library/time-based-simulated-annealing.hpp) | 焼きなまし・山登り。状態・近傍・差分評価・更新は共通 |
| [`simple-beam-search.hpp`](library/simple-beam-search.hpp) | 状態をコピーする初心者向けビームサーチ |
| [`action-beam-search.hpp`](library/action-beam-search.hpp) | Actionを先に上位N件へ絞るビーム。問題依存部分をまとめるRunner付き |
| [`tree-beam-search.hpp`](library/tree-beam-search.hpp) | 1手1世代のapply / revert型ビームサーチ。問題分離Runner付き |
| [`cost-tree-beam-search.hpp`](library/cost-tree-beam-search.hpp) | 1手の進み幅が異なるapply / revert型ビームサーチ |
| [`common-scenario-average.hpp`](library/common-scenario-average.hpp) | 全候補を同じ未来sampleで比較するrollout。問題分離Runner付き |

<details>
<summary>各方式のバリエーションを支える部品・専用Runner</summary>

以下は各方式を組み立てる補助部品や専用Runnerです。
対応する形式は[手法別フォルダ](template/search/README.md)で選べます。必要なものだけ使ってください。

| ファイル | できること |
|---|---|
| [`simulated-annealing.hpp`](library/simulated-annealing.hpp) | 外部から進捗率を渡す焼きなまし |
| [`prefix-replay.hpp`](library/prefix-replay.hpp) | 行動列の変更部分からだけ再計算。仮評価と採用を分けるcheckpoint cache |
| [`multi-start.hpp`](library/multi-start.hpp) | 回数・時間指定の多点スタート。共有締切・未完成試行の破棄・合法fallback付きAPIも選択可 |
| [`large-neighborhood-search.hpp`](library/large-neighborhood-search.hpp) | 部分破壊・再構築。山登り/RRT/SA、閾値打ち切り、近傍への成果通知 |
| [`iterated-local-search.hpp`](library/iterated-local-search.hpp) | 大きな摂動＋小さい局所探索を繰り返すILS。共有締切・最良解への再開 |
| [`chokudai-search.hpp`](library/chokudai-search.hpp) | 深さ別に候補を残して巡回する時間制限型探索。Action先行評価・容量上限 |
| [`monte-carlo-tree-search.hpp`](library/monte-carlo-tree-search.hpp) | 確率的な結果ごとの枝を保持するUCT。試行配分・訪問回数・報酬更新 |
| [`deterministic-rollout.hpp`](library/deterministic-rollout.hpp) | 全候補を決定的方策で仮実行して比較するrollout。問題分離Runner付き |

</details>

### 探索状態・候補管理

| ファイル | できること |
|---|---|
| [`shared-history.hpp`](library/shared-history.hpp) | 操作履歴の共通部分を親番号で共有・復元 |
| [`best-by-key.hpp`](library/best-by-key.hpp) | 同じ状態キーの候補を一番良いものだけにする |
| [`zobrist-hash.hpp`](library/zobrist-hash.hpp) | 配列状態のhashを差分更新 |
| [`fixed-vector.hpp`](library/fixed-vector.hpp) | allocationなしの固定上限vector |
| [`radix-heap.hpp`](library/radix-heap.hpp) | 単調な非負整数キー用の高速priority queue |
| [`farthest-point-sampling.hpp`](library/farthest-point-sampling.hpp) | 離れた代表点をO(NK)で選ぶ |
| [`greedy-balanced-partition.hpp`](library/greedy-balanced-partition.hpp) | 大きい要素から合計の軽い組へ分ける |

### データ構造

| ファイル | できること |
|---|---|
| [`cumulative-sum.hpp`](library/cumulative-sum.hpp) | 1次元累積和 |
| [`cumulative-sum-2d.hpp`](library/cumulative-sum-2d.hpp) | 2次元累積和 |
| [`difference-array.hpp`](library/difference-array.hpp) | 更新を全部先に処理する区間加算 |
| [`difference-array-2d.hpp`](library/difference-array-2d.hpp) | 矩形加算後に盤面を作る2次元いもす法 |
| [`fenwick-tree.hpp`](library/fenwick-tree.hpp) | 1点加算と区間和 |
| [`segment-tree.hpp`](library/segment-tree.hpp) | 1点変更と区間の和・最小値・最大値など |
| [`range-add-range-sum.hpp`](library/range-add-range-sum.hpp) | 区間加算と区間和 |
| [`range-add-range-minimum.hpp`](library/range-add-range-minimum.hpp) | 区間加算と区間最小値 |
| [`range-add-range-maximum.hpp`](library/range-add-range-maximum.hpp) | 区間加算と区間最大値 |
| [`range-assign-range-sum.hpp`](library/range-assign-range-sum.hpp) | 区間代入と区間和 |
| [`sparse-table.hpp`](library/sparse-table.hpp) | 静的配列の区間min・max・gcdをO(1)取得 |
| [`sliding-window-minimum.hpp`](library/sliding-window-minimum.hpp) | 固定幅区間のmin・maxを全体O(N)計算 |
| [`flat-grid.hpp`](library/flat-grid.hpp) | 連続メモリに置くキャッシュ効率重視の2次元配列 |
| [`coordinate-compression.hpp`](library/coordinate-compression.hpp) | 座標圧縮 |
| [`dsu.hpp`](library/dsu.hpp) | Union-Find。連結成分数とグループ一覧も取得可能 |
| [`weighted-dsu.hpp`](library/weighted-dsu.hpp) | 頂点間の差分制約を管理する重み付きUnion-Find |
| [`rollback-array.hpp`](library/rollback-array.hpp) | 変更を過去へ戻す配列。採用済み履歴の破棄にも対応 |
| [`rollback-dsu.hpp`](library/rollback-dsu.hpp) | 過去の状態へ戻せる Union-Find |
| [`stamp-array.hpp`](library/stamp-array.hpp) | ほぼ O(1) で初期化し直せる配列 |
| [`dense-int-set.hpp`](library/dense-int-set.hpp) | 固定範囲の整数集合。追加・削除・clearがO(1) |
| [`binary-trie.hpp`](library/binary-trie.hpp) | 非負整数集合の最小・最大XOR要素 |
| [`xor-basis.hpp`](library/xor-basis.hpp) | 部分集合XORの表現可能性・最小・最大値 |
| [`axis-aligned-rectangle.hpp`](library/axis-aligned-rectangle.hpp) | 半開矩形の面積・点包含・重なり判定 |
| [`largest-empty-rectangle.hpp`](library/largest-empty-rectangle.hpp) | 指定セルを含む最大空き長方形を列挙ベースで厳密に求める |
| [`interval-union.hpp`](library/interval-union.hpp) | 半開区間の併合・被覆長・2集合の対称差長 |

### グラフ・グリッド

| ファイル | できること |
|---|---|
| [`dijkstra.hpp`](library/dijkstra.hpp) | 非負辺グラフの最短距離と経路復元 |
| [`bellman-ford.hpp`](library/bellman-ford.hpp) | 負辺を含む最短距離・負閉路の影響範囲 |
| [`graph-bfs.hpp`](library/graph-bfs.hpp) | 重みなしグラフの最短距離と経路復元 |
| [`all-pairs-bfs.hpp`](library/all-pairs-bfs.hpp) | 重みなし全頂点間距離。距離型でメモリを調整 |
| [`zero-one-bfs.hpp`](library/zero-one-bfs.hpp) | コスト0/1の最短距離と経路復元 |
| [`grid-bfs.hpp`](library/grid-bfs.hpp) | 4方向グリッドの最短距離と経路復元 |
| [`topological-sort.hpp`](library/topological-sort.hpp) | DAGの順序と閉路検出 |
| [`bipartite-check.hpp`](library/bipartite-check.hpp) | 無向グラフの2色塗り・二部グラフ判定 |
| [`strongly-connected-components.hpp`](library/strongly-connected-components.hpp) | 有向グラフの強連結成分分解 |
| [`bridge-tree.hpp`](library/bridge-tree.hpp) | 無向グラフの橋と、橋を渡らず行き来できる成分 |
| [`functional-graph.hpp`](library/functional-graph.hpp) | 出辺1本のグラフの周期・入口・大きな回数の遷移 |
| [`floyd-warshall.hpp`](library/floyd-warshall.hpp) | 全頂点間最短距離と負閉路検出 |
| [`kruskal.hpp`](library/kruskal.hpp) | 最小全域木・最小全域森 |
| [`lowest-common-ancestor.hpp`](library/lowest-common-ancestor.hpp) | 木のLCA・距離・パス上の頂点 |
| [`tree-diameter.hpp`](library/tree-diameter.hpp) | 非負重みの木の直径と経路復元 |
| [`max-flow.hpp`](library/max-flow.hpp) | Dinic法の最大流・最小カット |
| [`min-cost-flow.hpp`](library/min-cost-flow.hpp) | 非負辺コスト用の最小費用流 |
| [`bipartite-matching.hpp`](library/bipartite-matching.hpp) | 左右の頂点を1対1対応させる最大マッチング |
| [`non-crossing-matching.hpp`](library/non-crossing-matching.hpp) | 左右の順序を保つ、交差しない重み最大の組選び |
| [`hungarian.hpp`](library/hungarian.hpp) | 費用最小の1対1割り当て |
| [`two-sat.hpp`](library/two-sat.hpp) | 「AまたはB」の論理条件を満たす割り当て |

### 数学

| ファイル | できること |
|---|---|
| [`static-mod-int.hpp`](library/static-mod-int.hpp) | コンパイル時modの四則演算・累乗・逆元 |
| [`prime-table.hpp`](library/prime-table.hpp) | 線形篩・素数判定・素因数分解・約数列挙 |
| [`mod-combination.hpp`](library/mod-combination.hpp) | 素数mod上のnCk・nPk・重複組合せ |
| [`extended-gcd.hpp`](library/extended-gcd.hpp) | 拡張Euclid互除法・mod逆元 |
| [`floor-sum.hpp`](library/floor-sum.hpp) | floorを含む等差数列の和 |
| [`integer-square-root.hpp`](library/integer-square-root.hpp) | 整数平方根の切り下げ・切り上げ |
| [`matrix.hpp`](library/matrix.hpp) | 連続メモリ行列の加算・乗算・累乗 |

### 文字列・列

| ファイル | できること |
|---|---|
| [`rolling-hash.hpp`](library/rolling-hash.hpp) | 部分列hash・連結・LCP |
| [`aho-corasick.hpp`](library/aho-corasick.hpp) | 整数アルファベットの複数パターン照合。重複・包含・suffixの一致IDを列挙 |
| [`sequence-overlap.hpp`](library/sequence-overlap.hpp) | 2列を末尾・先頭で最大限重ねて連結 |
| [`z-algorithm.hpp`](library/z-algorithm.hpp) | 各位置と先頭の最長共通接頭辞 |
| [`prefix-function.hpp`](library/prefix-function.hpp) | KMP用prefix function・パターン出現位置 |
| [`manacher.hpp`](library/manacher.hpp) | 全中心の最長回文を全体O(N)で計算 |

### 列・DP

| ファイル | できること |
|---|---|
| [`longest-increasing-subsequence.hpp`](library/longest-increasing-subsequence.hpp) | 最長増加部分列の長さと復元 |
| [`inversion-count.hpp`](library/inversion-count.hpp) | 大小関係が逆になった組の個数 |

### 確率DP

| ファイル | できること |
|---|---|
| [`probability-move-dp.hpp`](library/probability-move-dp.hpp) | 成功時に遷移、失敗時に停止する状態確率を1手更新 |
| [`forward-backward-dp.hpp`](library/forward-backward-dp.hpp) | 前後DPで固定長操作列の1操作変更を評価。採用時だけ前後の影響範囲を更新 |

### 幾何

| ファイル | できること |
|---|---|
| [`point-2d.hpp`](library/point-2d.hpp) | 2次元点・内積・外積・距離・向き |
| [`convex-hull.hpp`](library/convex-hull.hpp) | 整数座標の凸包を反時計回りで列挙 |
| [`segment-intersection.hpp`](library/segment-intersection.hpp) | 整数座標の線分交差・線分上判定 |
| [`hilbert-order.hpp`](library/hilbert-order.hpp) | 近い2次元点を近くへ並べやすいHilbert順 |

`int` 固定である必要がないパーツはテンプレートにしています。たとえば、次のように
得点は `double`、解は `vector<int>` のように自由に選べます。

```cpp
BestKeeper<double, vector<int>> best(initial_score, initial_answer);
CumulativeSum<long long> sum(values);
RollbackArray<string> names(initial_names);
```

各 `.hpp` は別のライブラリファイルを参照しません。必要なファイルだけを丸ごと
コピーできます。貼り付け後はカスタム名前空間を付けず、そのまま使えます。

GitHub 上ではファイルを開き、右上のコピーアイコン、または Raw 表示から全体を
コピーしてください。

## 参考資料

- [asi1024/MarathonLibrary](https://github.com/asi1024/MarathonLibrary) —
  前計算、候補生成前の足切り、処理別計測、条件付きqueueを調査。
  採用した機能・既存との重複・今後の候補を
  [調査メモ](MARATHON_LIBRARY_REVIEW.md)へ分けて記録しています。
- [木上のビームサーチ：高速化編](https://trap.jp/post/2920/) —
  apply / revert、履歴共有、状態コピー削減という考え方を参考にしています。
- [AtCoder Heuristic Contest Memo: Beam Search](https://jetbead.github.io/AtCoderHeuristicContestMemo/Library/beam_search.html) —
  候補を先に選んでから状態化する方法、上位N件のcutoff、多様性、
  重複除去、可変幅を監査項目として参考にしています。
- [ビームサーチ用C++テンプレート](https://jetbead.github.io/AtCoderHeuristicContestMemo/Library/cpp_lib/beam_cpp.html) —
  問題ごとに書き換える区間と探索処理を分ける構成を参考にし、
  このkitでは問題依存部分を`Problem` structへ集約しています。
- [上位N個を選ぶ処理の速度比較](https://zenn.dev/siman/articles/e94f63246f6cb3) —
  2N件ごとにN件へ縮め、既知の境界以下を保存しないvector方式を参考にしています。
- [heuristic-library-rs](https://github.com/e1jirou/heuristic-library-rs) —
  ヒューリスティックに限定しない分類と、1機能ずつ取り出せるAPIの粒度を
  参考にしています。コードの移植ではなく、このリポジトリ向けにC++で新規実装します。
- [TERRYのAHC練習問題まとめ](https://www.terry-u16.net/entry/ahc-practice-problem) —
  焼きなまし・ビームサーチを小問題から練習する順序を参考にしています。
- [AHC001参加記](https://blog.terry-u16.net/entry/ahc001)・
  [AHC039解説](https://blog.terry-u16.net/entry/ahc039) —
  固定長領域、差分評価、多点スタート、粗密を変える探索の実例を確認しています。
- [AC Library Documentation](https://atcoder.github.io/ac-library/production/document_ja/) —
  計算量、境界条件、型をコンパイル時に確定する汎用データ構造の設計を確認しています。
- [Introduction to Heuristics Contest A の実装記録](https://ruthen.hatenablog.com/entry/2024/01/25/000000) —
  差分計算、時計取得の間引き、理論計算量と定数倍の両方を見る実例を参考にしています。
- 手元のAHC001〜AHC068優勝コードレビュー知識から、複数問題で再利用例が
  確認できた時間管理・候補制限・重複除去・差分更新を選んでいます。

各ビームサーチは参考記事のコード移植ではありません。
生き残った履歴木だけをDFSし、`apply / revert`で状態を1個だけ管理する
木上2種に加え、候補Actionだけを2N件ずつ選抜して採用N件のみState化する
`action-beam-search.hpp`も、このkit向けの独自C++実装です。

## 過去AHCでの実戦例

全問題を網羅するのではなく、手法別の重点問題でフォーマットの性能を磨きます。
現在の対象と採用基準は[`practice/README.md`](practice/README.md)、
以下の全問題一覧と[`practice/PROGRESS.md`](practice/PROGRESS.md)は既存資産の記録です。

| 問題 | 主に使うパーツ |
|---|---|
| [`practice/ahc001`](practice/ahc001/) | 領域再構築SA、受理閾値、最大空き長方形、1位との得点差レポート |
| [`practice/ahc002`](practice/ahc002/) | 焼きなまし、多点スタート、destroy/repair |
| [`practice/ahc003`](practice/ahc003/) | オンライン辺重み推定、不確実性付きDijkstra |
| [`practice/ahc004`](practice/ahc004/) | 共通SA、Aho-Corasickの行・列差分評価、3bit配置比較 |
| [`practice/ahc005`](practice/ahc005/) | 共通SA、代表点と巡回順の同時探索、境界辺の差分評価 |
| [`practice/ahc006`](practice/ahc006/) | 焼きなまし、間引きタイマー、経路距離差分 |
| [`practice/ahc007`](practice/ahc007/) | Union-Find、未来辺によるオンライン連結判断 |
| [`practice/ahc008`](practice/ahc008/) | 対話型の安全判定、分担壁建設、段階閉鎖 |
| [`practice/ahc009`](practice/ahc009/) | 確率伝播DP、beam、可変長SA、前後DP |
| [`practice/ahc010`](practice/ahc010/) | 閉路DFS、bitset衝突判定、安全fallback |
| [`practice/ahc011`](practice/ahc011/) | Zobrist重複除去、親履歴付きbeam、短列探索 |
| [`practice/ahc012`](practice/ahc012/) | 分位格子、histogram評価、境界relocate |
| [`practice/ahc013`](practice/ahc013/) | 同種見通し移動、DSU、交差しない貪欲配線 |
| [`practice/ahc014`](practice/ahc014/) | 単位辺占有、候補制限、randomized multi-start |
| [`practice/ahc015`](practice/ahc015/) | 共通乱数rollout、固定長盤面、連結成分評価 |
| [`practice/ahc016`](practice/ahc016/) | 冗長グラフ符号、置換不変特徴、自己生成noise校正 |
| [`practice/ahc017`](practice/ahc017/) | farthest-point sample、日別Dijkstra cache、swap SA |
| [`practice/ahc018`](practice/ahc018/) | 硬さ推定、複数始点Dijkstra、rolling replan |
| [`practice/ahc019`](practice/ahc019/) | 24回転voxel重合、投影bitmask、残余domino化 |
| [`practice/ahc020`](practice/ahc020/) | 重み付きset cover、metric MST、非terminal葉刈り |
| [`practice/ahc021`](practice/ahc021/) | 前線確定型の木上ビーム、RadixHeap、必要な最短路だけ計算、正式scoreで完成候補を選択 |
| [`practice/ahc022`](practice/ahc022/) | 2値温度符号、active measurement、Hungarian復号 |
| [`practice/ahc023`](practice/ahc023/) | 永続通路、区間min-cost flow、区間彩色 |
| [`practice/ahc024`](practice/ahc024/) | 接触辺数差分、局所連結判定、短時間multi-start |
| [`practice/ahc025`](practice/ahc025/) | 順位推定、指数分布prior、保証付き均等化 |
| [`practice/ahc026`](practice/ahc026/) | 単独退避、完走rollout、箱位置の差分更新 |
| [`practice/ahc027`](practice/ahc027/) | 全点間BFS、汚れの緊急度、複数周期の再評価 |
| [`practice/ahc028`](practice/ahc028/) | 文字列overlap、盤面位置DP、挿入位置の再評価 |
| [`practice/ahc029`](practice/ahc029/) | 全カード×案件評価、投資・購入閾値、対話fallback |
| [`practice/ahc030`](practice/ahc030/) | 油田配置仮説bitset、情報量query、確定セル掘削 |
| [`practice/ahc031`](practice/ahc031/) | 共通帯DP、guillotine配置、壁区間の対称差 |
| [`practice/ahc032`](practice/ahc032/) | Action先行ビーム幅9,000、可換stamp列挙、確定セル評価。同じ幅での高速化と幅拡大の得点差を公式入力で分離検証 |
| [`practice/ahc033`](practice/ahc033/) | 入口退避buffer、搬出順制御、安全な単一大型crane |
| [`practice/ahc034`](practice/ahc034/) | 循環蛇行路、積載量分割、区間操作SA |
| [`practice/ahc035`](practice/ahc035/) | 交配期待値、成分極値保存、盤面swap SA |
| [`practice/ahc036`](practice/ahc036/) | 連結都市群、信号優先経路、再利用window辞書 |
| [`practice/ahc037`](practice/ahc037/) | 単調Steiner点、階層merge、局所付け替え |
| [`practice/ahc038`](practice/ahc038/) | 長さ違いの星型arm、同時回転、長さ1 fallback |
| [`practice/ahc039`](practice/ahc039/) | x単調直交多角形、多解像度境界SA、転置探索 |
| [`practice/ahc040`](practice/ahc040/) | Gaussian寸法推定、Kalman更新、bottom-left packing |
| [`practice/ahc041`](practice/ahc041/) | 深さ制限DFS、価値順構築、部分木reparent |
| [`practice/ahc042`](practice/ahc042/) | 福を守るshift、まとめ押し、盤面再生SA |
| [`practice/ahc043`](practice/ahc043/) | 駅候補制限、複数始点BFS、資金推移の正確な再生 |
| [`practice/ahc044`](practice/ahc044/) | rotor-router、流量割当、長期simulation座標降下 |
| [`practice/ahc045`](practice/ahc045/) | 容量付きk-d分割、分割MST query、都市交換 |
| [`practice/ahc046`](practice/ahc046/) | 停止岩候補、bitset重複除去、幅制限beam |
| [`practice/ahc047`](practice/ahc047/) | 12状態Markovモデル、確率近似SA、KMP厳密評価 |
| [`practice/ahc048`](practice/ahc048/) | 離散混色recipe、色cluster、残色reservoir再利用 |
| [`practice/ahc049`](practice/ahc049/) | 耐久保証付き複数箱搬送、経路交換・統合改善 |
| [`practice/ahc050`](practice/ahc050/) | 厳密確率伝播、同率最小riskのmulti-start |
| [`practice/ahc051`](practice/ahc051/) | 平面二分木、確率分類、装置・分別器の局所改善 |
| [`practice/ahc052`](practice/ahc052/) | 多始点BFS、共通ボタンlookahead、操作列短縮 |
| [`practice/ahc053`](practice/ahc053/) | 共有2進カード、禁止bit桁DP、不足修理 |
| [`practice/ahc054`](practice/ahc054/) | 花周辺guard、BFS通路、視界を狭める安全配置 |
| [`practice/ahc055`](practice/ahc055/) | 依存順序、攻撃先のO(1)差分焼きなまし |
| [`practice/ahc056`](practice/ahc056/) | BFS経路、時刻を色×状態へ平方根分割 |
| [`practice/ahc057`](practice/ahc057/) | 容量付き時空間cluster、時刻別MST、群間swap |
| [`practice/ahc058`](practice/ahc058/) | 先読み初期解＋購入順序SA、prefix cache、待機区間の一括更新。公式100ケースで平均約6.24%改善 |
| [`practice/ahc059`](practice/ahc059/) | 部分破壊・再構築（LNS、SA/RRT切替）、O(n)ペア再挿入、閾値打ち切り |
| [`practice/ahc060`](practice/ahc060/) | 色固定化、非逆走BFS、未登録文字列の最短配送 |
| [`practice/ahc061`](practice/ahc061/) | 共通シナリオRunner、粒子推定、3手rollout、独自乱数・同点処理 |
| [`practice/ahc062`](practice/ahc062/) | Hamilton閉路、prefix差分2-opt、合法swap |
| [`practice/ahc063`](practice/ahc063/) | リングバッファ蛇状態、bitset、層別beam |
| [`practice/ahc064`](practice/ahc064/) | ブロック移送、差分評価、非交差DP付きbeam |
| [`practice/ahc065`](practice/ahc065/) | Hamilton主ベルト、局所swap、円環順序対応 |
| [`practice/ahc066`](practice/ahc066/) | 向き付きBFS、運搬順序とmacroの交互探索 |
| [`practice/ahc067`](practice/ahc067/) | 橋の木、10ビットカウンタ、状態付きBFS |
| [`practice/ahc068`](practice/ahc068/) | 境界peel、長方形swap、正確な操作再生 |
| [`practice/ahc069`](practice/ahc069/) | compact配置、受理価格、限定的な再配置 |
| [`practice/ahc071`](practice/ahc071/) | 行DP、Actionビーム、費用閾値枝刈り、区間LNS |

各フォルダの `main.cpp` はローカルヘッダを参照しない、提出可能な単一ファイルです。

## 最小テンプレート

[`template/main.cpp`](template/main.cpp) は、パーツを貼る位置だけを示す最小構成です。
テンプレートにもローカルファイルへの依存はありません。

```cpp
#include <bits/stdc++.h>
using namespace std;

// 必要な library/*.hpp の中身をここへ貼る。

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  // 問題固有の処理を書く。
}
```

## ライブラリ追加時の約束

- ファイル全体を `main.cpp` の上部へ貼るだけで使えること。
- カスタム `namespace` や難しい共通基盤を使わないこと。
- ローカルファイルへの `#include "..."` を含めないこと。
- 外部パッケージや実行時ファイルに依存しないこと。
- 問題固有の `main` 関数や入出力を含めないこと。
- 使用する標準ヘッダをパーツ自身に記載すること。
- 最小のテストを追加すること。

## リポジトリ側の検証

パーツ集を更新するときだけ、次を実行します。コンテスト中の利用には不要です。

```bash
make verify
```

`make verify` はパーツのテストに加え、`practice/ahc*/main.cpp` をすべて
C++17で構文確認します。実戦例を増やしても、単一ファイル提出形式が壊れていないかを
まとめて確認できます。
