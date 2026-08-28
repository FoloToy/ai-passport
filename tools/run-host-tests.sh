#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$(mktemp -d /tmp/ai-passport-host-tests.XXXXXX)"
trap 'case "${test_dir}" in /tmp/ai-passport-host-tests.*) rm -rf -- "${test_dir}" ;; esac' EXIT

cd "${repo_root}"

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_ui_pixel_math.c main/ui_pixel_math.c \
    -o "${test_dir}/test_ui_pixel_math"
"${test_dir}/test_ui_pixel_math"

echo "Host tests: PASS (1 executable)"
