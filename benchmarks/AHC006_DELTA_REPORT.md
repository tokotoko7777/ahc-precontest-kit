# AHC006: 焼きなましの差分更新

目的は反復速度そのものではなく、**同じ時間内の公式得点を伸ばすこと**。
対象は過去問題[AHC006](https://atcoder.jp/contests/ahc006/tasks/ahc006_a)だけ。
AHC069は変更せず、AtCoderへの提出も行っていない。

## 変えた部分

旧`examples/search/ahc006_sa.cpp`は`evaluate_move`で差を返していたが、
その前に全経路をコピーし、全制約・全距離を計算していた。
新実装は`State`に現在距離とイベントの逆引き位置、32byteの`Move`に変更位置と差分を持つ。
不採用なら状態を変えず、採用時だけ経路・位置cache・距離を更新する。

| 近傍 | 候補評価・制約チェック | 採用時の更新 |
|---|---|---|
| 1点移動 | 変更辺だけO(1)、対応イベントの位置から合法な挿入範囲を抽選 | 移動区間だけO(k) |
| 2点交換 | 触れる高々4辺＋2イベントの先行制約、O(1) | 2点と2つの逆引き位置、O(1) |
| 区間反転 | 境界2辺O(1)＋区間内の先行制約O(k) | 反転区間だけO(k) |
| 注文入替 | 削除辺の差分＋2点最良挿入O(n)、候補経路コピーなし | 採用時だけ経路を詰めて再挿入、O(n) |

新しい汎用部品は[`ordered-pair-insertion.hpp`](../library/ordered-pair-insertion.hpp)。
異なる2辺への挿入は独立なので、前半点の最小増分を走査しながら保持するとO(n)になる。
同じ辺へ2点を入れる場合は別計算し、同点の選択も旧二重ループに揃えた。
点・経路・距離の型を固定せず、非対称距離にも対応する。
容量・時間窓などの追加制約は扱わない。反転差分だけは対称距離が必要。

[`route-utils.hpp`](../library/route-utils.hpp)にも1点移動・2点交換の差分を追加。
探索RunnerのAPIは変更していない。TODO付き穴埋めに差分更新の書き方を追記し、
[`practice/ahc006/main.cpp`](../practice/ahc006/main.cpp)はヘッダ展開済みで1ファイル提出できる。

距離前計算は`-DAHC006_PRECOMPUTE_DISTANCE=0/1`で独立に切り替え可能。
ONは2001×2001の`uint16_t`表（約8MB）を作る。前計算時間も探索予算に含む。
温度120→1、乱数seed20211115、近傍比率40/30/25/5%、初期解、時計間隔64は旧Runnerと同じ。
温度・近傍自体の追加チューニングはしていない。

## 正しさ

- 汎用移動・交換差分を、非対称距離・隣接・同一位置・long long/doubleで全計算と照合。
- 2点挿入を全組合せと照合し、同点の辞書順・長さ2・非対称距離も検査。
- ランダム2万近傍について、独立に作った候補経路、全距離、全先行制約、逆引き位置、
  選択注文を照合。前計算ON/OFFの両方を通常テストとASan/UBSan対象にしている。
- 公式seed 0–9で1万反復。旧版・差分版・前計算版の**全試行の評価・採否・経路のhash**と
  最終出力が一致。[診断CSV](results/ahc006_delta_fixed.csv)の30実行は採否の得点比較には使わない。
- 時間制限実行は全出力を独立Python検証と公式Rust `vis`の両方で採点する。
  別注文が座標を共有する場合も、最初のpickup訪問と最後のdelivery訪問で判定する。

## 比較条件

- 比較元commit: `9eb7c90011bec7c5bc8cde4e7447d78af295e2ac`。
- `full_copy`: 旧Runner例。入力・初期化も含む1850msに計時だけ揃え、回数をstderrへ追加。
- `delta`: 差分版、距離表なし。`delta_table`: 同じ差分版、距離表あり。
- `legacy_practice`: 旧practiceをそのまま凍結した参考比較。主比較は設定の揃った`full_copy`。
- Intel Core i5-12400F / WSL2、g++ 13.3.0、`-std=c++17 -O2 -DNDEBUG -Wall -Wextra`。
- 全版を先にコンパイルし、1本ずつ順序を巡回して実行。重いビルドや別solverを並走しない。
- 公式配布zipの入力を使う。開発seed 0–9、未使用の確認seed 20–39、各2 repeat。
  確認前に距離表ONを採用候補へ固定し、確認結果による再調整はしない。
- 主指標: 入力ごとに全比較版・全repeatの時間内合法runの共通bestを取り、
  `100 * score / best`を計算。repeat平均を入力数分足す。診断は分母から除く。
  同一入力の他の既存AHC006 CSVはなかった（旧READMEの平均だけでは分母を復元できない）。
- 不正・2秒超過も記録し相対点は0。生得点平均・探索回数・勝敗は副指標。
  実行時間はプロセス起動・終了待ちも含む手元の壁時計であり、提出環境の保証ではない。
- この共通best比は公式1位との比率ではない。上位者との同一入力・同一環境比較は未実施。

公式ツールは[配布zip](https://img.atcoder.jp/ahc006/c21daebb77aa4d38d65f4d7f7c7249.zip)を
Cargo.lockを変えずに`cargo build --release --locked`で構築した。

| 対象 | SHA-256 |
|---|---|
| zip | `bb740c7169e230691b5fa6de581834be322dce40b0ff0b4ea4651dcbf5cd5ffc` |
| Cargo.lock | `49b65f97d914a6b8f12d6bc3f3b6cb0152c2cdf5d0cd09a8391eb1f67d856cad` |
| vis | `fe9720f0e0b648790b91f91a2c46dc7ead84b843d9f12f6cccb160cd1df5bba0` |

## 開発10入力×2回

[生CSV](results/ahc006_delta_dev.csv)、80実行。

| 版 | 平均公式得点（副指標） | 共通best比 / 1000 | 平均率 |
|---|---:|---:|---:|
| full_copy | 19567.20 | 882.713150 | 88.271315% |
| delta | 21564.30 | 972.718761 | 97.271876% |
| delta_table | 21665.50 | 977.384365 | 97.738436% |
| legacy_practice | 19180.05 | 865.516201 | 86.551620% |

全80出力合法、採点一致、2秒超過0。最大1.8692秒。
探索回数中央値は旧Runner1155328、差分13831200、距離表あり20757184。
この回数だけで採用せず、確認入力の得点で判断する。

## 設定固定後の未使用20入力×2回

[生CSV](results/ahc006_delta_confirm.csv)、160実行。
開発入力と入力SHAの重複0件。下表は全版・全repeat共通の分母で集計した。

| 版 | 平均公式得点（副指標） | 共通best比 / 2000 | 平均率 |
|---|---:|---:|---:|
| full_copy | 19328.625 | 1728.043005 | 86.402150% |
| delta | 21589.725 | 1929.762174 | 96.488109% |
| delta_table | 21916.900 | 1958.837646 | 97.941882% |
| legacy_practice | 19439.600 | 1737.781958 | 86.889098% |

全160出力合法、独立採点と公式採点一致、2秒超過0（最大1.8682秒未満）。
距離表ありの差分版は旧Runnerに相対点**+230.794641 / 2000**、平均率**+11.539732ポイント**。
生得点平均の改善は約13.39%（副指標）。距離表なしでも相対点+201.719169。
差分化単体の改善と、距離表の追加効果を分けて確認できた。

この結果から、開発時に選んだ`delta_table`をpracticeの既定へ採用する。
前計算OFFも残し、問題依存の先行制約や距離表を汎用Runnerへ持ち込まない。
確認入力で温度・近傍を再調整していない。全240時間制限実行に失敗・時間超過なし。
スコア改善を確認したのはこの環境・この入力群であり、別問題への汎用的な得点改善を保証しない。

## 再現

```sh
python3 benchmarks/ahc006_delta_benchmark.py \
  --inputs /path/to/official-tools/in --tool /path/to/official-tools/target/release/vis \
  --first-seed 20 --cases 20 --repeats 2 \
  --variants full_copy delta delta_table legacy_practice \
  --artifacts /tmp/ahc006-confirm-new --output /tmp/ahc006-confirm-new.csv \
  --history benchmarks/results/ahc006_delta_dev.csv benchmarks/results/ahc006_delta_confirm.csv
```

`--iterations 10000 --cases 10 --first-seed 0 --repeats 1`を指定すれば軌跡一致診断。
その場合のvariantsは`full_copy delta delta_table`だけにする。
出力先が存在すると上書きせず停止する。各runの入力・出力・source・toolのSHA、
コンパイラ、フラグをCSVへ記録し、展開済みsource・stdout/stderr・採点結果をartifactsへ保存する。
一般的な辺差分・逆引き位置・線形挿入の考え方から独自実装しており、第三者提出のコードは移植していない。
