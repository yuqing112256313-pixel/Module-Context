#!/usr/bin/env bash
set -euo pipefail

API_URL="${RABBITMQ_API_URL:-http://127.0.0.1:15672/api}"
ADMIN_USER="${RABBITMQ_ADMIN_USER:-mc_admin}"
ADMIN_PASS="${RABBITMQ_ADMIN_PASS:-mc_admin_secret}"
VHOST="${RABBITMQ_VHOST:-mc_integration}"
MASTER_USER="${RABBITMQ_MASTER_USER:-mc_master}"
MASTER_PASS="${RABBITMQ_MASTER_PASS:-master_secret}"
WORKER_USER="${RABBITMQ_WORKER_USER:-mc_worker}"
WORKER_PASS="${RABBITMQ_WORKER_PASS:-worker_secret}"

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

request PUT "/exchanges/$VHOST/mc.task.exchange" '{"type":"direct","durable":true,"auto_delete":false,"internal":false,"arguments":{}}'
request PUT "/exchanges/$VHOST/mc.result.exchange" '{"type":"direct","durable":true,"auto_delete":false,"internal":false,"arguments":{}}'
request PUT "/queues/$VHOST/mc.task.queue" '{"durable":true,"auto_delete":false,"arguments":{}}'
request PUT "/queues/$VHOST/mc.result.queue" '{"durable":true,"auto_delete":false,"arguments":{}}'
request POST "/bindings/$VHOST/e/mc.task.exchange/q/mc.task.queue" '{"routing_key":"task.dispatch","arguments":{}}'
request POST "/bindings/$VHOST/e/mc.result.exchange/q/mc.result.queue" '{"routing_key":"result.ready","arguments":{}}'
request DELETE "/queues/$VHOST/mc.task.queue/contents"
request DELETE "/queues/$VHOST/mc.result.queue/contents"

request PUT "/permissions/$VHOST/$MASTER_USER" '{"configure":"^(mc\\.task\\.exchange|mc\\.result\\.queue)$","write":"^(mc\\.task\\.exchange)$","read":"^(mc\\.result\\.queue)$"}'
request PUT "/permissions/$VHOST/$WORKER_USER" '{"configure":"^(mc\\.result\\.exchange|mc\\.task\\.queue)$","write":"^(mc\\.result\\.exchange)$","read":"^(mc\\.task\\.queue)$"}'

echo "RabbitMQ bootstrap completed for vhost '$VHOST'."
echo "  master user: $MASTER_USER"
echo "  worker user: $WORKER_USER"
