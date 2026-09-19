# 手法ごとの探索フォーマット

手法のフォルダを開き、その中から問題に合うバリエーションを選びます。
焼きなましと山登りは「局所探索」にまとめています。種類を減らすための分類ではありません。

| フォルダ | 中にあるバリエーション |
|---|---|
| [beam/](beam/README.md) | 通常、差分評価、木上、世代飛ばし木上、chokudai、枝刈り設定、多点スタート |
| [local-search/](local-search/README.md) | 焼きなまし・山登り共通、部分破壊・再構築、適応的な近傍選択、反復局所探索、途中再生、前後DP |
| [monte-carlo/](monte-carlo/README.md) | 共通未来のrollout、木探索（UCT） |

1. 選んだcppをmain.cppへコピーする。
2. TODOを上から埋める。関数の位置・引数・戻り値は配置済み。
3. 提出時はincludeを対応hppの全文へ置き換える。main.cppだけで動かす。

開発中のincludeを使う場合は、リポジトリ直下から次のようにコンパイルできます。

```sh
g++ -std=c++17 -O3 -I. template/search/local-search/basic.cpp -o main
```

[局所探索の基本形](local-search/basic.cpp)はUSE_ANNEALINGをtrueにすると焼きなまし、
falseにすると山登りです。状態・近傍・差分評価・更新は同じものを使います。
LNSも独立した大分類にはせず、[部分破壊・再構築の形式](local-search/destroy-repair.cpp)として
同じフォルダに置いています。各形式の戻り値や同点の扱いはフォルダ内の説明を参照してください。

基本形も設定付きの形式も同じ手法のフォルダで探せます。
未知の未来を乱数で試さない[決定的な先読み](../advanced/README.md)は、モンテカルロとは別に残しています。
既存のhppの場所とAPI、実問題例、ベンチマーク履歴は維持しています。
何も埋めていない雛形はコンパイル確認用で、問題の解答を出すものではありません。

[短いガイド](../../SEARCH_GUIDE.md) / [実問題の完成例](../../examples/search/README.md)
