# AHC058 — 先読み初期解＋購入順序の焼きなまし

[公式問題](https://atcoder.jp/contests/ahc058/tasks/ahc058_a)は4時間の短期コンテストです。
`main.cpp`だけでコンパイル・提出できます。別のhppは不要です。

## 読む場所・編集する場所

ヘッダ分離版は[`ahc058_prefix_sa.cpp`](../../examples/search/ahc058_prefix_sa.cpp)。
汎用の空欄入り雛形は[`prefix-replay-annealing.cpp`](../../template/search/local-search/prefix-replay.cpp)です。

`BEGIN LIBRARY`〜`END LIBRARY`は通常編集しません。
`TODO(AHC058)`以降に初期解用の`ProductionProblem`、
`TODO(AHC058-SA)`以降に焼きなまし用の`PurchaseSequenceProblem`をまとめています。

| 編集するもの | 書く内容・返す値 |
|---|---|
| `State.actions` | 待機を除いた購入順序。`level * 10 + id`で1購入を表す |
| `propose_move` | 現在列を変更した候補。作れなければ`nullopt` |
| `advance_purchase` | 1購入を最速で実行。資金不足なら必要ターンだけ待つ |
| `score` | 最終りんご数から公式の「大きいほど良い」得点を返す |
| `evaluate_move` | 候補得点−現在得点。仮cacheを作るが現在解は変更しない |
| `apply_move` | 採用時だけ候補列と仮cacheを確定する |
| `decode` | 購入順序から合法な500行の実行命令へ変換する |

共通処理は3つに分かれます。

- `DeterministicRolloutRunner`: 初期解の候補比較。
- `PrefixReplay`: 変更前の共通区間を再利用し、変更後の区間だけ再生。
- `TimeBasedAnnealingRunner`: 温度、採否、最良解保存、時間切れ判定。

## 解法

1. 従来と同じ3手先読みで500ターンの初期解を作る。2手目は上位12候補＋各Level代表。
2. 待機を除き「どの機械を、どの順に購入するか」の列へ変換する。
3. 交換・移動・変更・挿入・削除、同じIDの4段階まとめ挿入を試す。
4. 購入列を再生し、公式得点 `round(100000 * log2(apples))` で焼きなます。
5. 最良列を復号する。厳密なりんご数で初期解より悪ければ初期解を出力する。

最初の安いLevel 0購入は変更しません。購入できないときは必要なだけ待ち、
終了まで買えなければ後続の購入も行いません。必ず500行を出力します。

### 差分再生と待機の高速化

購入8個おきに状態を保存します。候補と現在列の最初の相違点を探し、
その直前の保存状態からだけ再計算します。候補不採用で現在cacheは壊れません。
cacheは`State`でなく`Problem`に置くので、最良解保存のたびにはコピーしません。

購入のない期間の生産量は `C(t,1)..C(t,4)` の4項で計算できます。
この式で必要待機ターンを二分探索し、機械数も閉形式でまとめて進めます。
公式の生産順と一致することを、独立した1ターンずつの再生とテストしています。
りんご数・機械数の計算には128bit整数を使います。

## 実行

```sh
g++ -std=c++17 -O2 -DNDEBUG main.cpp -o main
./main < in.txt > out.txt
```

既定は初期解構築込み1,850ms、温度2,500→5、乱数seed 58です。
時間で終了するので、同じseedでも実行負荷によって反復数と出力は変わります。
`AHC058_SA_ITERATIONS`は固定反復の回帰検査専用で、提出時は指定しません。

## 公式スコア比較

2026-09-15、同じ公式配布seed 0〜99を従来3手先読みと比較しました。
最初の10ケースを確認後、設定を変更せず残り90ケースを測定しています。

| 入力 | 従来平均 | 新版平均 | 新版の勝/分/敗 |
|---|---:|---:|---:|
| seed 0〜9 | 5,192,506.70 | 5,481,894.70 | 10/0/0 |
| seed 10〜99 | 5,166,728.44 | 5,492,770.44 | 90/0/0 |
| 全100ケース | 5,169,306.27 | **5,491,682.87** | **100/0/0** |

全体で平均約6.24%改善、全出力を公式visualizerで採点・合法性確認しました。
新版の最大壁時計は1.852秒でした。測定は1回ずつで、提出環境の時間保証や
公式順位の主張ではありません。大きなビルドや他のsolverは並行させていません。

[開発10ケースCSV](../../benchmarks/results/ahc058-prefix-dev-10.csv)と
[追加90ケースCSV](../../benchmarks/results/ahc058-prefix-holdout-90.csv)に生スコア・
実行時間・入力/出力/ソースのSHA-256を保存しています。

```sh
python3 benchmarks/ahc058_annealing_benchmark.py \
  --inputs /path/to/ahc058/tools/in --tool /path/to/ahc058/tools/target/release/vis \
  --cases 100 --output build/ahc058-prefix-new.csv
```

## 参考にした解説と残る課題

[公式解説](https://img.atcoder.jp/ahc058/editorial.pdf)の購入順序SA、
貪欲・ビームとの組合せを参考に、独自実装しました。第三者の提出コードはコピーしていません。
購入先を段階的に絞る探索や、強い構築ビームとの組合せはまだ未実装です。
今回の改善を、上位解に到達したという意味には扱いません。

従来の決定的3手先読み版も
[`ahc058_deterministic_rollout.cpp`](../../examples/search/ahc058_deterministic_rollout.cpp)
に残しています。これだけを使う場合は時間や乱数によらず同じ答えになります。
