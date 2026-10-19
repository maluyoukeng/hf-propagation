#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_root}"

CC="${CC:-cc}"
PYTHON="${PYTHON:-python3}"
if ! command -v "${PYTHON}" >/dev/null 2>&1; then
    PYTHON=python
fi
test_dir="$(mktemp -d)"
trap 'rm -rf "${test_dir}"' EXIT

"${PYTHON}" tests/test_hf_font_coverage.py

"${CC}" -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_prop_model.c main/prop_model.c \
    -o "${test_dir}/test_prop_model" -lm
"${test_dir}/test_prop_model"

"${CC}" -std=c11 -Wall -Wextra -Werror -Imain -Itests/settings_stubs \
    tests/test_app_settings.c main/app_settings.c main/prop_model.c \
    tests/settings_stubs/nvs_stubs.c \
    -o "${test_dir}/test_app_settings" -lm
"${test_dir}/test_app_settings"

"${CC}" -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_app_spaceweather.c main/app_spaceweather.c \
    -o "${test_dir}/test_app_spaceweather" -lm
"${test_dir}/test_app_spaceweather"

"${CC}" -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_app_location.c main/app_location_parse.c \
    -o "${test_dir}/test_app_location" -lm
"${test_dir}/test_app_location"

echo "Host tests: PASS"
