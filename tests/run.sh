#!/bin/sh

set -eu

if [ "$#" -eq 0 ]; then
    echo "usage: tests/run.sh path-to-bfc [path-to-bfc ...]" >&2
    exit 2
fi

target=aarch64-apple-darwin
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
temp_dir=$(mktemp -d "${TMPDIR:-/tmp}/bfc-tests.XXXXXX")

cleanup()
{
    rm -rf "$temp_dir"
}

trap cleanup EXIT HUP INT TERM

baseline=""

for compiler do
    "$compiler" -S -t "$target" "$root_dir/tests/hello.bf" -o "$temp_dir/hello.s"

    if [ -z "$baseline" ]; then
        baseline=$temp_dir/hello.baseline.s
        cp "$temp_dir/hello.s" "$baseline"
        grep -q "\.section __DATA,__bss" "$baseline"
        grep -q "_bfc_tape:" "$baseline"
        grep -q "_putchar" "$baseline"
    else
        cmp "$baseline" "$temp_dir/hello.s"
    fi

    "$compiler" -S -t "$target" "$root_dir/tests/multiply.bf" -o "$temp_dir/multiply.s"
    test -s "$temp_dir/multiply.s"

    printf '[+\n' > "$temp_dir/malformed.bf"
    if "$compiler" -S -t "$target" "$temp_dir/malformed.bf" -o "$temp_dir/malformed.s" \
        > "$temp_dir/malformed.out" 2> "$temp_dir/malformed.err"; then
        echo "malformed input unexpectedly succeeded" >&2
        exit 1
    fi
    grep -q "ERR_MISMATCHED_BRACKET" "$temp_dir/malformed.err"
done

echo "regression tests passed"
