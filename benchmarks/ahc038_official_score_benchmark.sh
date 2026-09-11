#!/usr/bin/env bash
set -eu

# AHC038公式ツールに同梱されたseed 0..99を使う再現用ベンチマーク。
#
# 使い方:
#   benchmarks/ahc038_official_score_benchmark.sh /path/to/tools 10
#
# 第1引数は、公式zipを展開してできる tools ディレクトリ。
# 第2引数は先頭から使うケース数（省略時10、最大100）。

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
  echo "usage: $0 /path/to/ahc038/tools [case_count]" >&2
  exit 2
fi

TOOLS_DIR=$1
CASE_COUNT=${2:-10}
if ! [[ "$CASE_COUNT" =~ ^[0-9]+$ ]] ||
   [ "$CASE_COUNT" -lt 1 ] || [ "$CASE_COUNT" -gt 100 ]; then
  echo "case_count must be an integer from 1 to 100" >&2
  exit 2
fi

REPOSITORY_ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD_DIR="$REPOSITORY_ROOT/build/ahc038-official"
VISUALIZER="$TOOLS_DIR/target/release/vis"
mkdir -p "$BUILD_DIR/out"

if [ ! -x "$VISUALIZER" ]; then
  cargo build -q -r --bin vis --manifest-path "$TOOLS_DIR/Cargo.toml"
fi

CXX=${CXX:-g++}
COMMON_FLAGS=(-std=c++17 -O3 -DNDEBUG -I"$REPOSITORY_ROOT")
"$CXX" "${COMMON_FLAGS[@]}" \
  "$REPOSITORY_ROOT/practice/ahc038/main.cpp" \
  -o "$BUILD_DIR/existing-greedy"

# 幅0は新しい例の候補生成をそのまま貪欲に使う版。
for width in 0 1 3 6 7; do
  "$CXX" "${COMMON_FLAGS[@]}" -DAHC038_BEAM_WIDTH="$width" \
    "$REPOSITORY_ROOT/examples/search/ahc038_variable_cost_beam.cpp" \
    -o "$BUILD_DIR/beam-$width"
done

run_method() {
  local name=$1
  local executable=$2
  local sum=0
  local wins=0
  local scores=()

  for ((seed = 0; seed < CASE_COUNT; ++seed)); do
    local stem
    stem=$(printf '%04d' "$seed")
    local input="$TOOLS_DIR/in/$stem.txt"
    local output="$BUILD_DIR/out/$name-$stem.txt"
    if [ ! -f "$input" ]; then
      echo "missing official input: $input" >&2
      exit 2
    fi
    "$executable" < "$input" > "$output"
    local score_line
    score_line=$(cd "$BUILD_DIR" &&
      "$VISUALIZER" "$input" "$output" 2>/dev/null)
    local score=${score_line#Score = }
    if ! [[ "$score" =~ ^[0-9]+$ ]]; then
      echo "visualizer rejected $name seed $seed: $score_line" >&2
      exit 1
    fi
    scores+=("$score")
    sum=$((sum + score))
    if [ "$name" != existing-greedy ]; then
      local baseline_file="$BUILD_DIR/out/existing-greedy-$stem.score"
      local baseline
      baseline=$(<"$baseline_file")
      if [ "$score" -lt "$baseline" ]; then
        wins=$((wins + 1))
      fi
    else
      printf '%s\n' "$score" > "$BUILD_DIR/out/$name-$stem.score"
    fi
  done

  local average
  average=$(awk -v sum="$sum" -v count="$CASE_COUNT" \
    'BEGIN { printf "%.2f", sum / count }')
  printf '%-18s average=%8s sum=%8d' "$name" "$average" "$sum"
  if [ "$name" != existing-greedy ]; then
    printf ' wins_vs_existing=%d/%d' "$wins" "$CASE_COUNT"
  fi
  printf ' scores='
  printf '%s,' "${scores[@]}" | sed 's/,$//'
  printf '\n'
}

run_method existing-greedy "$BUILD_DIR/existing-greedy"
run_method macro-greedy "$BUILD_DIR/beam-0"
run_method beam-width-1 "$BUILD_DIR/beam-1"
run_method beam-width-3 "$BUILD_DIR/beam-3"
run_method beam-width-6 "$BUILD_DIR/beam-6"
run_method beam-width-7 "$BUILD_DIR/beam-7"
