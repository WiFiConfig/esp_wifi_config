#!/usr/bin/env bash
# Host test for the URI path-segment percent-decoder (src/esp_wifi_config_uri.h).
#
#   ./test/uri_decode/run.sh
#
# The header is dependency-free, so this needs nothing but a C compiler.
# Sanitizers are opt-in (URI_DECODE_SAN='-fsanitize=address,undefined'):
# see test/json_writer/run.sh for why ASan is not assumed to work on a host.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT

cc -std=c99 -Wall -Wextra -Werror -g ${URI_DECODE_SAN:-} \
   -I"$ROOT/src" "$HERE/test.c" -o "$OUT/uri_decode"
"$OUT/uri_decode"
