#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="${1:-$ROOT_DIR/dist/windows-offline}"
FOUNDATION_REF="${FOUNDATION_REF:-main}"
AMQP_REF="${AMQP_REF:-main}"
FOUNDATION_REPO="${FOUNDATION_REPO:-https://github.com/yuqing112256313-pixel/Foundation.git}"
AMQP_REPO="${AMQP_REPO:-https://github.com/yuqing112256313-pixel/AMQP-CPP-CXX11.git}"

usage() {
  cat <<USAGE
Usage: $0 [output-dir]

Creates a Windows offline source bundle that includes:
  - Module-Context source (current HEAD)
  - Foundation source
  - AMQP-CPP-CXX11 source

Environment variables:
  FOUNDATION_REPO / FOUNDATION_REF
  AMQP_REPO       / AMQP_REF
USAGE
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

BUNDLE_ROOT="$OUT_DIR/Module-Context-offline-src"
mkdir -p "$BUNDLE_ROOT/third_party"

echo "[pack] exporting Module-Context source"
git -C "$ROOT_DIR" archive --format=tar HEAD | tar -xf - -C "$BUNDLE_ROOT"

echo "[pack] fetching Foundation ($FOUNDATION_REF)"
git clone --depth 1 --branch "$FOUNDATION_REF" "$FOUNDATION_REPO" "$BUNDLE_ROOT/third_party/Foundation"

echo "[pack] fetching AMQP-CPP-CXX11 ($AMQP_REF)"
git clone --depth 1 --branch "$AMQP_REF" "$AMQP_REPO" "$BUNDLE_ROOT/third_party/AMQP-CPP-CXX11"

ZIP_PATH="$OUT_DIR/Module-Context-offline-src.zip"
(
  cd "$OUT_DIR"
  rm -f "$ZIP_PATH"
  if command -v zip >/dev/null 2>&1; then
    zip -r "$ZIP_PATH" "$(basename "$BUNDLE_ROOT")" >/dev/null
  else
    tar -czf "$OUT_DIR/Module-Context-offline-src.tar.gz" "$(basename "$BUNDLE_ROOT")"
    echo "[pack] zip not found, produced tar.gz instead"
  fi
)

echo "[pack] done"
echo "[pack] bundle root: $BUNDLE_ROOT"
if [[ -f "$ZIP_PATH" ]]; then
  echo "[pack] archive: $ZIP_PATH"
else
  echo "[pack] archive: $OUT_DIR/Module-Context-offline-src.tar.gz"
fi
