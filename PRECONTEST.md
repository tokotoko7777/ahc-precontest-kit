# コンテスト前に公開版を固定する手順

この文書は公式見解ではありません。参加するコンテスト時点の
[AtCoder公式ルール](https://info.atcoder.jp/entry/short-ahc-llm-rules-ja)と、
そのコンテスト固有の規定を必ず確認してください。

目的は、使用するコードと出典URLを同じ確定済みcommitへそろえ、コンテスト中は
必要な`.hpp`を自分の`main.cpp`へコピーするだけにしておくことです。
Pythonツールは準備を助ける任意機能であり、本番利用の必須依存ではありません。

## 1. 公開済みcommitを決める

コンテスト開始前に変更をcommit・pushし、GitHub上でそのcommitを開けることを
確認します。ローカルのcommit日時や`git log`だけでは、公開済みである確認には
なりません。

```bash
git rev-parse HEAD
```

表示された40桁のSHAを控えます。出典には、次のような`main`ではなくcommit固定の
URLを使います。

```text
https://github.com/tokotoko7777/ahc-precontest-kit/blob/<40桁のSHA>/library/timer.hpp
```

追跡中の文書やソースへ「そのファイル自身を含むcommit SHA」を事前に埋め込むことは
できません。SHA付きコピー物とオフラインbundleは、公開commitを確定した後に
生成します。

## 2. URL付きでパーツをコピーする

手動なら、GitHubまたは保存済みbundleで`.hpp`を開き、ファイル全体とその固定URLを
自分の`main.cpp`へコピーします。これが正式かつ最小の使い方です。

複数パーツへURLを自動で付ける場合は、公開済みcommitを明示します。

```bash
python3 tools/copy_part.py \
  --ref <コンテスト前に公開した40桁のSHA> \
  library/timer.hpp \
  library/random.hpp \
  library/simulated-annealing.hpp \
  -o copied-parts.hpp
```

問題固有の`main.cpp`まで結合することもできます。入力側と出力側は別ファイルに
してください。指定したパーツを読む`#include "library/xxx.hpp"`は、
`../../library/xxx.hpp`のような相対パス表記も含めて、結合時に自動で除去されます。
指定していないローカルincludeは残るため、パーツの指定漏れもコンパイル時に分かります。

```bash
python3 tools/copy_part.py \
  --ref <公開したSHA> \
  --main main.cpp \
  library/timer.hpp library/random.hpp \
  -o submission.cpp
```

各ブロックの直前には、次の3行が入ります。

```cpp
// BEGIN ahc-precontest-kit: library/timer.hpp
// Source: https://github.com/tokotoko7777/ahc-precontest-kit/blob/<SHA>/library/timer.hpp
// SHA-256: <元ファイルのhash>
```

ツールは指定refを1個の完全なcommit SHAへ解決し、全パーツをそのcommitのGit object
から読みます。選択した`.hpp`に未commit変更があれば停止するため、その変更へ古い
出典を黙って付けることはありません。現在のworking treeと異なる古い版を意図して
使う場合も、出力元は指定commitだけです。

このツールはcommitが実際にGitHubへpush済みかをネットワーク確認しません。
公開確認はコンテスト前に人が行います。

## 3. オフラインbundleを作る

公開を確認した同じSHAから、ネット接続なしで検索・コピーできるdirectoryを
コンテスト前に作ります。

```bash
python3 tools/make_offline_bundle.py \
  --ref <公開したSHA> \
  -o ../ahc-kit-offline
```

出力先は、新しい空の名前を指定します。bundleには次が入ります。

- `parts/library/*.hpp`: 指定commitに保存された全パーツ
- `manifest.json`: commit、固定URL、各ファイルのSHA-256
- `copy_part.py`: Gitやネットワークなしで使えるコピー補助
- `docs/`: パーツを探すためのREADME・ガイド類

オフライン環境ではbundleのdirectoryへ移動し、次のように使います。

```bash
python3 copy_part.py \
  --bundle . \
  --main /path/to/main.cpp \
  library/timer.hpp library/random.hpp \
  -o /path/to/submission.cpp
```

コピー前に各ファイルをmanifestのSHA-256と照合します。bundle内のファイルが
壊れた、差し替わった、URLが一致しない場合は出力せず停止します。Pythonを使わず、
`parts/library/`から手でコピーしても構いません。

## 4. コンテスト前チェックリスト

- 利用版をコンテスト開始前にGitHubへ公開した。
- 40桁のcommit SHAと、実際に開ける`blob/<SHA>/...` URLを確認した。
- 必要なパーツをコピーした単一`main.cpp`がC++17でコンパイルできた。
- `main.cpp`内で、各パーツの出典コメントとコードの対応が読める。
- オフラインbundleを作り、ネットワークなしでコピーできることを試した。
- bundle生成後にライブラリを変更した場合は、新しい公開版としてやり直した。
- 参加時点の公式ルールとコンテスト固有規定を再確認した。

この手順は、開催中に生成AIへ問題、エラー、スコアを送る仕組みを提供しません。
