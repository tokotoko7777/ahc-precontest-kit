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

考え方は [Luzhiled's Library](https://ei1333.github.io/library/) のような、必要な実装を
探して自分のコードへ取り込める競技プログラミング用ライブラリを参考にしています。

## Library Files

### Utility

- [`library/timer.hpp`](library/timer.hpp) — 時間制限を管理する `ahc::Timer`
- [`library/random.hpp`](library/random.hpp) — 再現可能な高速乱数生成器 `ahc::Random`

### Heuristic

- [`library/simulated-annealing.hpp`](library/simulated-annealing.hpp) —
  温度計算と遷移受理を担当する `ahc::SimulatedAnnealing`

各 `.hpp` は別のライブラリファイルを参照しません。たとえば焼きなましだけが必要なら
`simulated-annealing.hpp` だけをコピーできます。時間ベースで進捗を管理したい場合は、
`timer.hpp` もコピーして `Timer::progress()` の値を渡します。

GitHub 上ではパーツのファイルを開き、右上のコピーアイコン、または Raw 表示から
全体をコピーしてください。

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
