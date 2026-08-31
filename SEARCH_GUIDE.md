# AHC探索コアガイド

まずは、問題の状態をどう保持できるかと、1手で何世代進むかで選びます。

| 探索 | 向いている問題 | 状態管理 |
|---|---|---|
| `time-based-simulated-annealing.hpp` | 1つの解を局所変更し続けられる | 現在解を1つ保持 |
| `simple-beam-search.hpp` | 手数ごとに候補を残したい。`State`が小さい | 子の`State`をコピー |
| `tree-beam-search.hpp` | 全行動で1世代ずつ進む。`State`のコピーが重い | `apply / revert`と履歴木 |
| `cost-tree-beam-search.hpp` | 行動ごとに1、2、3世代など進み幅が違う | `apply / revert`と到着世代別の履歴木 |

迷ったら、局所変更が自然なら焼きなまし、手順を1手ずつ作るなら
`SimpleBeamSearch`から始めます。状態コピーがボトルネックになった時だけ、
木上版へ移します。

## 4本を単体で使った完全な例

| 探索コア | 問題例 | 完全な`main.cpp` |
|---|---|---|
| 時間焼きなまし | AHC006の配達経路 | [`ahc006_sa.cpp`](examples/search/ahc006_sa.cpp) |
| 通常ビーム | Introduction to Heuristics Contest A | [`intro_heuristics_simple_beam.cpp`](examples/search/intro_heuristics_simple_beam.cpp) |
| 木上ビーム | AHC021の山崩し | [`ahc021_tree_beam.cpp`](examples/search/ahc021_tree_beam.cpp) |
| 世代飛ばし木上ビーム | 移動時間1〜3の締切付き宝集め | [`variable_cost_beam.cpp`](examples/search/variable_cost_beam.cpp) |

どれも探索ヘッダを1個だけ読み込む、入力から出力まで揃った例です。
提出時は使用したヘッダの中身を`main.cpp`の先頭へコピーし、`#include`の1行を
削除します。検証内容とコンパイル方法は
[`examples/search/README.md`](examples/search/README.md) にまとめています。

## 焼きなましの最小形

`improvement`は必ず「正なら良い変更」にします。最大化は
`new_score - current_score`、最小化は`current_cost - new_cost`です。

```cpp
TimeBasedSimulatedAnnealing sa(
    1900.0, 100.0, 1.0, 123, 64);

State current = make_initial_state();
long long current_score = calculate_score(current);
State best = current;
long long best_score = current_score;

while (!sa.is_over()) {
  Move move = make_random_move(current);
  long long improvement = calculate_delta(current, move);

  if (sa.accept(improvement)) {
    apply(current, move);
    current_score += improvement;
    if (best_score < current_score) {
      best_score = current_score;
      best = current;
    }
  }
}
```

最後の`64`は時計と温度を更新する間隔です。近傍1回が重いなら1〜8、
軽いなら64〜256を目安にします。焼きなましの最終状態は最良とは限らないため、
`best`は必ず別に保存します。

## SimpleBeamSearchの最小形

`expand(state)`は子状態の`vector`、`rank_score(state)`はビーム内の
順位を返します。

```cpp
SimpleBeamSearch<State, long long> beam(initial_state, 200);
beam.reserve_candidates(200 * average_branch_count);

for (int turn = 0; turn < max_turn; ++turn) {
  if (!beam.step(expand, rank_score)) break;
}

State answer = beam.best();
```

`State`の中に盤面、得点、操作履歴を入れられるので、最初に試すのに
向いています。履歴や盤面が大きくなり、子ごとのコピーが重くなったら
`TreeBeamSearch`へ移します。

## 世代が飛ばないTreeBeamSearch

全行動がちょうど1世代進む時に使います。`State`は1個だけ持ち、
生き残った履歴木をDFSしながら状態を変更・復元します。

```cpp
TreeBeamSearch<State, Move, long long> beam(
    initial_state, rank_score(initial_state), 200);

for (int turn = 0; turn < max_turn; ++turn) {
  if (!beam.step(expand, apply, revert, rank_score)) break;
}

vector<Move> answer = beam.restore();
```

`apply(state, move)`で変えたスコア、hash、個数表、集合などは、
`revert(state, move)`で全て元へ戻します。`Move`には、上書き前の値など
復元に必要な情報も入れます。

## 世代が飛ぶCostTreeBeamSearch

1行動の消費手数が異なる場合は、同じ到着世代の候補だけを比較します。
`get_advance(move)`は正の整数を返します。

```cpp
CostTreeBeamSearch<State, Move, long long> beam(
    initial_state, rank_score(initial_state), 200, max_generation);

while (beam.step(
    expand, apply, revert, rank_score,
    [](const Move& move) { return move.advance; })) {
}

vector<Move> answer = beam.restore();
```

`advance <= 0`は不正です。`max_generation`を超える行動は自動で候補から外れます。
`step()`は、候補が存在する最小の到着世代へ進みます。

## 順位評価と最終目的を分ける

`evaluate`または`rank_score`は、限られた幅に残す候補を決めるための値です。
現在得点に、残り手数の余力、未達成罰則、将来価値などを加えても構いません。

一方、提出解を比べる時は必ず問題本来の`official_score`を使います。
木上ビームの`best_score()`は順位評価値であり、提出得点とは限りません。

```cpp
long long rank_score(const State& state) {
  return state.current_score + estimate_future_gain(state);
}

long long official_score(const State& state) {
  return calculate_problem_score(state);
}
```

## terminalを消さない

一部の状態だけ早く終端に着く問題では、`expand`が空の候補を返すと
その状態は次の層に残りません。終端に到達した瞬間に、本来の得点と
出力に必要な情報を別に保存します。

```cpp
optional<pair<long long, Answer>> best_terminal;

auto expand = [&](const State& state) {
  if (is_terminal(state)) {
    long long score = official_score(state);
    if (!best_terminal || best_terminal->first < score) {
      best_terminal = pair{score, make_answer(state)};
    }
    return vector<Move>{};
  }
  return make_moves(state);
};
```

全経路が必ず同じ最終世代へ着くなら、探索後の`best()`または`restore()`で
十分です。早く終わるterminalがある時は、現在のビームだけを見て答えにしては
いけません。

木上版では、各`step`の前に現在層をコピーなしで確認できます。

```cpp
beam.for_each_state(
    [&](int rank, const State& state) {
      if (is_terminal(state)) {
        save_if_better(official_score(state), beam.restore(rank));
      }
    },
    apply, revert);
```

これは現在の幅に残った状態を調べるAPIです。幅から落ちた候補は戻らないため、
terminalを過小評価しない`rank_score`も用意します。

## keyで重複を消す

同じ世代の同じ状態へ多くの経路から到達するなら、`step_with_key`を使います。

```cpp
// SimpleBeamSearch
beam.step_with_key(expand, rank_score, make_key);

// TreeBeamSearch
beam.step_with_key(expand, apply, revert, rank_score, make_key);

// CostTreeBeamSearch
beam.step_with_key(
    expand, apply, revert, rank_score, get_advance, make_key);
```

keyは「今後の行動候補と最終得点の比較に必要な状態」を区別します。
盤面が同じでも、残り資源や将来の選択肢が違うなら別keyです。世代はライブラリ側で
分けるため、通常はkeyに入れる必要がありません。

64bit Zobrist hashは高速ですが、衝突可能性は0ではありません。正確性を必ず保ちたい時は
状態そのものを表す値をkeyにします。重複が少ない場合はhash表の費用が増えるだけなので、
通常の`step`を使います。

## apply / revertの確認

`TreeBeamSearch`と`CostTreeBeamSearch`で最も重要な条件です。

```cpp
State before = state;
apply(state, move);
revert(state, move);
assert(state == before);
```

盤面だけでなく、得点、hash、統計量、候補集合も確認します。`apply`内で
乱数を振ると復元しにくいため、ランダムな選択結果は先に`Move`へ入れます。

## 速度の最終チェック

- 得点とhashはできるだけ差分更新する。
- `rank_score`は全候補に呼ばれるため、安い評価を先に使う。
- `expand`の`vector`と候補bufferは容量を再利用する。
- `SimpleBeamSearch::reserve_candidates`、`TreeBeamSearch::reserve_nodes`、
  `TreeBeamSearch::reserve_candidates`、`CostTreeBeamSearch`の同名関数で
  上限が分かる領域を予約する。
- 分岐数の上限が小さいなら、`expand`から`vector`ではなく
  `FixedVector<State, N>`または`FixedVector<Move, N>`を返して層ごとの
  heap確保を避ける。
- 上限を固定できない場合は、外に`vector`を1個用意して`reserve`し、
  `expand`からその非const参照を返すと容量を層間で再利用できる。
- `expand`が返した非constコンテナの要素は探索コアがmoveして消費する。
  再利用する場合は、次の`expand`呼び出しで`clear`して中身を作り直す。
  読み取り専用の固定行動表は`const vector<Move>&`で返せばコピーして使える。
- `State`と`Move`を小さく保ち、文字列や全履歴を候補ごとに持たない。
- key重複除去は、同じ世代の重複が十分多い時だけ使う。
- ビーム幅だけでなく、1秒当たりの候補評価数と複数seedの最終得点で比べる。
