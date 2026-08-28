# ahc-precontest-kit

AHC のコンテスト中に、自分の `main.cpp` へ直接コピーして使う C++ パーツ集です。

## 使い方

コンテスト中に編集するファイルは `main.cpp` だけです。

1. 下の一覧から必要なパーツを開く。
2. ファイル全体をコピーする。
3. 自分の `main.cpp` の `main` 関数より上へ貼り付ける。
4. そのまま `main.cpp` だけをコンパイル・提出する。

ローカルファイルを参照する手順、生成スクリプト、実行時ファイルは必要ありません。
各パーツは標準ライブラリの依存も含め、ファイル全体を単独で貼れる形にします。
初心者がそのまま読めるように、カスタム `namespace` や複雑な共通基盤は使わず、
原則として 1 ファイルを 1 機能だけにします。

考え方は [Luzhiled's Library](https://ei1333.github.io/library/) のような、必要な実装を
探して自分のコードへ取り込める競技プログラミング用ライブラリを参考にしています。

## まずはこの4つ

初めて使うなら、次の4ファイルだけ見れば十分です。

- [`timer.hpp`](library/timer.hpp) — 時間切れ判定と進捗率
- [`random.hpp`](library/random.hpp) — 整数・小数・shuffle・重み付き抽選
- [`time-based-simulated-annealing.hpp`](library/time-based-simulated-annealing.hpp) —
  タイマー内蔵の焼きなまし
- [`best-keeper.hpp`](library/best-keeper.hpp) — 今までで一番良い解を保存

各ファイルの先頭にも、短い使い方例を書いてあります。もう少し長い例は
[`USAGE.md`](USAGE.md) にあります。

## パーツ一覧

### 時間・乱数・小道具

| ファイル | できること |
|---|---|
| [`timer.hpp`](library/timer.hpp) | 経過時間・残り時間・進捗率 |
| [`random.hpp`](library/random.hpp) | 型を選べる乱数、ランダム選択、重み付き選択 |
| [`chmin-chmax.hpp`](library/chmin-chmax.hpp) | 値が良くなる時だけ更新 |
| [`schedule.hpp`](library/schedule.hpp) | 時間経過に合わせて値を変化 |
| [`best-keeper.hpp`](library/best-keeper.hpp) | best score と best state を保存 |
| [`top-k.hpp`](library/top-k.hpp) | 良い候補を上位 K 個だけ保存 |

### 探索

| ファイル | できること |
|---|---|
| [`simulated-annealing.hpp`](library/simulated-annealing.hpp) | 外部から進捗率を渡す焼きなまし |
| [`time-based-simulated-annealing.hpp`](library/time-based-simulated-annealing.hpp) | タイマー内蔵の焼きなまし |

### データ構造

| ファイル | できること |
|---|---|
| [`cumulative-sum.hpp`](library/cumulative-sum.hpp) | 1次元累積和 |
| [`cumulative-sum-2d.hpp`](library/cumulative-sum-2d.hpp) | 2次元累積和 |
| [`coordinate-compression.hpp`](library/coordinate-compression.hpp) | 座標圧縮 |
| [`dsu.hpp`](library/dsu.hpp) | Union-Find |
| [`rollback-array.hpp`](library/rollback-array.hpp) | 変更を過去の状態へ戻せる配列 |
| [`rollback-dsu.hpp`](library/rollback-dsu.hpp) | 過去の状態へ戻せる Union-Find |
| [`stamp-array.hpp`](library/stamp-array.hpp) | ほぼ O(1) で初期化し直せる配列 |

`int` 固定である必要がないパーツはテンプレートにしています。たとえば、次のように
得点は `double`、解は `vector<int>` のように自由に選べます。

```cpp
BestKeeper<double, vector<int>> best(initial_score, initial_answer);
CumulativeSum<long long> sum(values);
RollbackArray<string> names(initial_names);
```

各 `.hpp` は別のライブラリファイルを参照しません。必要なファイルだけを丸ごと
コピーできます。貼り付け後はカスタム名前空間を付けず、そのまま使えます。

GitHub 上ではファイルを開き、右上のコピーアイコン、または Raw 表示から全体を
コピーしてください。

## 最小テンプレート

[`template/main.cpp`](template/main.cpp) は、パーツを貼る位置だけを示す最小構成です。
テンプレートにもローカルファイルへの依存はありません。

```cpp
#include <bits/stdc++.h>
using namespace std;

// 必要な library/*.hpp の中身をここへ貼る。

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  // 問題固有の処理を書く。
}
```

## ライブラリ追加時の約束

- ファイル全体を `main.cpp` の上部へ貼るだけで使えること。
- カスタム `namespace` や難しい共通基盤を使わないこと。
- ローカルファイルへの `#include "..."` を含めないこと。
- 外部パッケージや実行時ファイルに依存しないこと。
- 問題固有の `main` 関数や入出力を含めないこと。
- 使用する標準ヘッダをパーツ自身に記載すること。
- 最小のテストを追加すること。

## リポジトリ側の検証

パーツ集を更新するときだけ、次を実行します。コンテスト中の利用には不要です。

```bash
make verify
```
