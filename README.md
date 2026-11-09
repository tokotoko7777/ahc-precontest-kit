# AHC precontest kit

競技プログラミング向けのC++17ライブラリです。
各部品は `library/` に配置しています。

必要なヘッダをプロジェクトからインクルードして使用します。

```cpp
#include "library/dsu.hpp"

int main() {
    return 0;
}
```

```sh
g++ -std=c++17 -O2 -I . main.cpp -o main
make verify
```

`make verify` は各ヘッダをC++17の翻訳単位としてコンパイルし、全ヘッダを同時に使用できることも確認します。
