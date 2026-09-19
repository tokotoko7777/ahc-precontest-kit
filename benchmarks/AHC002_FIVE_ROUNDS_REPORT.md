# AHC002: 区間修復SAの5回改善比較

2026-09-19。`TimeBasedAnnealingRunner`の基本フォーマットで5候補を比較した。
第2回（内部区間を多く試す）と第4回（修復DFSの厳密な上界枝刈り）を採用した。
不採用案もsource差分とraw scoreを残している。

## 条件と主指標

[公式問題](https://atcoder.jp/contests/ahc002/tasks/ahc002_a)の2秒制限に対して、
全体のsolver予算は初期化込み1,870 ms。C++17、O3、NDEBUG。
公式生成器のseed 0〜9で開発し、設定固定後にseed 30〜49で確認した。
これらは大会の100ケースとは別のローカル入力。
各ケース各版1回、実行順を交互にして逐次実行。測定中に他のbuild/solverを実行しない。

全経路のタイル重複・盤面外移動を独立検査し、公式visualizerの得点と一致を確認した。
失敗・2秒超過は0点。主指標は入力SHAごとの合法・制限時間内の過去最高を分母とする
`sum(100 * score / best)`。今回の全比較版と保存済みLNS実験も含む共通ベクトルで再計算。
開発は1000点、確認は2000点満点。raw平均・勝敗は補助指標。

## 5回の開発比較

| 回 | 候補 | 対照 → 候補 /1000 | 平均比率 | 判定 |
|---|---|---:|---:|---|
| 1 | 候補を置換区間だけ保持し、採用後cacheを差分更新 | 969.743 → 961.363 | 96.9743% → 96.1363% | 不採用 |
| 2 | 内部修復の割合を30%→80%へ | 963.773 → 967.167 | 96.3773% → 96.7167% | 採用 |
| 3 | 最初の合法修復でDFSを止め、SAへ悪化も渡す＋温度500→1 | 979.316 → 922.856 | 97.9316% → 92.2856% | 不採用 |
| 4 | 残り歩数×99点・到達偶奇による上界枝刈り | 978.160 → 979.939 | 97.8160% → 97.9939% | 採用 |
| 5 | 半分の区間を逆向きDFSで修復 | 981.447 → 978.125 | 98.1447% → 97.8125% | 不採用 |

各回で対照も再実行している。時間ベースの初期化・SAなので同じ版の得点にも揺らぎがある。
特に第2・4回の差は小さく、開発10件だけで一般的優位は主張しない。
第1回は実装上のcopy削減をしても、この測定では得点が上がらなかったため採用しなかった。

## 別20ケースでの固定設定確認

| 版 | 共通分母 /2000 | 平均比率 | 平均公式score | 新版の勝/分/敗 |
|---|---:|---:|---:|---|
| 元のフォーマットSA | 1952.711484 | 97.635574% | 56,618.70 | 16 / 0 / 4 |
| 従来のpractice | 1959.346457 | 97.967323% | 56,800.75 | 15 / 0 / 5 |
| 第2・4回を採用 | 1983.619212 | 99.180961% | 57,519.15 | — |

元のフォーマット比+1.5904%、従来practice比+1.2648%。
開発100出力＋確認60出力は全件合法・2秒以内・独立採点と公式採点が一致。
確認での最長実時間は1.873秒未満。確認結果を使った追加チューニングは行っていない。
各ケース1回で、統計的有意差・別環境での時間保証・大会順位相当は主張しない。
同一入力集合の1位比較データは取得していないため、1位との差は記載しない。

## 問題側と共通部分

`TilePathProblem`だけが経路、使用タイル、修復DFS、差分scoreを知る。
Runnerは温度・時間・採否・best管理を担い、今回変更していない。
修復DFSの枝刈りは、固定終点を除く追加マス数×99が楽観上界になることを利用したもの。
汎用Runnerに問題固有の99点や格子の偶奇は埋め込まない。
得点は変更区間の差分で計算するが、採用後の使用タイル・prefix cache再構築は従来通り。
採用しなかった第1回の差分cache更新が混ざっているわけではない。

## 検証と再現

- 6,000近傍の合法性・差分得点・cacheを全再計算と照合。
- 3×3の全単純路300件と枝刈りDFSの最大得点を照合。DFSの仮更新が戻ることも確認。
- 上記検査は通常ビルド、ASan/UBSanとも成功。
- 5候補のpatchから測定時sourceを復元し、manifestのSHAと一致を検査。
- 提出用`practice/ahc002/main.cpp`は原本＋ヘッダの展開一致をCIで検査する。

[最終集計](ahc002_five_rounds/final-summary.json)、
[確認CSV](results/ahc002-five-rounds-confirmation.csv)、
[測定手順](ahc002_five_rounds/PROTOCOL.md)。開発CSVは`ahc002-five-rounds-{1..5}-dev.csv`。
`ahc002_five_rounds/baseline.cpp`へ各`round{1..5}.patch`を当てると測定時候補になる。
各patchはbaselineからの累積差分。従来practiceも`legacy.cpp`として保存した。

```sh
python3 benchmarks/ahc002_improvement_report.py \
  --experiment benchmarks/results --history benchmarks/results

python3 benchmarks/ahc002_improvement_benchmark.py \
  --variant baseline=benchmarks/ahc002_five_rounds/baseline.cpp \
  --variant selected=examples/search/ahc002_destroy_repair_sa.cpp \
  --variant legacy=benchmarks/ahc002_five_rounds/legacy.cpp \
  --tools /path/to/ahc002/tools --seeds benchmarks/ahc002_five_rounds/seeds.txt \
  --first-case 30 --cases 20 --output build/ahc002-new-confirmation
```

新しい汎用手法・テンプレートは追加していない。局所探索の1種類のProblem実装に収めた。
