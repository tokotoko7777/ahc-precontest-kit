// GCC提出用の最適化。診断時は -DAHC_DISABLE_GCC_OPTIMIZE で無効化。
#if defined(__GNUC__) && !defined(__clang__) && !defined(AHC_DISABLE_GCC_OPTIMIZE)
#pragma GCC optimize("O3")
#endif

#include <bits/stdc++.h>

using namespace std;

// Copy the contents of the required library/*.hpp files here.

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  // Write problem-specific code here.
}
