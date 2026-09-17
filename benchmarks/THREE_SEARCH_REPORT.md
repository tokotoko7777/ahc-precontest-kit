# chokudai / UCT / ILSを実問題で試す

追加したのは、コピー用hpp、TODO付き穴埋め、過去AHCの完成例、公式採点harness。
探索方法を増やす実験であり、「追加した3方式が従来方式より強い」とはしない。
AHC069は今回の対象外。提出は行っていない。

| 方式 | hpp | 穴埋め | 完成例 |
|---|---|---|---|
| chokudai | [本体](../library/chokudai-search.hpp) | [TODO](../template/search/chokudai-search.cpp) | [AHC032](../examples/search/ahc032_chokudai.cpp) |
| UCT | [本体](../library/monte-carlo-tree-search.hpp) | [TODO](../template/search/monte-carlo-tree-search.cpp) | [AHC015](../examples/search/ahc015_uct.cpp) |
| ILS | [本体](../library/iterated-local-search.hpp) | [TODO](../template/search/iterated-local-search.cpp) | [AHC059](../examples/search/ahc059_ils.cpp) |

APIと編集場所は[SEARCH_GUIDE](../SEARCH_GUIDE.md#chokudai--uct--ils)。
着想は[chokudai氏の原記事](https://chokudai.hatenablog.com/entry/2017/04/12/055515)、
[UCTの原論文](https://doi.org/10.1007/11871842_29)、
[ILSの原著者による総説](https://arxiv.org/abs/math/0102188)。考え方から独自実装し、
問題側の盤面・修復処理はこのkitの既存例を再利用した。第三者の提出コードは同梱しない。

## 比較条件

- 旧版はcommit `a2939214735b5b70a220955f19c62909b3cf8d0d`のpracticeを凍結。
- CPU: Intel Core i5-12400F / WSL2。g++ 13.3.0、`-std=c++17 -O2 -DNDEBUG -Wall -Wextra`。
- 1本ずつ実行し、入力番号とrepeatで順序を巡回。全版を先にコンパイルする。
  計時中は別solver・重いビルドを並走しない。
- AHC032: 同じスタンプ候補・順位評価・手数配分。既存ビームは幅6000/9000/12000、
  chokudaiは層容量256/1024を比較。chokudaiは前処理込み1.85秒枠、合法なgreedyをfallbackにする。
  ビームは固定幅を最後まで処理する。両方式に同じ2秒上限を課す。
- AHC015: UCTとflat MCは同じ盤面、終局の軽い方策、評価、合計1.75秒枠。
  flatは同じ未来の乱数シナリオを4手へ渡す。UCTは結果ごとに枝を分けるclosed-loop。
  `legacy`は旧practiceそのまま（約1.0秒）で、**対等な主比較はflatとUCT**。
- AHC059: 同じペア回収表現・O(n)挿入・距離表・安全な閾値打ち切り。
  既存LNS+SAは区間15、ILSは区間30で摂動して区間4で局所改善。両方1.85秒枠。
  これは解法/近傍の比較であり、同一アルゴリズムの純粋な速度比較ではない。
- 全出力を独立Python再生と公式Rust採点で照合。AHC015は各ターンの出力を受け取って
  から次の配置順位を渡す対話形式で、未知の配置を先読みできない。
- 不正・2秒超過もCSVに残し、相対評価では0点。超過runをbestの候補へ入れない。
  終了待ちや対話の費用を含む手元の壁時計であり、AtCoderの実行時間保証ではない。
- 主指標は入力ごとの共通bestに対する `100 * score / best`。
  同じ入力の全比較版・全repeat・既存CSVの有効な実時間runを分母へ含める。
  2 repeatの相対点を平均して20入力分を合計し、2000点満点と平均率で示す。
  生得点平均・勝敗・探索回数は副指標。問題間で得点を合算しない。

公式問題・配布ツールへの入口:
[AHC032](https://atcoder.jp/contests/ahc032/tasks/ahc032_a)、
[AHC015](https://atcoder.jp/contests/ahc015/tasks/ahc015_a)、
[AHC059](https://atcoder.jp/contests/ahc059/tasks/ahc059_a)。
Cargo.lockを保ち`cargo build --release --locked`で構築したツールを使用。

| 問題 | gen SHA-256 | vis SHA-256 |
|---|---|---|
| 032 | `4f9c49ce44e87fedcc3ce42062a2097f3cc183d65cc11704febb33bcc99fc8d2` | `08ccc6190df8e65d1d31fa4cb16b983c4f63ef8773c55b7b98630236c50af1f1` |
| 015 | `d111462e7d99b307864ac525ec4af2cc6988b0ea78366be1fc869d2d3d334000` | `c51d4527a003d63e6687563810a819fbe4d33ffa41cc3438462825853c7eaef3` |
| 059 | `61e77e03c950b0f978807d797f962cc16c5be748c5abcaf2128c83c297d22dd3` | `bafe215694f111c4131022aa2d981aff913e142a619086dc9b0c83a25faef8af` |

## 開発入力: 公式seed 0–4

以下は既存CSVの同一入力のbestも含めて再集計した値。生CSVへ記録した初回実行直後の
集計より分母が上がる場合がある。固定反復診断は除外する。

### AHC032

[CSV](results/ahc032-chokudai-dev-5.csv)、25実行。

| 版 | 平均公式得点 | 共通best比 / 500 | 平均率 |
|---|---:|---:|---:|
| beam6000 | 79155159178.2 | 499.857615 | 99.971523% |
| beam9000 | 79174176869.2 | 499.977751 | 99.995550% |
| beam12000 | 79174176869.2 | 299.977751 | 59.995550% |
| chokudai256 | 79103245707.4 | 499.529761 | 99.905952% |
| chokudai1024 | 79099629864.8 | 499.507125 | 99.901425% |

beam12000は2/5実行が2秒超過（最大2.217秒）。生平均が同じでも採用しない。
chokudaiは容量256を確認候補に固定。候補集合・評価関数は変更していない。

### AHC015

[初回CSV](results/ahc015-uct-dev-5.csv)、25実行。

| 版 | 平均公式得点 | 共通best比 / 500 | 平均率 |
|---|---:|---:|---:|
| legacy | 565406.0 | 327.649618 | 65.529924% |
| flat | 830122.8 | 480.695607 | 96.139121% |
| uct1 | 768512.0 | 445.040304 | 89.008061% |
| uct8 | 656419.4 | 380.349513 | 76.069903% |
| uct32 | 592292.0 | 343.603812 | 68.720762% |

[追加調整CSV](results/ahc015-uct-tune-5.csv)、20実行。
`ordered`は未試行の手を軽い方策順で試し、`low`は探索係数を0.3→0.05にしたもの。
この前に、最大深さ直後の未使用ノードを作らない軽量化を入れた。

| 版 | 平均公式得点 | 共通best比 / 500 | 平均率 |
|---|---:|---:|---:|
| flat（再測定） | 742604.0 | 431.239907 | 86.247981% |
| uct8_ordered | 596273.6 | 346.012847 | 69.202569% |
| uct8_low | 565366.0 | 328.555336 | 65.711067% |
| uct1_low | 712817.6 | 413.883694 | 82.776739% |

flatの同一ソースでも再実行差が大きい。時間打ち切りでサンプル数・以後の盤面が変わり、
5入力1回の小差は安定した改善とみなせない。調整版もflatを上回らないため、
確認では元の順序・係数0.3を固定し、深さ1と8を別々に残した。
**深さ1は根のバンディット**で、MCTSの深い木が効いた証拠にはしない。

### AHC059

[CSV](results/ahc059-ils-dev-5.csv)、20実行。

| 版 | 平均公式得点 | 共通best比 / 500 | 平均率 |
|---|---:|---:|---:|
| legacy（LNS+SA） | 15355.6 | 499.817662 | 99.963532% |
| ils16 | 15338.2 | 499.251332 | 99.850266% |
| ils64 | 15349.8 | 499.628855 | 99.925771% |
| ils_walk | 15335.8 | 499.173369 | 99.834674% |

16/64は局所探索の連続非改善回数。walkは64かつ外側で悪化も受理する。
確認はils64、悪化棄却、20回最良更新なしで最良解へ戻る設定に固定した。

## 設定固定後の未使用20入力×2回

3問題それぞれの公式genに**seed 1000–1019**を渡して新しく生成した。
CSVの`seed=0..19`はファイル番号であり、生成seedは`1000 + seed`。
入力SHAを既存結果と照合し、各20入力とも過去CSVとの重複0件を確認した。
この結果を使った追加チューニングはしない。

### AHC032: 80実行

[CSV](results/ahc032-chokudai-confirm-20x2.csv)。

| 版 | 平均公式得点 | 共通best比 / 2000 | 平均率 |
|---|---:|---:|---:|
| beam9000 | 79162472144.0 | 1999.570909 | 99.978545% |
| chokudai256 | 79079397319.6 | 1997.474638 | 99.873732% |

全80出力合法、独立再生と公式採点一致、2秒超過なし。
chokudaiは相対点−2.096271 / 2000。既存ビームを置き換えない。
ビームの最大1.967秒、chokudaiの最大1.868秒で、提出環境の時間内動作を保証する値ではない。

### AHC015: 160実行

[CSV](results/ahc015-uct-confirm-20x2.csv)。

| 版 | 平均公式得点 | 共通best比 / 2000 | 平均率 |
|---|---:|---:|---:|
| legacy | 575560.350 | 1350.093668 | 67.504683% |
| flat | 808164.625 | 1884.342754 | 94.217138% |
| uct1（根のバンディット） | 802693.525 | 1872.845122 | 93.642256% |
| uct8（深いMCTS） | 602484.625 | 1407.132293 | 70.356615% |

全160出力合法、採点一致、2秒超過なし（最大1.754秒未満）。
UCT1はflatに相対点−11.497632 / 2000、UCT8は−477.210461 / 2000。
旧practiceを上回ることだけでMCTSの効果とせず、方策・時間の揃ったflatを基準にする。
深いMCTSへの置き換えは見送る。今回のUCT1とflatの小差を普遍的な優劣とはしない。
flatの方策/時間配分による旧practice改善は、MCTSの改善と分離して扱う。

確率分岐が多いAHC015では観測結果ごとの試行数が薄くなること、木の未試行手が
rolloutの軽い方策より弱い可能性が考えられる。ただしこれは原因の**推測**であり、
今回の得点比較だけで寄与を特定したものではない。ノード共有や木の使い回しも未実装。

### AHC059: 80実行

[CSV](results/ahc059-ils-confirm-20x2.csv)。

| 版 | 平均公式得点 | 共通best比 / 2000 | 平均率 |
|---|---:|---:|---:|
| legacy（LNS+SA） | 15362.775 | 1999.521823 | 99.976091% |
| ils64 | 15356.800 | 1998.743751 | 99.937188% |

全80出力合法、採点一致、2秒超過なし（最大1.868秒未満）。
ILSは相対点−0.778072 / 2000、生平均−5.975点。既存LNS+SAを置き換えない。

## 採否

3方式とも独立パーツ・穴埋め・完成例として追加するが、**既存practiceの標準解は変更しない**。
新手法の動作確認と、既存の強い解に対する改善確認は分ける。
開発90実行と確認320実行の計410実行は全出力合法。開発時の幅12000ビームだけ2回時間超過。
確認320実行は全て時間内で、失敗した開発runも消していない。

副指標の勝敗は入力ごとに2 repeatの生得点を平均して比較したもの:
chokudai対ビームは5勝2分13敗、UCT1対flatは11勝0分9敗、UCT8対flatは0勝0分20敗、
ILS対LNS+SAは6勝1分13敗。勝ったケース数だけでも採用しない。
共通bestは今回のローカル比較に使う尺度であり、公式順位や1位との比率ではない。

## 再測定

各公式ツールの`gen`へ、1000から1019まで1行1整数を並べた`seeds.txt`を渡す。
出力先は問題ごとに新しい作業ディレクトリを使い、既存の`in/`を上書きしない。
例: 新しい作業ディレクトリ内で `/path/to/tools/target/release/gen seeds.txt`。
CSVとartifactも未使用のパスを指定する。

```sh
python3 benchmarks/three_search_benchmark.py --task 032 \
  --inputs /path/to/fresh032/in --tool /path/to/ahc032/tools/target/release/vis \
  --variants beam9000 chokudai256 --cases 20 --repeats 2 \
  --artifacts build/three032 --output build/three032.csv
python3 benchmarks/three_search_benchmark.py --task 015 \
  --inputs /path/to/fresh015/in --tool /path/to/ahc015/tools/target/release/vis \
  --variants legacy flat uct1 uct8 --cases 20 --repeats 2 \
  --artifacts build/three015 --output build/three015.csv
python3 benchmarks/three_search_benchmark.py --task 059 \
  --inputs /path/to/fresh059/in --tool /path/to/ahc059/tools/target/release/vis \
  --variants legacy ils64 --cases 20 --repeats 2 \
  --artifacts build/three059 --output build/three059.csv
```

初回は比較全版・全repeatが共通分母。再測定では対応する今回の確認CSVを
`--history benchmarks/results/ahc...-confirm-20x2.csv`で渡し、過去bestも含める。
入力・公式ツール・バイナリはリポジトリに同梱しない。
CSVには入出力・ヘッダ展開済みソース・採点器のSHA、比較元commit、flagsを保存。
全展開ソースとstdout/stderr・採点ログは実行時のartifactディレクトリに残す。

## 回帰テスト

`tests/three_search_upgrades_test.cpp`は、chokudaiの順位値と完成得点の分離、容量制限、
終端初期解、未完成解、最大化/最小化、非有限値、UCTの確率分岐・再確保・ノード/深さ制限、
空の合法手、0反復、seed再現性、不正報酬、ILSの採否・再開・0反復・NaNを検査する。
`make verify`と`make verify-sanitize`の通常対象に含めた。
対話harnessのテストでは、未来の配置を読むまで出力しないsolverを意図的に止め、
将来の実入力が漏れていないことを確認する。
実問題の出力合法性・得点は別途、上記の公式ツールと独立再生で確認する。
