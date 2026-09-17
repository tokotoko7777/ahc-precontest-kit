# AHC071 練習解答: 行DP + Actionビーム + 区間再構築

公式問題: [A - Wall Making](https://atcoder.jp/contests/ahc071/tasks/ahc071_a)

この実験ブランチのメモリ再利用版は**未採用・マージ保留**です。
問題側を変えずに共通コアを軽くしましたが、追加30ケース×3回は同幅の旧版と全て同点でした。
速度や探索回数だけでは強化済みとせず、[検証レポート](../../benchmarks/AHC071_ALLOCATION_REPORT.md)に残しています。

[`main.cpp`](main.cpp)は、ローカルheaderへ依存しない提出可能な単一ファイルです。
読みやすい開発元は
[`examples/search/ahc071_action_beam.cpp`](../../examples/search/ahc071_action_beam.cpp)
で、次の公開済みパーツを先頭へ埋め込んであります。

- `action-beam-search.hpp`: 上位Action選抜、State生成、重複除去、履歴復元
- `simulated-annealing.hpp`: 区間再構築を受理する焼きなまし

各パーツのinclude直後には公開元URLがあります。提出時に別ファイルを参照するわけではなく、
この1ファイルだけでコンパイルできます。このブランチの試作を使う場合は、公開済みの
実験commitを指定して生成し、mainに採用済みのコードとは区別してください。

## 問題を短く言うと

穴の空いた壁を、幅1・3・5・7・9のレンガで覆います。レンガには幅ごとの費用があり、
同じ段では重ねられません。2段目以降のレンガは、中心の真下に別のレンガが必要です。
全ての穴を覆い、レンガ費用を小さくするのが目的です。

上から下へ考えると、次の段へ渡す情報は「直上段にあるレンガの中心位置」だけです。
盤面全体をStateへ入れず、このbitsetだけを持つのが実装の中心です。

## 人が問題に合わせて書く部分

`TODO(AHC071)`を検索すると、主な編集箇所を順番に読めます。

1. `row_dp`: 1段の必須マスを覆う最小費用DP
2. `enumerate_rows`: 最良だけでなく、ビームへ渡す上位の行候補を列挙
3. `RowBeamProblem::State`: 次段へ渡す中心bitset、累積費用、完成行
4. `generate_actions`: 次の1段として置ける候補を作る
5. `evaluate_action_with_threshold`: 現在の上位境界を超えない候補を途中で枝刈り
6. `make_key`: 次段から見て同じ状態を1件へまとめる
7. `apply_action`: 選抜されたActionだけをStateへ反映
8. `rebuild`: 一部の段を固定し直してActionビームで再構築

## ライブラリが担当する部分

`ActionBeamRunner<RowBeamProblem>`は次を担当します。

- 全候補のStateを作る前に、軽いActionと差分評価だけで上位N件を選ぶ
- 同じ中心bitsetへ到達する候補を、最も安い1件だけ残す
- 完成済み解の費用を閾値として、勝てない候補の評価を打ち切る
- 採用したActionだけStateをコピーし、最終履歴を復元する

問題側は行DP・候補生成・費用・状態更新を書き、上位選抜の実装は通常変更しません。

## 探索の流れ

最初に貪欲解を作り、それを費用上限として全段をActionビームで構築します。その後は、
連続する数段を選んで再構築するLNSを行います。局所的に費用が悪化する再構築も
`SimulatedAnnealing`で時々受理し、同じ構造へ固定されるのを避けます。

高速化の要点は次の通りです。

- 幅60以下の行を`uint64_t`で保持
- 1行DPは固定長配列
- 行候補を最大数までに制限
- Actionを選抜してからStateをコピー
- 同一中心bitsetを重複除去
- 完成解より高くなると確定した候補を閾値枝刈り

## コンパイル

```bash
g++ -std=c++17 -O3 -DNDEBUG practice/ahc071/main.cpp -o solver
```

標準では約1.8秒探索します。短い動作確認ではコンパイル時に変更できます。

```bash
g++ -std=c++17 -O2 -DAHC071_TIME_LIMIT=0.05 \
    practice/ahc071/main.cpp -o smoke
```

## 公式入力での確認

公式seed 0000〜0009、各1.8秒で、全10ケースの出力が合法であることを確認しています。

| 実装 | 平均score | 合計 |
|---|---:|---:|
| kitのフォーマット版 | 10,652.30 | 106,523 |
| 参考`main3.cpp` | 10,652.50 | 106,525 |

kit版から見た勝敗は2勝4分4敗です。両方とも壁時計で探索を止めるため、再実行では数点
変動することがあります。ほぼ同点ですが、上位者相当を一般に超えると主張する
比較ではなく、指定された参考実装と同じ公式10入力・同じ時間での回帰測定です。

公式ツールがある場合は次で再現できます。

```bash
python3 benchmarks/ahc071_official_benchmark.py \
  --tools /path/to/AHC071 \
  --solver practice/ahc071/main.cpp \
  --reference /path/to/AHC071/main3.cpp \
  --cases 10
```

Rust visualizerを使えない環境では`--ported-score`で、公式`src/lib.rs`から移植した
合法性・得点検査を使用できます。

## 開発用と提出用の関係

普段変更するのは`examples/search/ahc071_action_beam.cpp`の`TODO(AHC071)`部分です。
CIではexampleとheaderを展開した全文が、単一ファイルと一致することを検査します。
公開済みcommitに固定した出典付きの提出コードが必要な場合は、次のように生成できます。

```bash
python3 tools/copy_part.py --ref <公開済みSHA> \
  --main examples/search/ahc071_action_beam.cpp \
  library/action-beam-search.hpp library/simulated-annealing.hpp \
  -o build/ahc071-submit.cpp
```
