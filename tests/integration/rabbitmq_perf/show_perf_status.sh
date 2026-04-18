#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
COMPOSE_FILE="$SCRIPT_DIR/docker-compose.yml"
RUNTIME_DIR="${1:-${SCRIPT_DIR}/../../../build/tests/rabbitmq_perf_runtime}"
RABBITMQ_API_URL="${RABBITMQ_API_URL:-http://127.0.0.1:15673/api}"
RABBITMQ_ADMIN_USER="${RABBITMQ_ADMIN_USER:-mc_perf_admin}"
RABBITMQ_ADMIN_PASS="${RABBITMQ_ADMIN_PASS:-mc_perf_admin_secret}"
RABBITMQ_VHOST="${RABBITMQ_VHOST:-mc_perf}"
LOG_DIR="$RUNTIME_DIR/logs"
REPORT_DIR="$RUNTIME_DIR/report"
MONITOR_CSV="$RUNTIME_DIR/monitor/queue_metrics.csv"

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

queue_json() {
  local queue_name="$1"
  curl -fsS -u "$RABBITMQ_ADMIN_USER:$RABBITMQ_ADMIN_PASS" \
    "$RABBITMQ_API_URL/queues/$RABBITMQ_VHOST/$queue_name"
}

echo "=== compose ps ==="
compose ps || true

echo
for queue in mc.perf.task.queue mc.perf.result.queue; do
  echo "=== queue: $queue ==="
  queue_json "$queue" 2>/dev/null || echo "queue unavailable"
  echo
 done

if [[ -d "$RUNTIME_DIR" ]]; then
  echo "=== runtime tree ==="
  find "$RUNTIME_DIR" -maxdepth 3 -type f | sort
  echo
fi

if [[ -f "$REPORT_DIR/index.html" ]]; then
  echo "=== report ==="
  echo "$REPORT_DIR/index.html"
  echo
fi

for log_file in "$LOG_DIR/master.log" "$LOG_DIR"/worker-*.log; do
  [[ -f "$log_file" ]] || continue
  echo "=== tail $(basename "$log_file") ==="
  tail -n 40 "$log_file"
  echo
 done

if [[ -f "$MONITOR_CSV" ]]; then
  echo "=== latest monitor samples ==="
  tail -n 10 "$MONITOR_CSV"
fi
