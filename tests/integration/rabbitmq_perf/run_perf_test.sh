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
MONITOR_SCRIPT="$SCRIPT_DIR/monitor_perf.py"
REPORT_SCRIPT="$SCRIPT_DIR/generate_perf_report.py"

RABBITMQ_API_URL="${RABBITMQ_API_URL:-http://127.0.0.1:15673/api}"
RABBITMQ_ADMIN_USER="${RABBITMQ_ADMIN_USER:-mc_perf_admin}"
RABBITMQ_ADMIN_PASS="${RABBITMQ_ADMIN_PASS:-mc_perf_admin_secret}"
RABBITMQ_MASTER_USER="${RABBITMQ_MASTER_USER:-mc_perf_master}"
RABBITMQ_MASTER_PASS="${RABBITMQ_MASTER_PASS:-perf_master_secret}"
RABBITMQ_WORKER_USER="${RABBITMQ_WORKER_USER:-mc_perf_worker}"
RABBITMQ_WORKER_PASS="${RABBITMQ_WORKER_PASS:-perf_worker_secret}"
RABBITMQ_VHOST="${RABBITMQ_VHOST:-mc_perf}"

MASTER_URI="amqp://${RABBITMQ_MASTER_USER}:${RABBITMQ_MASTER_PASS}@127.0.0.1:5673/${RABBITMQ_VHOST}"
WORKER_URI="amqp://${RABBITMQ_WORKER_USER}:${RABBITMQ_WORKER_PASS}@127.0.0.1:5673/${RABBITMQ_VHOST}"
SCENARIO_ID="${MC_SCENARIO_ID:-perf-$(date +%Y%m%d-%H%M%S)}"
WORKER_COUNT="${MC_WORKER_COUNT:-5}"
TASK_COUNT="${MC_TASK_COUNT:-120}"
TARGET_RATE="${MC_TARGET_RATE:-60}"
IMAGE_BYTES="${MC_IMAGE_BYTES:-20971520}"
SIMULATE_PROCESS_MS="${MC_SIMULATE_PROCESS_MS:-80}"
TIMEOUT_MS="${MC_TIMEOUT_MS:-600000}"
IDLE_TIMEOUT_MS="${MC_IDLE_TIMEOUT_MS:-5000}"
KEEP_ENV="${MC_KEEP_ENV:-1}"
RECREATE_RUNTIME="${MC_RECREATE_RUNTIME:-1}"
NOTES="${MC_NOTES:-单机 macOS + Colima + Docker + RabbitMQ，5 个 worker 进程竞争同一任务队列；图片处理耗时为模拟 sleep。}"

IMAGE_DIR="$RUNTIME_DIR/shared/images"
WORKER_OUTPUT_DIR="$RUNTIME_DIR/worker/output"
REPORT_DIR="$RUNTIME_DIR/report"
LOG_DIR="$RUNTIME_DIR/logs"
MONITOR_DIR="$RUNTIME_DIR/monitor"
STOP_FILE="$RUNTIME_DIR/stop.signal"
MASTER_LOG="$LOG_DIR/master.log"
MONITOR_LOG="$LOG_DIR/monitor.log"
MONITOR_CSV="$MONITOR_DIR/queue_metrics.csv"
REPORT_HTML="$REPORT_DIR/index.html"
WORKER_PIDS=()
MONITOR_PID=""

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

  echo "No container compose runtime found. Install docker compose, or set MC_COMPOSE_BIN." >&2
  return 127
}

cleanup() {
  local exit_code=$?
  touch "$STOP_FILE" >/dev/null 2>&1 || true

  if [[ -n "$MONITOR_PID" ]]; then
    wait "$MONITOR_PID" >/dev/null 2>&1 || true
  fi

  for pid in "${WORKER_PIDS[@]:-}"; do
    if [[ -n "$pid" ]] && kill -0 "$pid" >/dev/null 2>&1; then
      kill "$pid" >/dev/null 2>&1 || true
      wait "$pid" >/dev/null 2>&1 || true
    fi
  done

  if [[ $exit_code -ne 0 ]]; then
    echo "[run] perf test failed, dumping recent logs..." >&2
    compose logs --no-color || true
    for log_file in "$MASTER_LOG" "$MONITOR_LOG" "$LOG_DIR"/worker-*.log; do
      [[ -f "$log_file" ]] || continue
      echo "--- $log_file ---" >&2
      tail -n 80 "$log_file" >&2 || true
    done
  fi

  if [[ "$KEEP_ENV" == "0" ]]; then
    echo "[run] MC_KEEP_ENV=0, cleaning compose resources" >&2
    compose down -v --remove-orphans >/dev/null 2>&1 || true
  else
    echo "[run] environment kept for inspection" >&2
    echo "[run] status:  bash $SCRIPT_DIR/show_perf_status.sh $RUNTIME_DIR" >&2
    echo "[run] cleanup: bash $SCRIPT_DIR/cleanup_perf_env.sh $RUNTIME_DIR" >&2
  fi
}
trap cleanup EXIT

if [[ "$RECREATE_RUNTIME" == "1" ]]; then
  rm -rf "$RUNTIME_DIR"
fi
mkdir -p "$IMAGE_DIR" "$WORKER_OUTPUT_DIR" "$REPORT_DIR" "$LOG_DIR" "$MONITOR_DIR"
rm -f "$STOP_FILE"

echo "[run] runtime dir: $RUNTIME_DIR"
echo "[run] starting RabbitMQ perf environment"
compose up -d

echo "[run] bootstrapping RabbitMQ perf topology"
RABBITMQ_API_URL="$RABBITMQ_API_URL" \
RABBITMQ_ADMIN_USER="$RABBITMQ_ADMIN_USER" \
RABBITMQ_ADMIN_PASS="$RABBITMQ_ADMIN_PASS" \
RABBITMQ_MASTER_USER="$RABBITMQ_MASTER_USER" \
RABBITMQ_MASTER_PASS="$RABBITMQ_MASTER_PASS" \
RABBITMQ_WORKER_USER="$RABBITMQ_WORKER_USER" \
RABBITMQ_WORKER_PASS="$RABBITMQ_WORKER_PASS" \
RABBITMQ_VHOST="$RABBITMQ_VHOST" \
bash "$BOOTSTRAP_SCRIPT"

echo "[run] starting queue monitor"
python3 "$MONITOR_SCRIPT" \
  --api-url "$RABBITMQ_API_URL" \
  --admin-user "$RABBITMQ_ADMIN_USER" \
  --admin-pass "$RABBITMQ_ADMIN_PASS" \
  --vhost "$RABBITMQ_VHOST" \
  --interval-ms 1000 \
  --output "$MONITOR_CSV" \
  --stop-file "$STOP_FILE" >"$MONITOR_LOG" 2>&1 &
MONITOR_PID=$!

for index in $(seq 1 "$WORKER_COUNT"); do
  worker_id=$(printf 'worker-%02d' "$index")
  worker_log="$LOG_DIR/${worker_id}.log"
  echo "[run] launching $worker_id"
  "$WORKER_BIN" \
    --rabbitmq-uri "$WORKER_URI" \
    --worker-id "$worker_id" \
    --output-dir "$WORKER_OUTPUT_DIR" \
    --simulate-process-ms "$SIMULATE_PROCESS_MS" \
    --timeout-ms "$TIMEOUT_MS" \
    --idle-timeout-ms "$IDLE_TIMEOUT_MS" \
    --cleanup-inputs 1 \
    --stop-file "$STOP_FILE" >"$worker_log" 2>&1 &
  WORKER_PIDS+=("$!")
done

sleep 2

echo "[run] launching master benchmark"
"$MASTER_BIN" \
  --rabbitmq-uri "$MASTER_URI" \
  --image-dir "$IMAGE_DIR" \
  --report-dir "$REPORT_DIR" \
  --scenario-id "$SCENARIO_ID" \
  --worker-count "$WORKER_COUNT" \
  --task-count "$TASK_COUNT" \
  --target-rate "$TARGET_RATE" \
  --image-bytes "$IMAGE_BYTES" \
  --simulate-process-ms "$SIMULATE_PROCESS_MS" \
  --timeout-ms "$TIMEOUT_MS" \
  --notes "$NOTES" >"$MASTER_LOG" 2>&1

touch "$STOP_FILE"

for pid in "${WORKER_PIDS[@]}"; do
  wait "$pid"
done

wait "$MONITOR_PID"
MONITOR_PID=""

echo "[run] generating HTML report"
python3 "$REPORT_SCRIPT" \
  --scenario "$REPORT_DIR/scenario.txt" \
  --publish-csv "$REPORT_DIR/publish_metrics.csv" \
  --result-csv "$REPORT_DIR/result_metrics.csv" \
  --monitor-csv "$MONITOR_CSV" \
  --output "$REPORT_HTML"

test -f "$REPORT_HTML"

echo "[run] RabbitMQ perf benchmark finished"
echo "[run] report: $REPORT_HTML"
echo "[run] master log: $MASTER_LOG"
echo "[run] monitor csv: $MONITOR_CSV"
