#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
COMPOSE_FILE="$SCRIPT_DIR/docker-compose.yml"
RUNTIME_DIR="${1:-${SCRIPT_DIR}/../../../build/tests/rabbitmq_perf_runtime}"
REMOVE_RUNTIME="${MC_REMOVE_RUNTIME:-0}"

compose() {
  if [[ -n "${MC_COMPOSE_BIN:-}" ]]; then
    ${MC_COMPOSE_BIN} -f "$COMPOSE_FILE" "$@"
    return
  fi

  if command -v docker >/dev/null 2>&1 && docker compose version >/dev/null 2>&1; then
    docker compose -f "$COMPOSE_FILE" "$@"
    return
  fi

  if command -v docker-compose >/dev/null 2>&1; then
    docker-compose -f "$COMPOSE_FILE" "$@"
    return
  fi

  echo "No container compose runtime found." >&2
  return 127
}

echo "[cleanup] stopping RabbitMQ perf compose resources"
compose down -v --remove-orphans

if [[ "$REMOVE_RUNTIME" == "1" ]]; then
  echo "[cleanup] removing runtime dir: $RUNTIME_DIR"
  rm -rf "$RUNTIME_DIR"
else
  echo "[cleanup] runtime dir kept: $RUNTIME_DIR"
  echo "[cleanup] set MC_REMOVE_RUNTIME=1 to remove it too"
fi
