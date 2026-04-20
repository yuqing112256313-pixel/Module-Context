#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${MC_BUILD_DIR:-$ROOT_DIR/build}"
SUITE="${1:-all}"
ENV_FILE="${MC_INTEGRATION_ENV_FILE:-$SCRIPT_DIR/integration.defaults.env}"

if [[ -f "$ENV_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$ENV_FILE"
fi

usage() {
  cat <<USAGE
Usage: $0 [all|task-flow|perf]

Environment variables:
  MC_BUILD_DIR            CMake build directory (default: <repo>/build)
  MC_KEEP_ENV             1 to keep RabbitMQ containers/runtime artifacts (default: 1)
  MC_RABBITMQ_ENV         auto|compose|external (default: auto)
  MC_COMPOSE_BIN          Override compose command, e.g. "docker compose" or "podman compose"
  RABBITMQ_API_URL        RabbitMQ management API URL (default: task-flow/perf script defaults)
  RABBITMQ_ADMIN_USER     RabbitMQ admin user (used for API availability probe)
  RABBITMQ_ADMIN_PASS     RabbitMQ admin password (used for API availability probe)
  MC_SKIP_CONFIGURE       1 to skip CMake configure step
  MC_SKIP_BUILD           1 to skip build step
  MC_INTEGRATION_ENV_FILE Override defaults env file (default: tests/integration/integration.defaults.env)
USAGE
}

if [[ "$SUITE" == "-h" || "$SUITE" == "--help" ]]; then
  usage
  exit 0
fi

if [[ "$SUITE" != "all" && "$SUITE" != "task-flow" && "$SUITE" != "perf" ]]; then
  echo "[error] unsupported suite: $SUITE" >&2
  usage
  exit 2
fi

compose_available() {
  if [[ -n "${MC_COMPOSE_BIN:-}" ]]; then
    return 0
  fi

  if command -v docker >/dev/null 2>&1 && docker compose version >/dev/null 2>&1; then
    return 0
  fi

  if command -v docker-compose >/dev/null 2>&1; then
    return 0
  fi

  return 1
}

rabbitmq_api_ready() {
  local api_url="${RABBITMQ_API_URL:-http://127.0.0.1:15672/api}"
  local admin_user="${RABBITMQ_ADMIN_USER:-guest}"
  local admin_pass="${RABBITMQ_ADMIN_PASS:-guest}"

  if ! command -v curl >/dev/null 2>&1; then
    return 1
  fi

  curl -fsS -u "$admin_user:$admin_pass" "$api_url/overview" >/dev/null 2>&1
}

resolve_bin() {
  local base="$1"
  if [[ -x "$base" ]]; then
    echo "$base"
    return 0
  fi
  if [[ -x "${base}.exe" ]]; then
    echo "${base}.exe"
    return 0
  fi
  echo "[error] executable not found: $base (or ${base}.exe)" >&2
  return 1
}

run_setup() {
  mkdir -p "$BUILD_DIR"

  if [[ "${MC_SKIP_CONFIGURE:-0}" != "1" ]]; then
    echo "[setup] configuring CMake (MC_BUILD_TESTS=ON, MC_BUILD_REAL_RABBITMQ_TESTS=ON)"
    cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
      -DMC_BUILD_TESTS=ON \
      -DMC_BUILD_REAL_RABBITMQ_TESTS=ON
  fi

  if [[ "${MC_SKIP_BUILD:-0}" != "1" ]]; then
    echo "[setup] building test binaries"
    cmake --build "$BUILD_DIR" -j
  fi
}

run_task_flow() {
  local master_bin
  local worker_bin
  local runtime_dir="$BUILD_DIR/tests/rabbitmq_task_flow_runtime"

  master_bin="$(resolve_bin "$BUILD_DIR/tests/mc_rabbitmq_task_master_sim")"
  worker_bin="$(resolve_bin "$BUILD_DIR/tests/mc_rabbitmq_task_worker_sim")"

  echo "[test] running RabbitMQ task-flow integration"
  bash "$SCRIPT_DIR/rabbitmq_task_flow/run_task_flow_test.sh" \
    "$master_bin" "$worker_bin" "$runtime_dir"
}

run_perf() {
  local master_bin
  local worker_bin
  local runtime_dir="$BUILD_DIR/tests/rabbitmq_perf_runtime"

  master_bin="$(resolve_bin "$BUILD_DIR/tests/mc_rabbitmq_perf_master")"
  worker_bin="$(resolve_bin "$BUILD_DIR/tests/mc_rabbitmq_perf_worker")"

  echo "[test] running RabbitMQ performance integration"
  bash "$SCRIPT_DIR/rabbitmq_perf/run_perf_test.sh" \
    "$master_bin" "$worker_bin" "$runtime_dir"
}

run_setup

RABBITMQ_ENV_MODE="${MC_RABBITMQ_ENV:-auto}"
if [[ "$RABBITMQ_ENV_MODE" != "auto" && "$RABBITMQ_ENV_MODE" != "compose" && "$RABBITMQ_ENV_MODE" != "external" ]]; then
  echo "[error] unsupported MC_RABBITMQ_ENV: $RABBITMQ_ENV_MODE (expected: auto|compose|external)" >&2
  exit 2
fi

if [[ "$RABBITMQ_ENV_MODE" == "auto" ]]; then
  if compose_available; then
    export MC_RABBITMQ_ENV="compose"
    echo "[env] MC_RABBITMQ_ENV=compose (compose runtime detected)"
  elif rabbitmq_api_ready; then
    export MC_RABBITMQ_ENV="external"
    echo "[env] MC_RABBITMQ_ENV=external (using pre-installed local RabbitMQ)"
  else
    echo "[error] neither compose runtime nor reachable RabbitMQ API found." >&2
    echo "[hint] offline Windows recommended flow: start local RabbitMQ and set MC_RABBITMQ_ENV=external," >&2
    echo "       and optionally set RABBITMQ_API_URL/RABBITMQ_ADMIN_USER/RABBITMQ_ADMIN_PASS." >&2
    exit 1
  fi
else
  export MC_RABBITMQ_ENV="$RABBITMQ_ENV_MODE"
  echo "[env] MC_RABBITMQ_ENV=$MC_RABBITMQ_ENV"
fi

case "$SUITE" in
  task-flow)
    run_task_flow
    ;;
  perf)
    run_perf
    ;;
  all)
    run_task_flow
    run_perf
    ;;
esac

echo "[done] integration suite '$SUITE' completed"
