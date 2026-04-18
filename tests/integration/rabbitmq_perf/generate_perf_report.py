#!/usr/bin/env python3
import argparse
import csv
import html
import math
import os
import statistics
from collections import Counter, defaultdict


def parse_kv_file(path):
    data = {}
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line or "=" not in line:
                continue
            key, value = line.split("=", 1)
            data[key] = value
    return data


def parse_csv(path):
    with open(path, "r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def to_int(value, default=0):
    try:
        if value is None or value == "":
            return default
        return int(float(value))
    except ValueError:
        return default


def to_float(value, default=0.0):
    try:
        if value is None or value == "":
            return default
        return float(value)
    except ValueError:
        return default


def percentile(values, p):
    if not values:
        return 0.0
    if len(values) == 1:
        return float(values[0])
    values = sorted(values)
    rank = (len(values) - 1) * p
    low = int(math.floor(rank))
    high = int(math.ceil(rank))
    if low == high:
        return float(values[low])
    fraction = rank - low
    return values[low] + (values[high] - values[low]) * fraction


def format_ms(value):
    return f"{value:.1f} ms"


def format_rate(value):
    return f"{value:.2f} /s"


def format_bytes(value):
    if value <= 0:
        return "0 B"
    units = ["B", "KB", "MB", "GB", "TB"]
    size = float(value)
    unit = 0
    while size >= 1024.0 and unit < len(units) - 1:
        size /= 1024.0
        unit += 1
    return f"{size:.2f} {units[unit]}"


def duration_seconds(rows, field):
    values = [to_int(row.get(field, "0")) for row in rows if str(row.get(field, "")).strip()]
    if len(values) < 2:
        return 0.0
    return max(values) / 1000.0 - min(values) / 1000.0


def svg_line_chart(points, width=760, height=220, color="#2563eb", title=""):
    if not points:
        return "<p>No samples</p>"
    xs = [x for x, _ in points]
    ys = [y for _, y in points]
    min_x, max_x = min(xs), max(xs)
    min_y, max_y = min(ys), max(ys)
    if max_x == min_x:
        max_x += 1
    if max_y == min_y:
        max_y += 1

    def scale_x(value):
        return 40 + (value - min_x) * (width - 60) / float(max_x - min_x)

    def scale_y(value):
        return height - 30 - (value - min_y) * (height - 50) / float(max_y - min_y)

    polyline = " ".join(f"{scale_x(x):.2f},{scale_y(y):.2f}" for x, y in points)
    return f'''
    <svg width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img" aria-label="{html.escape(title)}">
      <rect x="0" y="0" width="{width}" height="{height}" fill="#fff" stroke="#e5e7eb"/>
      <line x1="40" y1="20" x2="40" y2="{height - 30}" stroke="#9ca3af"/>
      <line x1="40" y1="{height - 30}" x2="{width - 20}" y2="{height - 30}" stroke="#9ca3af"/>
      <polyline fill="none" stroke="{color}" stroke-width="2.5" points="{polyline}"/>
      <text x="44" y="18" fill="#374151" font-size="12">{html.escape(title)}</text>
      <text x="44" y="{height - 8}" fill="#6b7280" font-size="11">0s</text>
      <text x="{width - 52}" y="{height - 8}" fill="#6b7280" font-size="11">{max_x:.1f}s</text>
      <text x="6" y="26" fill="#6b7280" font-size="11">{max_y:.0f}</text>
      <text x="6" y="{height - 34}" fill="#6b7280" font-size="11">{min_y:.0f}</text>
    </svg>
    '''


def summary_table(rows):
    body = "".join(
        f"<tr><th>{html.escape(str(k))}</th><td>{html.escape(str(v))}</td></tr>" for k, v in rows
    )
    return f"<table class='summary'>{body}</table>"


def metric_table(title, metrics):
    rows = "".join(
        f"<tr><th>{html.escape(name)}</th><td>{html.escape(value)}</td></tr>" for name, value in metrics
    )
    return f"<section><h3>{html.escape(title)}</h3><table class='summary'>{rows}</table></section>"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--scenario", required=True)
    parser.add_argument("--publish-csv", required=True)
    parser.add_argument("--result-csv", required=True)
    parser.add_argument("--monitor-csv", required=False)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    scenario = parse_kv_file(args.scenario)
    publish_rows = parse_csv(args.publish_csv)
    result_rows = parse_csv(args.result_csv)
    monitor_rows = parse_csv(args.monitor_csv) if args.monitor_csv and os.path.exists(args.monitor_csv) else []

    completed_rows = [row for row in result_rows if row.get("status")]
    success_rows = [row for row in completed_rows if row.get("status") == "processed"]
    failure_rows = [row for row in completed_rows if row.get("status") != "processed"]

    publish_duration = duration_seconds(publish_rows, "publish_ts_ms")
    completion_duration = duration_seconds(completed_rows, "master_receive_ts_ms") if completed_rows else 0.0
    published_count = len(publish_rows)
    completed_count = len(completed_rows)
    success_count = len(success_rows)
    failure_count = len(failure_rows)

    image_write = [to_float(row.get("image_write_ms")) for row in publish_rows]
    publish_call = [to_float(row.get("publish_call_ms")) for row in publish_rows]
    queue_wait = [to_float(row.get("queue_wait_ms")) for row in success_rows]
    read_ms = [to_float(row.get("read_ms")) for row in success_rows]
    process_ms = [to_float(row.get("process_ms")) for row in success_rows]
    write_ms = [to_float(row.get("write_ms")) for row in success_rows]
    cleanup_ms = [to_float(row.get("cleanup_ms")) for row in success_rows]
    end_to_end_ms = [to_float(row.get("end_to_end_ms")) for row in success_rows]

    publish_rate = (published_count / publish_duration) if publish_duration > 0 else 0.0
    complete_rate = (success_count / completion_duration) if completion_duration > 0 else 0.0

    worker_counter = Counter(row.get("worker_id", "unknown") for row in success_rows)
    worker_latency = defaultdict(list)
    for row in success_rows:
        worker_latency[row.get("worker_id", "unknown")].append(to_float(row.get("end_to_end_ms")))

    peak_task_ready = max((to_int(row.get("task_ready")) for row in monitor_rows if str(row.get("task_ready", "")).isdigit()), default=0)
    peak_task_unacked = max((to_int(row.get("task_unacked")) for row in monitor_rows if str(row.get("task_unacked", "")).isdigit()), default=0)
    peak_result_ready = max((to_int(row.get("result_ready")) for row in monitor_rows if str(row.get("result_ready", "")).isdigit()), default=0)

    queue_points = [
        (to_float(row.get("elapsed_ms")) / 1000.0, to_float(row.get("task_ready")))
        for row in monitor_rows
        if str(row.get("task_ready", "")).isdigit()
    ]
    unacked_points = [
        (to_float(row.get("elapsed_ms")) / 1000.0, to_float(row.get("task_unacked")))
        for row in monitor_rows
        if str(row.get("task_unacked", "")).isdigit()
    ]
    result_points = [
        (to_float(row.get("elapsed_ms")) / 1000.0, to_float(row.get("result_ready")))
        for row in monitor_rows
        if str(row.get("result_ready", "")).isdigit()
    ]

    worker_rows = []
    for worker_id, count in sorted(worker_counter.items()):
        latencies = worker_latency[worker_id]
        worker_rows.append(
            f"<tr><th>{html.escape(worker_id)}</th><td>{count}</td><td>{format_ms(statistics.mean(latencies) if latencies else 0.0)}</td></tr>"
        )
    worker_table = (
        "<table class='summary'><tr><th>Worker</th><th>Completed</th><th>Avg end-to-end</th></tr>"
        + "".join(worker_rows)
        + "</table>"
    )

    highlights = [
        ("发布任务数", str(published_count)),
        ("完成任务数", str(completed_count)),
        ("成功任务数", str(success_count)),
        ("失败任务数", str(failure_count)),
        ("目标吞吐", format_rate(to_float(scenario.get("target_rate", "0")))),
        ("实际发布吞吐", format_rate(publish_rate)),
        ("实际完成吞吐", format_rate(complete_rate)),
        ("端到端延迟 P50 / P95 / P99",
         f"{format_ms(percentile(end_to_end_ms, 0.50))} / {format_ms(percentile(end_to_end_ms, 0.95))} / {format_ms(percentile(end_to_end_ms, 0.99))}"),
        ("任务队列峰值 backlog", str(peak_task_ready)),
        ("任务队列峰值 unacked", str(peak_task_unacked)),
    ]

    environment = [
        ("测试模式", "单机 RabbitMQ 拟真压测，一主五从竞争消费"),
        ("消息链路", "主进程写 20MB 左右图片到共享目录，发布 task_id + image_path；5 个 worker 竞争消费，模拟处理后发布结果；主进程汇总结果"),
        ("处理模型", f"处理耗时为模拟 sleep，当前配置 {scenario.get('simulate_process_ms', '0')} ms/张；不代表真实算法耗时"),
        ("真实背景说明", "真实业务更接近多机台 + 共享内存/共享存储，本次是单机本地磁盘 + 本地 RabbitMQ + 本地 5 进程拟真，重点看调度、排队、磁盘 IO 与消息总线表现"),
        ("图片大小", format_bytes(to_int(scenario.get("image_bytes", "0")))),
        ("任务总数", scenario.get("task_count", "0")),
        ("Worker 数", scenario.get("worker_count", "0")),
        ("场景 ID", scenario.get("scenario_id", "")),
    ]

    bottlenecks = [
        ("主机写图 P50 / P95", f"{format_ms(percentile(image_write, 0.50))} / {format_ms(percentile(image_write, 0.95))}"),
        ("主机 publish 调用 P50 / P95", f"{format_ms(percentile(publish_call, 0.50))} / {format_ms(percentile(publish_call, 0.95))}"),
        ("队列等待 P50 / P95", f"{format_ms(percentile(queue_wait, 0.50))} / {format_ms(percentile(queue_wait, 0.95))}"),
        ("worker 读图 P50 / P95", f"{format_ms(percentile(read_ms, 0.50))} / {format_ms(percentile(read_ms, 0.95))}"),
        ("worker 模拟处理 P50 / P95", f"{format_ms(percentile(process_ms, 0.50))} / {format_ms(percentile(process_ms, 0.95))}"),
        ("worker 写回 P50 / P95", f"{format_ms(percentile(write_ms, 0.50))} / {format_ms(percentile(write_ms, 0.95))}"),
        ("worker 清理 P50 / P95", f"{format_ms(percentile(cleanup_ms, 0.50))} / {format_ms(percentile(cleanup_ms, 0.95))}"),
    ]

    notes = scenario.get("notes", "")
    html_text = f"""
<!doctype html>
<html lang='zh-CN'>
<head>
  <meta charset='utf-8' />
  <title>RabbitMQ Perf Report</title>
  <style>
    body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; margin: 24px; color: #111827; background: #f9fafb; }}
    h1, h2, h3 {{ margin-bottom: 8px; }}
    p {{ line-height: 1.5; }}
    .grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(340px, 1fr)); gap: 16px; }}
    .card {{ background: white; border: 1px solid #e5e7eb; border-radius: 12px; padding: 16px; box-shadow: 0 1px 2px rgba(0,0,0,0.04); }}
    table.summary {{ width: 100%; border-collapse: collapse; }}
    table.summary th, table.summary td {{ padding: 8px 10px; border-bottom: 1px solid #e5e7eb; text-align: left; vertical-align: top; }}
    table.summary th {{ width: 42%; color: #374151; font-weight: 600; }}
    .muted {{ color: #6b7280; }}
    .charts svg {{ width: 100%; height: auto; }}
    .badge {{ display: inline-block; padding: 4px 10px; border-radius: 999px; background: #dbeafe; color: #1d4ed8; font-size: 12px; margin-right: 8px; }}
  </style>
</head>
<body>
  <h1>RabbitMQ 一主五从压测报告</h1>
  <p class='muted'>场景 ID: {html.escape(scenario.get('scenario_id', ''))}</p>
  <p>
    <span class='badge'>单机场景</span>
    <span class='badge'>20MB 级图片</span>
    <span class='badge'>5 个 worker 竞争消费</span>
    <span class='badge'>RabbitMQ 实例独立运行</span>
  </p>

  <div class='grid'>
    <div class='card'>
      <h2>最关键结果</h2>
      {summary_table(highlights)}
    </div>
    <div class='card'>
      <h2>测试条件 / 背景</h2>
      {summary_table(environment)}
    </div>
  </div>

  <div class='grid'>
    <div class='card'>
      <h2>关键瓶颈指标</h2>
      {summary_table(bottlenecks)}
    </div>
    <div class='card'>
      <h2>Worker 分布</h2>
      {worker_table}
    </div>
  </div>

  <div class='grid charts'>
    <div class='card'>
      <h2>任务队列 backlog</h2>
      {svg_line_chart(queue_points, color='#dc2626', title='task queue ready backlog')}
    </div>
    <div class='card'>
      <h2>任务队列 unacked</h2>
      {svg_line_chart(unacked_points, color='#ea580c', title='task queue unacked')}
    </div>
    <div class='card'>
      <h2>结果队列 backlog</h2>
      {svg_line_chart(result_points, color='#16a34a', title='result queue ready backlog')}
    </div>
  </div>

  <div class='card'>
    <h2>结论建议</h2>
    <ul>
      <li>如果实际完成吞吐明显低于目标 60 张/秒，优先看主机写图耗时、worker 读写耗时、task queue backlog 峰值。</li>
      <li>如果 queue_wait 明显高，说明 5 个 worker 总处理能力或磁盘带宽已经跟不上发布速度。</li>
      <li>当前 harness 默认在 worker 处理后删除输入图和输出图，避免压测时磁盘被持续写满。</li>
      <li>这份结果更适合比较“改动前后”趋势，而不是直接当成真实多机产线容量值。</li>
    </ul>
    <p class='muted'>{html.escape(notes)}</p>
  </div>
</body>
</html>
"""

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as handle:
        handle.write(html_text)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
