#!/bin/bash
# Builds and runs the ownership harness (ASan + UBSan) on macOS.
#   tests/ownership/run.sh [SOURCE_TREE]
# SOURCE_TREE defaults to this checkout; pass another tree (e.g. a git archive
# of an older commit) to run the same harness against those plugin sources.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$(cd "${1:-$HERE/../..}" && pwd)"
OUT="${OUT:-$(mktemp -d)}"; mkdir -p "$OUT"
SDK="$SRC/4D Plugin API"
INC=(-I"$SDK" -I"$SDK/Classes" -I"$SRC" -I"$SRC/support"
     -I"$SRC/include/soci-darwin/include" -I"$SRC/include/soci-darwin/include/mysql")
SAN=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined -g -O1)

set -e
clang -x c -c "${SAN[@]}" "${INC[@]}" "$SDK/4DPluginAPI.c" -o "$OUT/4DPluginAPI.o" -w -Wno-int-conversion -Wno-incompatible-function-pointer-types
for f in "$SRC/4DPlugin-SOCI.cpp" "$SRC/support/4DPlugin-JSON.cpp" "$SDK/Classes/C_TEXT.cpp" "$HERE/harness.cpp"; do
  clang++ -x objective-c++ -std=c++17 -c "${SAN[@]}" "${INC[@]}" "$f" -o "$OUT/$(basename "$f").o" -w
done
LIBS=(soci_core soci_sqlite3 soci_odbc soci_mysql soci_postgresql sqlite3 odbc odbcinst
      mysqlclient pq pgcommon pgport ssl crypto zstd lz4 z)
clang++ "${SAN[@]}" "$OUT"/*.o -L"$SRC/a" "${LIBS[@]/#/-l}" "$(xcrun --show-sdk-path)/usr/lib/libiconv.tbd" -lresolv \
  -framework Cocoa -framework CoreFoundation -framework Security -o "$OUT/harness"
set +e

export ASAN_OPTIONS="abort_on_error=0:detect_leaks=0"
export UBSAN_OPTIONS="print_stacktrace=1"
pass=0; fail=0
for t in $("$OUT/harness"); do
  if "$OUT/harness" "$t" > "$OUT/$t.log" 2>&1; then
    pass=$((pass+1)); echo "PASS $t"
  else
    fail=$((fail+1)); echo "FAIL $t"
    grep -E "live:|leaked|check failed|ERROR: AddressSanitizer|runtime error|FAKE4D" "$OUT/$t.log" | head -5 | sed 's/^/    /'
  fi
done
echo "passed=$pass failed=$fail"
[ "$fail" -eq 0 ]
