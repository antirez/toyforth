#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."

pi=314159265358979323846264338327
for example in examples/pi.toy examples/pi_golf.toy; do
    digits=$(./toyforth "$example" | sed -n '2p')
    test "$digits" = "$pi"
    printf '%s: OK\n' "$example"
done
