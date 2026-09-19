# AHC001: フォーマットを使った5回の改善比較

2026-09-19。既存の`RegionProblem`と`TimeBasedAnnealingRunner`を基準に、
候補を5回比較した。第2・3・4回を採用、第1・5回を不採用とし、
設定固定後の別20ケースでも公式スコア改善を確認した。
AHC069の解答・ベンチマークは変更していない。AtCoderへの提出も行っていない。

## 測定条件

- [公式問題](https://atcoder.jp/contests/ahc001/tasks/ahc001_a)、5秒制限。solver予算は4,750 ms。
- 公開system seed一覧の**行100〜109**を開発用、**行200〜219**を確認用とした。
  数字は生成seedそのものではない。以前の測定が100〜119を含むため、確認用を分離した。
- 各版・各ケース1回。実行順を交互にし、逐次実行。測定中に他の重いbuild/solverを実行しない。
- C++17、`-O3 -DNDEBUG -Wall -Wextra`。入力、出力、展開済みsource、公式toolのhashを記録。
- 公式visualizerと独立した合法性・得点検査を照合。失敗・時間超過は0点。
- 主指標は `sum(100 * score / best)`。`best`は入力SHAごとの合法・制限時間内の
  過去最高で、今回の全比較版も含む**共通の最新ベクトル**。leave-one-outにはしない。
  開発10件で1000点、確認20件で2000点。平均比率も併記する。

## 開発: 5回の比較

各行で対照も再実行しているため、同じ版名の得点にも時間ベース探索の揺らぎがある。
以下は全5回終了時の共通分母で再計算した値。速度単体では採否を決めていない。

| 回 | 変更案 | 対照 → 候補 /1000 | 平均比率の変化 | 判定 |
|---|---|---:|---:|---|
| 1 | 最終フェーズを微小温度SAから厳密山登りへ | 997.519 → 996.907 | 99.7519% → 99.6907% | 不採用 |
| 2 | 最大空き長方形の作業領域再利用＋面積上界で打ち切り | 996.705 → 997.260 | 99.6705% → 99.7260% | 採用 |
| 3 | 重なった領域を4方向から合法に縮め、最大面積を残す | 997.423 → 998.704 | 99.7423% → 99.8704% | 採用 |
| 4 | 1/4の境界近傍で要求面積に合う辺長を直接提案 | 998.492 → 998.879 | 99.8492% → 99.8879% | 採用 |
| 5 | 3/4の選択をランダム4社中の最低評価へ偏らせる | 999.403 → 998.411 | 99.9403% → 99.8411% | 不採用 |

第1回では中立移動まで拒否する変更が得点を下げた。領域の面積に余裕を持つ代理評価では、
同得点の移動にも価値があり得る。ただし因果の切り分け実験までは行っていない。
第5回は低評価部分だけの優先で全体配置が良くなるとは限らない例。

## 設定固定後: 別20ケース

| 版 | 共通分母の合計 /2000 | 平均比率 | 平均公式score | 勝/分/敗（候補から） |
|---|---:|---:|---:|---|
| 元の版 | 1994.758797 | 99.737940% | 985,088,989.80 | — |
| 第2〜4回を採用 | 1999.848399 | 99.992420% | 987,606,315.90 | 17 / 0 / 3 |

平均scoreは+0.255543%、満点からの損失は16.8823%減。
開発100出力＋確認40出力はすべて合法で、公式採点と独立採点は全件一致。
確認時の最長実時間は4.738秒未満、5秒超過0件。
各ケース1回の測定であり、統計的有意差・他環境での時間保証・公式順位は主張しない。
確認結果を使った追加チューニングはしていない。

保存済みの1位参考値（hakomo、1000件平均993,823,071.683）との差は6,216,755.783点、
比率99.3745%、満点からの損失倍率2.0064倍。
**入力集合が違うため参考値であり、現在の順位表を再確認した値ではない。**
出典・取得時点・再確認の制約は[practice側](../practice/ahc001/README.md#1位との差の記録方法)を参照。

## ライブラリと問題固有部分

- 汎用: `LargestEmptyRectangleWorkspace<Rect>`でscratchを再利用。従来の4引数APIは維持。
  面積上界の枝刈りは、候補幅が以後小さくなることによる厳密な打ち切り。
- 問題固有: 4方向の領域縮小、目標面積の辺長提案。`RegionProblem`内に置く。
- 探索・時間・温度・採否は既存の`TimeBasedAnnealingRunner`。新しい方式別テンプレートは増やさない。
- `practice/ahc001/main.cpp`は原本＋ヘッダの展開一致をCIで検査する、提出可能な1ファイル。

## 再現用記録

- [最終集計](ahc001_five_rounds/final-summary.json)、[測定手順](ahc001_five_rounds/PROTOCOL.md)
- [確認40件CSV](results/ahc001-five-rounds-confirmation.csv)
- 開発CSVは`results/ahc001-five-rounds-{1..5}-dev.csv`。
- `ahc001_five_rounds/baseline.cpp`と`round{1..5}.patch`から各候補の**測定時source**を復元可能。
  各patchは直前版ではなくbaselineからの差分。不採用の版も保存している。
- 第4回・確認で共通の候補source SHA256:
  `28a09200d081e3e4359d5d2dfabb1cc1978eb37dd40f2b7b4b29b71958206866`。

保存済みCSVから共通分母で再集計:

```sh
python3 benchmarks/ahc001_improvement_report.py \
  --experiment benchmarks/results --history benchmarks/results
```

公式toolを別途用意して新しいrun directoryへ測定:

```sh
python3 benchmarks/ahc001_improvement_benchmark.py \
  --variant baseline=benchmarks/ahc001_five_rounds/baseline.cpp \
  --variant selected=examples/search/ahc001_region_sa.cpp \
  --tools /path/to/ahc001/tools --seeds /path/to/system/seeds.txt \
  --first-case 200 --cases 20 --output build/ahc001-new-confirmation
```

時間ベースなので再実行時の完全な得点一致は要求しない。sourceの同一性と採点の整合性は検査する。

## コード検証

- `make verify`: 全体の構文検査、単体検査、提出ファイルの展開一致検査に成功。
- `empty_rectangle_upgrades_test`と`ahc001_region_upgrades_test`: ASan/UBSanに成功。
- `ahc001_improvement_test.py`: 5候補のpatch復元SHAと保存済み共通分母集計の一致に成功。
- 最初の未commit状態の`make verify`は、配布ツールが未commitヘッダの梱包を拒否して停止。
  専用作業枝へcommitした後、全体を再実行して成功した。検査を無効化していない。
