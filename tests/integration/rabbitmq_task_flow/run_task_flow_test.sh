#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 3 ]]; then
  echo "Usage: $0 <master-bin> <worker-bin> <runtime-dir>" >&2
  exit 2
fi

MASTER_BIN="$1"
WORKER_BIN="$2"
RUNTIME_DIR="$3"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
COMPOSE_FILE="$SCRIPT_DIR/docker-compose.yml"
BOOTSTRAP_SCRIPT="$SCRIPT_DIR/bootstrap_rabbitmq.sh"

RABBITMQ_ADMIN_USER="${RABBITMQ_ADMIN_USER:-mc_admin}"
RABBITMQ_ADMIN_PASS="${RABBITMQ_ADMIN_PASS:-mc_admin_secret}"
RABBITMQ_MASTER_USER="${RABBITMQ_MASTER_USER:-mc_master}"
RABBITMQ_MASTER_PASS="${RABBITMQ_MASTER_PASS:-master_secret}"
RABBITMQ_WORKER_USER="${RABBITMQ_WORKER_USER:-mc_worker}"
RABBITMQ_WORKER_PASS="${RABBITMQ_WORKER_PASS:-worker_secret}"
RABBITMQ_VHOST="${RABBITMQ_VHOST:-mc_integration}"

MASTER_URI="amqp://${RABBITMQ_MASTER_USER}:${RABBITMQ_MASTER_PASS}@127.0.0.1:5672/${RABBITMQ_VHOST}"
WORKER_URI="amqp://${RABBITMQ_WORKER_USER}:${RABBITMQ_WORKER_PASS}@127.0.0.1:5672/${RABBITMQ_VHOST}"
TASK_ID="${MC_TASK_ID:-task-$(date +%Y%m%d-%H%M%S)}"
IMAGE_DIR="$RUNTIME_DIR/shared/images"
RESULT_DIR="$RUNTIME_DIR/worker/results"
IMAGE_PATH="$IMAGE_DIR/${TASK_ID}.ppm"
MASTER_LOG="$RUNTIME_DIR/master.log"
WORKER_LOG="$RUNTIME_DIR/worker.log"
KEEP_ENV="${MC_KEEP_ENV:-1}"
RECREATE_RUNTIME="${MC_RECREATE_RUNTIME:-1}"

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

cleanup() {
  local exit_code=$?
  if [[ $exit_code -ne 0 ]]; then
    echo "[run] task-flow test failed, dumping RabbitMQ logs..." >&2
    compose logs --no-color || true
    [[ -f "$MASTER_LOG" ]] && { echo "--- master.log ---" >&2; cat "$MASTER_LOG" >&2; }
    [[ -f "$WORKER_LOG" ]] && { echo "--- worker.log ---" >&2; cat "$WORKER_LOG" >&2; }
  fi

  if [[ "$KEEP_ENV" == "0" ]]; then
    echo "[run] MC_KEEP_ENV=0, cleaning compose resources" >&2
    compose down -v --remove-orphans >/dev/null 2>&1 || true
  else
    echo "[run] environment kept for inspection" >&2
    echo "[run] status:  bash $SCRIPT_DIR/show_task_flow_status.sh $RUNTIME_DIR" >&2
    echo "[run] cleanup: bash $SCRIPT_DIR/cleanup_task_flow_env.sh $RUNTIME_DIR" >&2
  fi
}
trap cleanup EXIT

if [[ "$RECREATE_RUNTIME" == "1" ]]; then
  rm -rf "$RUNTIME_DIR"
fi
mkdir -p "$IMAGE_DIR" "$RESULT_DIR"

echo "[run] using runtime dir: $RUNTIME_DIR"
echo "[run] starting RabbitMQ compose environment"
compose up -d
echo "[run] bootstrapping RabbitMQ accounts, vhost, topology"
RABBITMQ_ADMIN_USER="$RABBITMQ_ADMIN_USER" \
RABBITMQ_ADMIN_PASS="$RABBITMQ_ADMIN_PASS" \
RABBITMQ_MASTER_USER="$RABBITMQ_MASTER_USER" \
RABBITMQ_MASTER_PASS="$RABBITMQ_MASTER_PASS" \
RABBITMQ_WORKER_USER="$RABBITMQ_WORKER_USER" \
RABBITMQ_WORKER_PASS="$RABBITMQ_WORKER_PASS" \
RABBITMQ_VHOST="$RABBITMQ_VHOST" \
bash "$BOOTSTRAP_SCRIPT"

echo "[run] launching worker simulator"
"$WORKER_BIN" \
  --rabbitmq-uri "$WORKER_URI" \
  --output-dir "$RESULT_DIR" \
  --timeout-ms 30000 >"$WORKER_LOG" 2>&1 &
WORKER_PID=$!

sleep 2

echo "[run] launching master simulator"
"$MASTER_BIN" \
  --rabbitmq-uri "$MASTER_URI" \
  --image-path "$IMAGE_PATH" \
  --task-id "$TASK_ID" \
  --timeout-ms 30000 >"$MASTER_LOG" 2>&1

wait "$WORKER_PID"

echo "[run] validating generated artifacts"
test -f "$RESULT_DIR/${TASK_ID}_processed.ppm"
test -f "$RESULT_DIR/${TASK_ID}_result.txt"
grep -q '^status=processed$' "$RESULT_DIR/${TASK_ID}_result.txt"
grep -q "^task_id=${TASK_ID}$" "$RESULT_DIR/${TASK_ID}_result.txt"

echo "[run] RabbitMQ task-flow integration test passed"
echo "[run] master log: $MASTER_LOG"
echo "[run] worker log: $WORKER_LOG"
echo "[run] result report: $RESULT_DIR/${TASK_ID}_result.txt"
