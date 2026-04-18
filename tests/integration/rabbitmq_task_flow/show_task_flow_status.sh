#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
COMPOSE_FILE="$SCRIPT_DIR/docker-compose.yml"
RUNTIME_DIR="${1:-${SCRIPT_DIR}/../../../build/tests/rabbitmq_task_flow_runtime}"
MASTER_LOG="$RUNTIME_DIR/master.log"
WORKER_LOG="$RUNTIME_DIR/worker.log"
RESULT_DIR="$RUNTIME_DIR/worker/results"

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

  echo "No container compose runtime found. Install Docker Desktop / docker compose plugin, or set MC_COMPOSE_BIN to a compatible compose command." >&2
  return 127
}

echo "=== compose ps ==="
compose ps || true

echo
if [[ -d "$RUNTIME_DIR" ]]; then
  echo "=== runtime dir ==="
  echo "$RUNTIME_DIR"
  echo
  echo "=== runtime tree ==="
  find "$RUNTIME_DIR" -maxdepth 3 -type f | sort
else
  echo "runtime dir not found: $RUNTIME_DIR"
fi

echo
if [[ -f "$MASTER_LOG" ]]; then
  echo "=== tail master.log ==="
  tail -n 40 "$MASTER_LOG"
fi

echo
if [[ -f "$WORKER_LOG" ]]; then
  echo "=== tail worker.log ==="
  tail -n 40 "$WORKER_LOG"
fi

echo
if compgen -G "$RESULT_DIR/*_result.txt" >/dev/null 2>&1; then
  echo "=== latest result reports ==="
  ls -1 "$RESULT_DIR"/*_result.txt
fi
