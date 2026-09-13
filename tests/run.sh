#!/bin/sh

set -eu

compiler=${1:?usage: tests/run.sh path-to-bfc}
target=aarch64-apple-darwin
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
temp_dir=$(mktemp -d "${TMPDIR:-/tmp}/bfc-tests.XXXXXX")

cleanup()
{
    rm -rf "$temp_dir"
}

trap cleanup EXIT HUP INT TERM

"$compiler" -S -t "$target" "$root_dir/tests/hello.bf" -o "$temp_dir/hello.s"
cmp "$root_dir/tests/hello.bf.s" "$temp_dir/hello.s"

"$compiler" -S -t "$target" "$root_dir/tests/multiply.bf" -o "$temp_dir/multiply.s"
test -s "$temp_dir/multiply.s"

printf '[+\n' > "$temp_dir/malformed.bf"
if "$compiler" -S -t "$target" "$temp_dir/malformed.bf" -o "$temp_dir/malformed.s" \
    > "$temp_dir/malformed.out" 2> "$temp_dir/malformed.err"; then
    echo "malformed input unexpectedly succeeded" >&2
    exit 1
fi
grep -q "ERR_MISMATCHED_BRACKET" "$temp_dir/malformed.err"

echo "regression tests passed"