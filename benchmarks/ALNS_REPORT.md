# 適応的な近傍選択 — AHC059

## 何を分離して比べるか

- `library/adaptive-operator-selector.hpp`: 近傍の選択と、区間平均報酬による重み更新。
- `template/search/adaptive-large-neighborhood-search.cpp`: 問題側の壊し方・修復・報酬をTODOで説明。
- LNSの`last_outcome()`で、最良更新・現在値改善・同点/悪化採用・棄却を区別。
- AHC059は区間長4/8/15/30と、離れたランダム4ペアの5近傍を同じ修復処理で試す。

着想は[Røpke・PisingerのALNS論文](https://doi.org/10.1287/trsc.1050.0135)。
過去の成果に応じて近傍の使用頻度を変えるという考え方から独自に実装した。
論文の詳細な実験設定や第三者の提出コードを再現したものではない。

| 版 | 意味 |
|---|---|
| `legacy` | commit `6dca9b038915826f055a13ec94a227a5ce966810`のpractice。従来LNS+SA、区間15のみ |
| `sa` | 新しいコードで適応機能OFF。区間15のみ。追加コストの回帰確認 |
| `uniform` | 同じ5近傍を常に等確率で選ぶ |
| `adaptive` | 同じ5近傍を過去の成果に応じて選ぶ |

`uniform`対`adaptive`が**選択ポリシーだけの比較**。
単一近傍版との差には、問題固有の近傍追加による効果も含まれる。
どの版もSA温度8→0.25、総時間枠1.85秒（入力・初期解を含む）、距離表/途中打ち切りON。

## 学習設定

1試行あたりの報酬は最良更新1、現在値改善0.5、同点/悪化採用0.1、棄却/修復失敗0。
128試行ごとに `weight = 0.8 * weight + 0.2 * mean_reward`。
未試行の近傍は重みを維持。全重み0なら等確率。選択確率は
`0.1 / 近傍数 + 0.9 * weight / sum(weight)`なので、5近傍なら最低2%を残す。
近傍生成、SA採否、近傍選択の乱数列は独立。

時計取得・盤面hash・一致判定なし。選択O(近傍数)、通常の記録O(1)、
区間更新O(近傍数)。試行中のメモリ確保はない。
単位時間あたりの成果ではなく試行あたりの成果を学ぶため、
高採用率の小近傍へ偏る可能性もあり、強さはスコアで判断する。

## 測定方法

AHC059公式配布入力と公式Rust採点器を使用。配布物/採点器SHAと構築方法は
前回の[LNS_REPORT.md](LNS_REPORT.md#測定条件)と同じ。
g++ 13.3.0、`-std=c++17 -O2 -DNDEBUG -Wall -Wextra`。
全版を先にビルドし、1本ずつ順序をseed/repeatでローテーションする。
計時中は他のsolverや重いビルドを並走させない。
全出力を独立Python再生と公式採点で照合。時間超過・不正出力もCSVへ残し、0点扱い。

主指標は入力ごとの共通bestを分母にした `100 * score / best`。
全比較版・全repeat・同じ入力の過去の有効な実時間runを共通分母へ含める。
固定反復診断は分母へ入れない。2 repeatの場合は相対点を平均してからケース数分を合計する。
生得点平均・勝敗・反復数は副指標。本番順位や1位比率ではない。

## 固定反復による回帰確認

- [適応版の4スイッチ組合せ](results/ahc059-alns-fixed-5.csv): seed0–4、2,000反復。
  距離表/閾値打ち切りON/OFFの全4通りで得点・出力SHA・採用回数が一致。
- [旧版との互換](results/ahc059-alns-compat-fixed-5.csv): 同じ5入力・2,000反復。
  新しい`sa`と凍結した旧LNS+SAで得点・出力SHA・採用回数が一致。
- 単体テスト: 平滑更新、区間境界、報酬の平均、未試行、全0、探索下限、学習OFF、
  seed再現性、不正引数、LNSの成果分類、全5近傍の修復合法性。

これら30実行は診断専用。実時間性能や採否の根拠にしない。

## 開発10ケース、各2回（seed 0–9）

[生CSV](results/ahc059-alns-dev-10.csv)。共通bestには前回の
[同じ開発入力のLNS結果](results/ahc059-lns-dev-10.csv)も含めた。
相対点は2 repeatの平均を各入力で取り、10入力分を合計している。

| 版 | 平均公式得点 | 共通best比 / 1000 | 平均率 |
|---|---:|---:|---:|
| `legacy` | 15349.70 | 999.414029 | 99.941403% |
| `sa` | 15350.00 | 999.433413 | 99.943341% |
| `uniform` | 15350.05 | 999.436773 | 99.943677% |
| `adaptive` | 15351.85 | 999.553949 | 99.955395% |

適応版は等確率比+1.80点、従来版比+2.15点だが、小差。
80実行すべて合法・採点一致・2秒以内。ここまでの結果で設定の追加チューニングはせず、
seed60–89を各2回使って確認する。確認前の変更はコメントの明確化と集計表示だけで、
探索アルゴリズム・パラメータは変更していない。

## 再実行

```sh
# 同じ公式入力ディレクトリ・公式visを全版へ渡す。出力先は毎回新しいパスにする。
python3 benchmarks/lns_official_benchmark.py --task 059 \
  --inputs /path/to/ahc059/tools/in --tool /path/to/ahc059/tools/target/release/vis \
  --reference-ref 6dca9b038915826f055a13ec94a227a5ce966810 \
  --variants legacy sa uniform adaptive --cases 10 --repeats 2 \
  --history benchmarks/results/ahc059-lns-dev-10.csv \
  --artifacts /tmp/alns-dev-new --output /tmp/alns-dev-new.csv

# 最終確認。開発入力を混ぜず、設定固定後に実施。
python3 benchmarks/lns_official_benchmark.py --task 059 \
  --inputs /path/to/ahc059/tools/in --tool /path/to/ahc059/tools/target/release/vis \
  --reference-ref 6dca9b038915826f055a13ec94a227a5ce966810 \
  --variants legacy sa uniform adaptive --first-seed 60 --cases 30 --repeats 2 \
  --artifacts /tmp/alns-holdout-new --output /tmp/alns-holdout-new.csv
```

固定反復診断は同じコマンドに`--cases 5 --iterations 2000`を付け、
`--variants adaptive adaptive_no_cutoff adaptive_no_precompute adaptive_no_both`、
または`--variants previous_lns_sa sa`を選ぶ。別の新しい出力先を指定する。
固定反復は速度・得点による採用判断へ混ぜない。
スクリプトはソース・入出力・公式採点器のSHA、コンパイラ、全ログを保存する。
過去の実時間CSVを分母へ含める再比較では`--history`に全対象CSVを渡す。
