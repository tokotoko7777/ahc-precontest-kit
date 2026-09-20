# AHC005: 代表点選択と巡回順の差分焼きなまし、5回比較

非インタラクティブな過去問で共通の局所探索フォーマットを使いました。
今後の番号順の比較ではインタラクティブ問題を飛ばします。
AHC069の解法・ベンチマークは未変更、AtCoderへの提出もしていません。

## 設定固定後の20ケース

[selection.json](ahc005_five_rounds/selection.json)へround4のhashを固定した後、
未使用の公式seed 1020–1039を比較しました。確認結果を使った再調整はありません。

| 版 | 相対点 / 2000 | 平均best比率 | 平均公式点 | 最終版のW/T/L |
|---|---:|---:|---:|---|
| 旧practice | 1744.737551 | 87.236878% | 195,246.10 | — |
| round4 | 2000.000000 | 100.000000% | 225,045.25 | 20 / 0 / 0 |

平均公式点は**+15.262%**、主指標の平均best比率は+12.763122 percentage points。
100%はこの比較における入力別best比率です。問題の満点や大会1位相当ではありません。
公式本番とは入力が異なるため、順位表の総得点とは直接比較できません。

5開発比較200測定＋確認40測定、**全240測定が合法・3秒以内**。
全出力で公式visualizerと独立した経路・視界・時間計算が一致しました。

## 5回の改善ループ

開発seed 1000–1009で、毎回直前採用版と候補を各2回、逐次・交互実行しました。
旧測定のseed 0–99と分離しています。
分母は全5回の全比較版・全repeatを含む最新の共通入力別bestです。
分子は入力ごとの2回平均で、自己除外の分母は使いません。

| 回 | 変更 | 相対点 / 1000（比較版→候補） | 平均公式点（比較版→候補） | 採否 |
|---:|---|---:|---:|---|
| 1 | 代表点＋巡回順の共通SA | 871.475 → 986.781 | 194765.70 → 220697.15 | 採用 |
| 2 | 代表点変更＋relocateを同時化 | 987.660 → 995.647 | 220909.65 → 222662.45 | 採用 |
| 3 | 全辺から最良挿入位置を選ぶ | 992.689 → 995.475 | 222060.65 → 222643.70 | 採用 |
| 4 | 開始温度100→30 | 991.831 → 994.874 | 221860.20 → 222531.15 | 採用 |
| 5 | 探索用距離表を連続16bit化 | 996.889 → 992.520 | 222931.25 → 221971.40 | 不採用 |

[開発時点の採否](ahc005_five_rounds/DECISIONS.md)と
[最終集計JSON](ahc005_five_rounds/final-summary.json)も保存しています。
**5回目の高速化案はスコアが下がったため不採用**にし、round4へ戻しました。
小幅な差には時間依存の揺れも含まれます。各変更の普遍的優位性や統計的有意性は主張しません。

## フォーマットと解法

[保守用の例](../examples/search/ahc005_patrol_sa.cpp)は、
[局所探索の基本形](../template/search/local-search/basic.cpp)と同じ
TimeBasedAnnealingRunnerを使用し、専用テンプレートは増やしていません。
State / Move / propose_move / evaluate_move / apply_moveに日本語TODOを置きました。
[practiceの単一ファイル](../practice/ahc005/main.cpp)はヘッダ展開済みです。

長さ2以上の水平・垂直道路区間ごとに代表点を1つ選び、訪問順と同時に最適化します。
各groupの候補点内で代表を変更するので、探索中も全道路の可視性を保てます。
2-opt、代表点変更、代表点を変えながらの最良再挿入を使います。
距離差は境界辺だけで計算し、Stateを書き換えるのは採用時だけです。

到着マスの料金を引くと最短距離が対称になるため、
2-optでも反転区間内部を読み直す必要がありません。
候補生成の最良挿入位置検索はO(group数)、評価関数の差分自体はO(1)です。
受理閾値による途中打ち切りはこの例では使っていません。

最終設定は温度30→1、探索終了の目安2600ms、不要点削除の期限2820ms。
旧practiceも最初に実行して保存し、最後は合法性を検査して移動時間の短い方を出力します。
代表点被覆は必要条件より強い十分条件です。通過途中の視界を使った不要点削除で緩和しますが、
元の自由な巡回路問題の最適性を保証するものではありません。

## 比較上の注意

旧版は数ms〜数十msで終了する決定的解、新版は3秒枠で探索する解法です。
改善は代表点選択・近傍・探索時間を含む**解法全体の成績**であり、
Runner単独の高速化率とは扱いません。
5回目の16bit化はhotな距離参照用の追加表で、復元用の元の距離表は保持します。
総メモリを削減する変更ではなく、今回は最終版へ入れていません。

## 検査と記録

- 36,000試行の差分値を全辺再計算と照合。代表点の所属、開始点固定、閉路・視界も検査。
- 変更部分はASan/UBSan検査に通過。
- 全5版の累積patchを復元し、計測manifestのSHA-256と照合。
- 例とpracticeのヘッダ展開が完全一致することをCIで検査。
- 独立採点は不正方向、壁、盤外、帰還、部分可視、ゼロ移動も検査。
- raw CSVに入力・出力・ソースhash、時間、合法性、反復数を保存。
- 3秒超過・不正・クラッシュは0点。延長時間診断はありません。

## 再現

[事前プロトコル](ahc005_five_rounds/PROTOCOL.md)、
[raw CSV](results/)、[固定ソース・manifest](ahc005_five_rounds/)を参照してください。
全版C++17、`-O3 -DNDEBUG -Wall -Wextra`、重いビルドや他solverと並行させずに計測しました。

[公式問題](https://atcoder.jp/contests/ahc005/tasks/ahc005_a)、
[公式tools zip](https://img.atcoder.jp/ahc005/dc9ed10f037e2dd4b48ca255dbd470d9.zip)を使用。
zipのSHA-256:
`cb118e650312ffdded7b9d6c4f77b3d9ffb60880dacdcc0964acafd17eec4c09`。
ローカルレビューから構造的着想を得ましたが、第三者の提出コードは取得・転載していません。

```sh
# 公式toolsを展開してcargo build --release --binsした後:
python3 benchmarks/ahc005_improvement_benchmark.py \
  --variant baseline=benchmarks/ahc005_five_rounds/baseline.cpp \
  --variant final=examples/search/ahc005_patrol_sa.cpp \
  --tools /path/to/tools --seeds benchmarks/ahc005_five_rounds/seeds.txt \
  --first-case 20 --cases 20 --repeats 1 --output /tmp/ahc005-new-confirmation

python3 benchmarks/ahc005_improvement_report.py \
  --experiment benchmarks/results --history benchmarks/results

# 不採用の5回目も復元できる。各patchはbaselineからの累積差分。
patch --output /tmp/ahc005-round5.cpp \
  benchmarks/ahc005_five_rounds/baseline.cpp \
  benchmarks/ahc005_five_rounds/round5.patch
```

計測ディレクトリには生成入力、実出力、stderr、展開済みソースも保存しています。
GitにはCSV・manifest・baseline・再現patchを収録し、バイナリや全実出力は含めません。
