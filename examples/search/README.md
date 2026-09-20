# 探索コアを単体で使う完全な例

探索Runnerを実問題へ当てはめ、問題を最初から最後まで解く`main.cpp`です。
小さなAPI例ではなく、入力、状態、近傍または遷移、得点計算、
解の保存、出力まで含みます。差分更新が有効な例では、その実装も確認できます。

## まず読む例

| 入口 | 最初の例 |
|---|---|
| ビーム | [通常版](intro_heuristics_simple_beam.cpp)、[木上版](ahc021_tree_beam.cpp) |
| 局所探索 | [AHC006の差分更新](ahc006_sa.cpp)。[基本形](../../template/search/local-search/basic.cpp)で焼きなまし／山登りを切替 |
| モンテカルロ | [AHC015](ahc015_common_rollout.cpp) |

LNSのような部分破壊・再構築も近傍として扱います。
[AHC002](ahc002_destroy_repair_sa.cpp)では焼きなましのProblem内に実装しています。
この整理は既存の実問題解法・測定結果を変更するものではありません。

## ビーム

[穴埋めテンプレート](../../template/search/beam/README.md)

| ファイル | 使う探索 | 題材 | 確認したこと |
|---|---|---|---|
| [`ahc032_chokudai.cpp`](ahc032_chokudai.cpp) | `ChokudaiSearch` | AHC032 | Actionビームと同じProblem・評価。層別キュー巡回、容量制限、合法な初期回答 |
| [`ahc011_tree_beam.cpp`](ahc011_tree_beam.cpp) | `TreeBeamRunner` | AHC011 | 最大4手をFixedVectorで列挙。盤面1個をapply/revertし、差分hash、同一局面除去、全候補からの最良解復元を使用 |
| [`intro_heuristics_simple_beam.cpp`](intro_heuristics_simple_beam.cpp) | `SimpleBeamSearch` | Introduction to Heuristics Contest A | 365日入力を最後まで構築し、出力日数・番号範囲・得点計算を確認した |
| [`intro_heuristics_action_beam.cpp`](intro_heuristics_action_beam.cpp) | `ActionBeamRunner` | Introduction to Heuristics Contest A | 問題依存コードを1 structへ分離。State・Action・Scoreと3関数へ何を書き何を返すか、行ごとのコメント付き |
| [`ahc021_tree_beam.cpp`](ahc021_tree_beam.cpp) | `TreeBeamRunner`＋`RadixHeap` | AHC021 | 前線マスへの最短路で球を1個ずつ確定。候補生成を高速化し、完成候補の巡回で正式scoreを最大化。practice・採点と同じProblemを共有 |
| [`ahc032_action_beam.cpp`](ahc032_action_beam.cpp) | `ActionBeamRunner` | AHC032 | 3×3スタンプ多重集合を2-byte Action化。採用候補だけ盤面をコピーし、49位置を順に確定 |
| [`variable_cost_beam.cpp`](variable_cost_beam.cpp) | `CostTreeBeamRunner` | 締切付き宝集め | Problem型へ可変長行動を分離。1、2、3世代進む行動と再訪を扱い、200ランダムケースを厳密DPと照合した |
| [`ahc038_variable_cost_beam.cpp`](ahc038_variable_cost_beam.cpp) | `CostTreeBeamRunner` | AHC038 | 「次の把持・解放」まで1手で世代を飛ばす。問題側は候補・apply/revert・評価・進行量・keyだけを書き、公式seed 0--99を全て合法に完了した |
| [`ahc071_action_beam.cpp`](ahc071_action_beam.cpp) | `ActionBeamRunner` | AHC071 | 上段から必要な支持位置を渡す行DP。全体構築と区間再構築を同じProblem型で行い、同じ次段条件をkeyでまとめる |

## 局所探索（焼きなまし・山登り）

AHC001〜005の5回改善記録: [001](../../benchmarks/AHC001_FIVE_ROUNDS_REPORT.md)、
[002](../../benchmarks/AHC002_FIVE_ROUNDS_REPORT.md)、[003](../../benchmarks/AHC003_FIVE_ROUNDS_REPORT.md)、
[004](../../benchmarks/AHC004_FIVE_ROUNDS_REPORT.md)、[005](../../benchmarks/AHC005_FIVE_ROUNDS_REPORT.md)。
[AHC003の山登り推定](ahc003_online_fit.cpp)は、同じ基本フォーマットを未知コストモデルの
差分更新に使う例です。方式ごとのテンプレート追加はしていません。

[穴埋めテンプレート](../../template/search/local-search/README.md)

| ファイル | 使う探索 | 題材 | 確認したこと |
|---|---|---|---|
| [`ahc001_region_sa.cpp`](ahc001_region_sa.cpp) | `TimeBasedAnnealingRunner` | AHC001 | 境界押し移動と1・2領域再構築。単調な損失の閾値打ち切り、仮変更buffer、整数サイズ仕上げをProblemへ分離 |
| [`ahc002_destroy_repair_sa.cpp`](ahc002_destroy_repair_sa.cpp) | `TimeBasedAnnealingRunner` | AHC002 | 可変長経路の末尾再構築と区間DFS修復をProblemへ分離。採用時のbuffer移動と最良解からの再開を使用 |
| [`ahc004_genome_sa.cpp`](ahc004_genome_sa.cpp) | `TimeBasedAnnealingRunner`＋`AhoCorasick` | AHC004 | 巡回文字列の出現を変更行・列だけ差分更新。評価中はState不変、採用時だけcacheを反映 |
| [`ahc005_patrol_sa.cpp`](ahc005_patrol_sa.cpp) | `TimeBasedAnnealingRunner` | AHC005 | 直線道路の代表点選択と巡回順を同時探索。到着マス料金の対称化、境界辺だけの差分評価 |
| [`ahc003_online_fit.cpp`](ahc003_online_fit.cpp) | `TimeBasedAnnealingRunner`（山登り） | AHC003 | 観測履歴の正則化モデルを座標更新し、関係する観測の予測値だけ差分更新 |
| [`ahc006_sa.cpp`](ahc006_sa.cpp) | `TimeBasedAnnealingRunner` | AHC006 | 辺差分＋逆引き位置cache。候補経路コピーなし、採用時だけ更新。最良2点挿入O(n)、距離前計算ON/OFF |
| [`ahc002_destroy_repair_lns.cpp`](ahc002_destroy_repair_lns.cpp) | `LargeNeighborhoodSearch` | AHC002 | 既存の末尾/区間修復をdestroy/repairへ分離。得点最大化と、採用前のcache再構築省略の例 |
| [`ahc059_lns.cpp`](ahc059_lns.cpp) | `LargeNeighborhoodSearch` + `AdaptiveOperatorSelector`（任意） | AHC059 | ペアを削除しO(n)最良再挿入。SA/RRT/山登り、前計算、打ち切り。単一近傍/等確率/適応選択を比較可能 |
| [`ahc059_ils.cpp`](ahc059_ils.cpp) | `IteratedLocalSearch` | AHC059 | 区間30の摂動と区間4の局所改善を分離。局所探索の連続失敗上限・受理方式を比較 |
| [`ahc058_prefix_sa.cpp`](ahc058_prefix_sa.cpp) | `TimeBasedAnnealingRunner`＋`PrefixReplay` | AHC058 | 3手先読みを初期解に購入順序をSA。途中再生・待機一括更新を使い、公式100ケースで平均約6.24%改善 |

## モンテカルロ

[穴埋めテンプレート](../../template/search/monte-carlo/README.md)

| ファイル | 使う探索 | 題材 | 確認したこと |
|---|---|---|---|
| [`ahc015_common_rollout.cpp`](ahc015_common_rollout.cpp) | `CommonScenarioRolloutRunner` | AHC015 | 4方向を同じ未来配置で比較。盤面操作・Scenario・rollout評価と共通乱数処理の境界を明示 |
| [`ahc061_common_rollout.cpp`](ahc061_common_rollout.cpp) | `CommonScenarioRolloutRunner` | AHC061 | 相手の粒子推定と衝突simulationを問題側へ分離。独自乱数・double加算・近似同点を保存 |
| [`ahc015_uct.cpp`](ahc015_uct.cpp) | `MonteCarloTreeSearch` | AHC015 | 1手ずつ対話入力。未知の配置順位を抽選し、その結果ごとに木を分岐。共通未来flat MCとも比較 |

## 決定的な先読み

[穴埋めテンプレート](../../template/advanced/README.md)

| ファイル | 使う探索 | 題材 | 確認したこと |
|---|---|---|---|
| [`ahc026_deterministic_rollout.cpp`](ahc026_deterministic_rollout.cpp) | `DeterministicRolloutRunner` | AHC026 | 全先読み幅を最後まで同じ貪欲で仮実行。山操作・候補幅・完走評価と最小値選択の境界を明示 |
| [`ahc058_deterministic_rollout.cpp`](ahc058_deterministic_rollout.cpp) | `DeterministicRolloutRunner` | AHC058 | 固定長状態を3手先読み。合法手・投資・生産式はProblemへ、候補比較はRunnerへ分離 |

数値はライブラリの適用確認用で、AtCoder上の順位やスコアを主張するものでは
ありません。乱数seedは固定ですが、壁時計で終了するAHC006例の反復回数と結果は
実行負荷によって多少変わります。

AHC002・006・011・015・021・026・032のRunner形式7本は、決定的な入力を使った短時間
スモークテストで、出力行数・値域・操作数を確認しています。AHC002・006は経路、
AHC011は全スライドと木サイズ、AHC021は操作再生後の差分score、AHC026は全ての
箱移動と消費energy、AHC032は盤面と
最終scoreも全再計算と照合します。
これは入出力と実装整合性の確認であり、公式seedの得点比較ではありません。

AHC058・061の旧rollout移植比較は[`rollout_official_benchmark.py`](../../benchmarks/rollout_official_benchmark.py)
で再実行できます。AHC058の`practice`はSAへ強化したため、この移植比較だけは旧commitの
単一ファイルを固定して使います。新SAとの比較は
[`ahc058_annealing_benchmark.py`](../../benchmarks/ahc058_annealing_benchmark.py)です。
AHC061は対話問題なので、公式`tester`が必須です。AHC058の閉形式の生産量は
独立した1ターンずつの再生ともCIで照合します。

ビーム幅による問題本来の得点差は、`make benchmark-search`でAHC032
「Mod Stamp」相当の固定5ケースを解いて確認できます。これは公式入力ではなく、
公式仕様と同じ値域・分布で独自生成した回帰ベンチマークです。
`ActionBeamRunner`が上位N個の選択・Stateの生成・探索ループを担当し、
問題側には確定順、候補生成、差分評価、反映だけを書いています。

焼きなまし、Monte Carlo、apply/revert木上ビームにも同じ境界のRunnerと実問題
ベンチマークがあります。各方式で人が書く箇所、ライブラリが担当する箇所、得点は
[`REAL_PROBLEM_BENCHMARKS.md`](../../REAL_PROBLEM_BENCHMARKS.md)を参照してください。

## コンパイル

リポジトリのルートで実行します。

```sh
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc001_region_sa.cpp -o /tmp/ahc001_sa
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc002_destroy_repair_sa.cpp -o /tmp/ahc002_sa
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc006_sa.cpp -o /tmp/ahc006_sa
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc011_tree_beam.cpp -o /tmp/ahc011_beam
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc015_common_rollout.cpp -o /tmp/ahc015_rollout
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc026_deterministic_rollout.cpp -o /tmp/ahc026_rollout
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/intro_heuristics_simple_beam.cpp -o /tmp/intro_beam
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/intro_heuristics_action_beam.cpp -o /tmp/action_beam
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc021_tree_beam.cpp -o /tmp/ahc021_beam
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc032_action_beam.cpp -o /tmp/ahc032_beam
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/variable_cost_beam.cpp -o /tmp/cost_beam
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc038_variable_cost_beam.cpp -o /tmp/ahc038_beam
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc071_action_beam.cpp -o /tmp/ahc071_beam
```

世代飛ばし版には小さい厳密DPとの自己テストも同じファイルに入っています。

```sh
g++ -std=c++17 -O2 -Wall -Wextra -pedantic \
  -DVARIABLE_COST_BEAM_SELF_TEST examples/search/variable_cost_beam.cpp \
  -o /tmp/cost_beam_test
/tmp/cost_beam_test
```

## 提出用の1ファイルにする

AHC001は[`practice/ahc001/main.cpp`](../../practice/ahc001/main.cpp)にヘッダを
展開済みです。そのまま1ファイルで提出でき、原本との一致をCIで検査します。
問題側の編集場所、公式ツールでの採点、1位との差の扱いは
[`practice/ahc001/README.md`](../../practice/ahc001/README.md)を参照してください。

例では読みやすさのため、次のようにリポジトリ内のヘッダを参照しています。

```cpp
#include "../../library/simple-beam-search.hpp"
```

実際に提出する時は、そのヘッダの中身をこの行の位置へ丸ごと貼り、`#include`の
1行を削除します。それ以外の補助ファイルや生成処理は必要ありません。公開済みcommit
から自動で1ファイル化する場合は、`copy_part.py`がこの相対includeも除去します。

```sh
python3 tools/copy_part.py --ref <公開済みSHA> \
  --main examples/search/ahc038_variable_cost_beam.cpp \
  library/cost-tree-beam-search.hpp -o submission.cpp
```

各例は仕組みを見通せることを優先しています。問題に合わせて最初に変える場所は、
焼きなましなら近傍と温度、ビームなら`evaluate`と幅です。

AHC038例は、特に触る場所へ`TODO(AHC038)`を付けています。`State`、`Move`、
候補生成、差分適用・復元、評価関数、腕形状、ビーム幅の順に読めます。幅だけを
比較する時は`-DAHC038_BEAM_WIDTH=1`のようにコンパイル時指定できます。

AHC002の公式配布入力がある場合は、既存版との合法性・score比較を複数seedで
再現できます。Runner版だけは厳格警告もエラーとしてコンパイルします。

```sh
python3 benchmarks/ahc002_official_benchmark.py \
  --inputs /path/to/ahc002/in --cases 10
```

AHC011も、公式generatorで作った入力を同じ形で比較できます。全スライドを再生し、
最大の非巡回連結成分と公式scoreを独立に計算します。

```sh
python3 benchmarks/ahc011_official_benchmark.py \
  --inputs /path/to/ahc011/in --cases 10
```

AHC026は未知情報をsampleせず、候補の先読み幅を最後まで決定的に仮実行します。
`TODO(AHC026)`を検索すると、State、候補、置き先、分離条件、完走評価の順に読めます。
公式generatorの入力がある場合は、既存版とRunner版を同じ入力で比較できます。

```sh
python3 benchmarks/ahc026_official_benchmark.py \
  --inputs /path/to/ahc026/in --cases 10
```

AHC071例も同様に`TODO(AHC071)`を検索できます。1行の最小費用DP、その上位候補列挙、
将来費用、`State / Action / Score`、候補生成、評価、反映、同一状態key、完成済み解を
使う費用閾値の順です。公式入力とvisualizerがある場合は、次で別solverとも同一seedを
比較できます。

```sh
python3 benchmarks/ahc071_official_benchmark.py \
  --tools /path/to/AHC071 --reference /path/to/AHC071/main3.cpp --cases 10
```

Rust toolchainを使えない環境では`--ported-score`を付けると、公式`src/lib.rs`の
合法性検査とscore式を移植したPython版で採点します。
