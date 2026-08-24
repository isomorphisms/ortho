#!/usr/bin/env sh
set -eu

IDRIC=${IDRIC:-../Idric/build/exec/idris2}
OUTPUT=app/src/main/cpp/orthant_model.h
TMP=$(mktemp)
trap 'rm -f "$TMP"' EXIT HUP INT TERM

"$IDRIC" src/Generate.idric -o ortho-model-generator
./build/exec/ortho-model-generator > "$TMP"

if [ "${1:-}" = "--check" ]; then
    cmp "$TMP" "$OUTPUT"
else
    mv "$TMP" "$OUTPUT"
    trap - EXIT HUP INT TERM
fi
