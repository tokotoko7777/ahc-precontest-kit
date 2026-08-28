# ahc-precontest-kit

AHC で繰り返し使う小さな C++ 部品を、提出コードへ取り込むためのライブラリです。

## 最重要: 提出するのは 1 ファイル

AtCoder からこのリポジトリを参照することはできません。開発中の
`#include "library/..."` は整理のための記述であり、提出前に必ず中身を展開します。

使い方は次のどちらかです。

1. 必要な `.hpp` の中身を自分の `main.cpp` へコピーする。
2. `tools/bundle.py` でローカル `#include` を再帰的に展開し、単一の
   `submission.cpp` を作る。

手作業のコピー漏れを避けられるため、通常は 2 を推奨します。考え方は
[Luzhiled's Library](https://ei1333.github.io/library/) の、機能ごとにヘッダを分けて
検証し、提出時に Bundle する構成を参考にしています。

## ディレクトリ

```text
library/ahc/          コピー可能なヘッダライブラリ
examples/             利用例
tests/                ライブラリと Bundle の検証
tools/bundle.py       ローカル include を展開する単一ファイル化ツール
```

各ヘッダは、単体で `main.cpp` に貼り付けても使えるようにします。外部パッケージや
実行時ファイルには依存せず、標準ライブラリと `library/` 内の明示された依存だけを
使用します。

## Bundle の使い方

このリポジトリ内で開発する場合:

```bash
python3 tools/bundle.py examples/basic.cpp -o submission.cpp
g++ -std=c++17 -O2 -Wall -Wextra -pedantic submission.cpp
```

別のコンテスト用ディレクトリに `main.cpp` がある場合:

```bash
python3 /path/to/ahc-precontest-kit/tools/bundle.py \
  /path/to/contest/main.cpp \
  -I /path/to/ahc-precontest-kit \
  -o /path/to/contest/submission.cpp
```

`submission.cpp` には `#include "..."` が残りません。このファイルだけを提出します。
標準ライブラリの `#include <...>` はそのまま残ります。

## 収録済み

- [`library/ahc/timer.hpp`](library/ahc/timer.hpp): `steady_clock` ベースの時間管理
- [`library/ahc/random.hpp`](library/ahc/random.hpp): 再現可能な高速乱数生成

## 検証

```bash
make verify
```

ライブラリの単体テスト、Bundle 後の「ローカル include が残っていないこと」、および
生成した単一 C++ ファイルのコンパイルを確認します。

## ライブラリ追加時の約束

- 1 機能を 1 つの `.hpp` にまとめる。
- ヘッダ単体で貼り付け可能にする。
- `main` 関数や問題固有の入出力をライブラリへ入れない。
- 依存は標準ライブラリ、または `library/` 内の相対 include に限定する。
- API の最小利用例とテストを追加する。
