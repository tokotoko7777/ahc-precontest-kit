# AHC004: 共通局所探索フォーマットで5回改善

AHC004の過去問練習です。AHC069の解法・ベンチマークは変更していません。
AtCoderへの提出もしていません。

## 固定後の別20ケース

開発に使用していない公式generatorのseed 20–39で旧practiceと最終版を比較しました。
確認開始前にround5のソースhashを[selection.json](ahc004_five_rounds/selection.json)へ固定し、
確認結果を見ての再調整は行っていません。

| 版 | 相対点 / 2000 | 平均best比率 | 平均公式点 | W/T/L（最終版から見て） |
|---|---:|---:|---:|---|
| 旧practice | 1939.858410 | 96.992920% | 68,199,165.85 | — |
| round5 | 2000.000000 | 100.000000% | 70,255,501.65 | 20 / 0 / 0 |

平均公式点は+2,056,335.80（約+3.015%）、主指標の平均best比率は+3.007080 percentage points。
**100%はこの比較における共通best比率であり、問題の満点や本番1位相当を意味しません。**
本番とは別の入力なので、順位表の総得点との差を正確に主張することはできません。

全5開発比較200測定＋確認40測定、計240測定で不正出力・3秒超過は0。
最大実時間は2.785319秒。全出力の公式visualizerと独立採点が一致しました。

## 5回の改善ループ

開発はseed 0–9、各版2回。毎回、直前採用版と候補版を新たに逐次・交互実行しました。
分母は全5回の全比較版・全repeatを含む**共通の最新の入力別best**です。
分子は各入力について2回を平均します。自分だけを除く分母は使いません。

| 回 | 変更 | 相対点 / 1000（比較版→候補） | 平均公式点（比較版→候補） | 判定 |
|---:|---|---:|---:|---|
| 1 | 行・列差分のAho-Corasick照合＋共通SA Runner | 947.580 → 969.364 | 66260141.00 → 67702144.00 | 採用 |
| 2 | 3bit配置比較＋全800候補 | 972.875 → 978.600 | 67931416.35 → 68276612.55 | 採用 |
| 3 | 初期構築650→150ms | 978.033 → 983.006 | 68259811.60 → 68571749.95 | 採用 |
| 4 | 同点配置の均等選択 | 978.065 → 984.173 | 68257710.50 → 68687856.05 | 採用 |
| 5 | 開始温度3→6 | 981.121 → 984.840 | 68521923.10 → 68762542.80 | 採用 |

[開発時点での採否記録](ahc004_five_rounds/DECISIONS.md)も残しています。
今回の5候補は全て主指標で改善しましたが、各変更の一般的優位性や統計的有意性を
主張するものではありません。実時間終了なので、同じ版の再測定でも点が変動します。
小幅差を含む開発結果だけで完了せず、別20ケースで最終版を確認しました。

## 何をライブラリへ戻したか

- [Aho-Corasick](../library/aho-corasick.hpp)を追加。
  アルファベット数をテンプレート引数で選び、整数列を登録する複数パターン照合です。
  suffix・包含・重複登録を扱えます。
- [局所探索の基本フォーマット](../template/search/local-search/basic.cpp)は増設せず、
  既存のTimeBasedAnnealingRunnerを使用。
- [保守用例](../examples/search/ahc004_genome_sa.cpp)の
  State / Move / propose_move / evaluate_move / apply_moveに編集箇所のTODOを記載。
- [practice/ahc004/main.cpp](../practice/ahc004/main.cpp)はヘッダ展開済み。
  その1ファイルだけで使用できます。例と展開済みソースの一致をCIで検査します。

以前の版は変更セルを通る長さ2–12の各窓を再符号化していました。
新しい版は変更行・列を各1回走査し、語の出現数だけ差分更新します。
評価中のscratchはProblemへ置き、Stateと最良解は採用時だけ更新します。

3bitの一致度は**候補生成に限って使う代理値**です。
採否は全語達成時の空白bonusを含む正確な評価差で行います。
受理閾値を使った評価途中打ち切りはこの例では未実装です。

## 検査

- 単純な全文字比較と、1200回の差分評価・採用・棄却を照合。
- 巡回端、重複語、包含語、空白、全語達成時のbonusを検査。
- packed配置評価を文字ごとの比較と照合。
- 変更箇所のASan/UBSan検査を実施。
- 5候補の累積patchからソースを復元し、計測manifestのSHA-256と照合。
- 公式visualizerとは別実装のPython採点で、全出力の合法性・得点を再計算。

## 実験条件と再現

[事前プロトコル](ahc004_five_rounds/PROTOCOL.md)、
[生データ](results/)、[集計JSON](ahc004_five_rounds/final-summary.json)が正本です。
入力・出力・展開済みソースのSHA-256、tool binary hash、コンパイラ情報を保存しています。
同じ入力hashの既存CSVは分母へ含めます。今回の開始時に対応する旧raw CSVはありませんでした。

[公式問題](https://atcoder.jp/contests/ahc004/tasks/ahc004_a)の制限は3秒。
全版 `g++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra`、
重いビルドや別solverを並行させずに測定しました。
時間超過・不正・クラッシュは比較時に0点、診断用の延長実行はありません。

[公式ツールzip](https://img.atcoder.jp/ahc004/222362f13a30b1342bf79d0041bd4d39.zip)
のSHA-256は
`b17da9bf7e05231fb25cbc1733e2ef07063bb4bdb9cea13dd24e2e2227c9c4ed`。
[公式解説一覧](https://atcoder.jp/contests/ahc004/editorial)には解説がありませんでした。
既存の構造別レビューは着想にのみ利用し、第三者の提出コードを追加取得・転載していません。

```sh
# まず公式toolsを展開してcargo build --release --binsする。
python3 benchmarks/ahc004_improvement_benchmark.py \
  --variant baseline=benchmarks/ahc004_five_rounds/baseline.cpp \
  --variant final=examples/search/ahc004_genome_sa.cpp \
  --tools /path/to/tools \
  --seeds benchmarks/ahc004_five_rounds/seeds.txt \
  --first-case 20 --cases 20 --repeats 1 --output /tmp/ahc004-new-confirmation

# 保存済み全5比較を、同じ最新best分母で再計算する。
python3 benchmarks/ahc004_improvement_report.py \
  --experiment benchmarks/results --history benchmarks/results

# 例えばround3を復元する。各patchはbaselineからの累積差分。
patch --output /tmp/ahc004-round3.cpp \
  benchmarks/ahc004_five_rounds/baseline.cpp \
  benchmarks/ahc004_five_rounds/round3.patch
```

各計測ディレクトリには入力・出力・stderr・凍結ソースも残しています。
Gitにはraw CSV、manifest、凍結baseline、全5版の再現patchを収録し、
実行バイナリや全出力は含めません。

## 残る限界

盤面の直接被覆を改善する解法です。潜在的な40本の巡回列を再構成して
交点を整合させる構造的解法は実装していません。
今回は基本フォーマットの実戦適用と強化の確認であり、最適性・上位相当を主張しません。
