#!/usr/bin/env bash
set -euo pipefail

API_URL="${RABBITMQ_API_URL:-http://127.0.0.1:15673/api}"
ADMIN_USER="${RABBITMQ_ADMIN_USER:-mc_perf_admin}"
ADMIN_PASS="${RABBITMQ_ADMIN_PASS:-mc_perf_admin_secret}"
VHOST="${RABBITMQ_VHOST:-mc_perf}"
MASTER_USER="${RABBITMQ_MASTER_USER:-mc_perf_master}"
MASTER_PASS="${RABBITMQ_MASTER_PASS:-perf_master_secret}"
WORKER_USER="${RABBITMQ_WORKER_USER:-mc_perf_worker}"
WORKER_PASS="${RABBITMQ_WORKER_PASS:-perf_worker_secret}"

request() {
  local method="$1"
  local path="$2"
  local body="${3:-}"
  if [[ -n "$body" ]]; then
    curl -fsS -u "$ADMIN_USER:$ADMIN_PASS" \
      -H 'content-type: application/json' \
      -X "$method" "$API_URL$path" \
      -d "$body" >/dev/null
  else
    curl -fsS -u "$ADMIN_USER:$ADMIN_PASS" \
      -H 'content-type: application/json' \
      -X "$method" "$API_URL$path" >/dev/null
  fi
}

for _ in $(seq 1 60); do
  if curl -fsS -u "$ADMIN_USER:$ADMIN_PASS" "$API_URL/overview" >/dev/null 2>&1; then
    break
  fi
  sleep 1
done

curl -fsS -u "$ADMIN_USER:$ADMIN_PASS" "$API_URL/overview" >/dev/null

request PUT "/vhosts/$VHOST"
request PUT "/users/$MASTER_USER" "{\"password\":\"$MASTER_PASS\",\"tags\":\"\"}"
request PUT "/users/$WORKER_USER" "{\"password\":\"$WORKER_PASS\",\"tags\":\"\"}"

request PUT "/exchanges/$VHOST/mc.perf.task.exchange" '{"type":"direct","durable":true,"auto_delete":false,"internal":false,"arguments":{}}'
request PUT "/exchanges/$VHOST/mc.perf.result.exchange" '{"type":"direct","durable":true,"auto_delete":false,"internal":false,"arguments":{}}'
request PUT "/queues/$VHOST/mc.perf.task.queue" '{"durable":true,"auto_delete":false,"arguments":{}}'
request PUT "/queues/$VHOST/mc.perf.result.queue" '{"durable":true,"auto_delete":false,"arguments":{}}'
request POST "/bindings/$VHOST/e/mc.perf.task.exchange/q/mc.perf.task.queue" '{"routing_key":"task.dispatch","arguments":{}}'
request POST "/bindings/$VHOST/e/mc.perf.result.exchange/q/mc.perf.result.queue" '{"routing_key":"result.ready","arguments":{}}'
request DELETE "/queues/$VHOST/mc.perf.task.queue/contents"
request DELETE "/queues/$VHOST/mc.perf.result.queue/contents"

request PUT "/permissions/$VHOST/$MASTER_USER" '{"configure":"^(mc\\.perf\\.task\\.exchange|mc\\.perf\\.result\\.queue)$","write":"^(mc\\.perf\\.task\\.exchange)$","read":"^(mc\\.perf\\.result\\.queue)$"}'
request PUT "/permissions/$VHOST/$WORKER_USER" '{"configure":"^(mc\\.perf\\.result\\.exchange|mc\\.perf\\.task\\.queue)$","write":"^(mc\\.perf\\.result\\.exchange)$","read":"^(mc\\.perf\\.task\\.queue)$"}'

echo "RabbitMQ perf bootstrap completed for vhost '$VHOST'."
echo "  master user: $MASTER_USER"
echo "  worker user: $WORKER_USER"
