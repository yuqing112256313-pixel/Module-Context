#!/usr/bin/env python3
import argparse
import base64
import csv
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request


def request_json(api_url, user, password, path):
    request = urllib.request.Request(api_url.rstrip("/") + path)
    token = base64.b64encode(f"{user}:{password}".encode("utf-8")).decode("ascii")
    request.add_header("Authorization", f"Basic {token}")
    with urllib.request.urlopen(request, timeout=5) as response:
        return json.loads(response.read().decode("utf-8"))


def queue_stats(api_url, user, password, vhost, queue_name):
    encoded_vhost = urllib.parse.quote(vhost, safe="")
    encoded_queue = urllib.parse.quote(queue_name, safe="")
    payload = request_json(api_url, user, password, f"/queues/{encoded_vhost}/{encoded_queue}")
    return {
        "messages": int(payload.get("messages", 0)),
        "messages_ready": int(payload.get("messages_ready", 0)),
        "messages_unacknowledged": int(payload.get("messages_unacknowledged", 0)),
        "consumers": int(payload.get("consumers", 0)),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--api-url", required=True)
    parser.add_argument("--admin-user", required=True)
    parser.add_argument("--admin-pass", required=True)
    parser.add_argument("--vhost", required=True)
    parser.add_argument("--interval-ms", type=int, default=1000)
    parser.add_argument("--output", required=True)
    parser.add_argument("--stop-file", required=True)
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    started = int(time.time() * 1000)

    with open(args.output, "w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow([
            "sample_ts_ms",
            "elapsed_ms",
            "task_messages",
            "task_ready",
            "task_unacked",
            "task_consumers",
            "result_messages",
            "result_ready",
            "result_unacked",
            "result_consumers",
        ])
        handle.flush()

        while True:
            now_ms = int(time.time() * 1000)
            try:
                task = queue_stats(args.api_url, args.admin_user, args.admin_pass, args.vhost, "mc.perf.task.queue")
                result = queue_stats(args.api_url, args.admin_user, args.admin_pass, args.vhost, "mc.perf.result.queue")
                writer.writerow([
                    now_ms,
                    now_ms - started,
                    task["messages"],
                    task["messages_ready"],
                    task["messages_unacknowledged"],
                    task["consumers"],
                    result["messages"],
                    result["messages_ready"],
                    result["messages_unacknowledged"],
                    result["consumers"],
                ])
                handle.flush()
            except urllib.error.URLError as error:
                writer.writerow([now_ms, now_ms - started, "error", str(error), "", "", "", "", "", ""])
                handle.flush()

            if os.path.exists(args.stop_file):
                break
            time.sleep(max(args.interval_ms, 100) / 1000.0)

    return 0


if __name__ == "__main__":
    sys.exit(main())
