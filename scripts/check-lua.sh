#!/usr/bin/env bash
# Check the maintained Lua sources using the same configuration locally and in CI.
set -euo pipefail
cd "$(dirname "$0")/.."

if (( $# > 1 )); then
  echo "Usage: $0 [--fix]" >&2
  exit 2
fi
paths=(examples/lua .luacheckrc)
if [[ -d lua ]]; then paths+=(lua); fi
case "${1:-}" in
  '') stylua --check --verify "${paths[@]}" ;;
  --fix) stylua --verify "${paths[@]}" ;;
  *) echo "Usage: $0 [--fix]" >&2; exit 2 ;;
esac
luacheck "${paths[@]}"
