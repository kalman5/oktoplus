#!/usr/bin/env python3
"""Generate benchmark charts from benchmark_results/raw/*.csv.

Outputs:
  * chart_p16.svg          — static SVG, embedded in README.md
  * chart_concurrency.svg  — static SVG, embedded in README.md
  * report.html            — interactive Chart.js dashboard, viewable
                             via htmlpreview.github.io

No external Python dependencies (pure stdlib), so it runs on a bare
devserver after every benchmark run.
"""

import csv
import json
import pathlib
import sys


HERE = pathlib.Path(__file__).resolve().parent
RAW = HERE / "raw"


# ---- CSV helpers ------------------------------------------------------------


def read_rps(path: pathlib.Path) -> dict[str, float]:
    """Map test name -> rps."""
    out: dict[str, float] = {}
    with path.open() as f:
        reader = csv.reader(f)
        next(reader)  # header
        for row in reader:
            if not row:
                continue
            test = row[0].strip()
            out[test] = float(row[1])
    return out


def short_name(test: str) -> str:
    """Strip redis-benchmark's verbose annotations to a short label."""
    if test.startswith("LPUSH (needed"):
        return "LPUSH (warmup)"
    if test.startswith("LRANGE_100"):
        return "LRANGE_100"
    if " " in test:
        # "RPUSH mylist__rand_int__ val" -> "RPUSH"
        return test.split()[0]
    return test


# ---- SVG helpers ------------------------------------------------------------


def svg_open(width: int, height: int) -> list[str]:
    # The SVG is loaded as its own document by GitHub's markdown renderer,
    # so prefers-color-scheme works. We supply two themes:
    #   * light mode: white background, dark text
    #   * dark mode:  GitHub-dark background (#0d1117), light text
    # Bar/line colors stay constant — they're chosen to be readable on both.
    return [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}" font-family="-apple-system, Segoe UI, '
        'Helvetica, Arial, sans-serif" font-size="12">',
        "<style>"
        ".bg{fill:#ffffff}"
        ".title{font-size:15px;font-weight:600;fill:#24292f}"
        ".axis{stroke:#57606a;stroke-width:1}"
        ".gridline{stroke:#d0d7de;stroke-width:1}"
        ".label{fill:#24292f}"
        ".legend{font-size:12px;fill:#24292f}"
        ".bar-okto{fill:#3fb950}.bar-redis{fill:#f85149}"
        "@media (prefers-color-scheme: dark){"
        ".bg{fill:#0d1117}"
        ".title,.label,.legend{fill:#c9d1d9}"
        ".axis{stroke:#8b949e}"
        ".gridline{stroke:#21262d}"
        "}"
        "</style>",
        f'<rect class="bg" width="{width}" height="{height}"/>',
    ]


def svg_close() -> list[str]:
    return ["</svg>"]


def grouped_bar_chart(
    title: str,
    series: list[tuple[str, list[float], str]],  # (label, values, css class)
    categories: list[str],
    y_max: float,
    out_path: pathlib.Path,
) -> None:
    """series: list of (legend_label, values_per_category, css_class)."""
    width, height = 880, 420
    margin_l, margin_r, margin_t, margin_b = 70, 20, 60, 70
    plot_w = width - margin_l - margin_r
    plot_h = height - margin_t - margin_b
    n = len(categories)
    n_series = len(series)
    group_w = plot_w / n
    bar_w = group_w * 0.36
    bar_gap = group_w * 0.05

    parts = svg_open(width, height)
    # Title
    parts.append(
        f'<text class="title" x="{width/2}" y="22" text-anchor="middle">{title}</text>'
    )

    # Y gridlines + labels
    n_yticks = 5
    for i in range(n_yticks + 1):
        v = y_max * i / n_yticks
        y = margin_t + plot_h - (v / y_max) * plot_h
        parts.append(
            f'<line class="gridline" x1="{margin_l}" y1="{y}" '
            f'x2="{width - margin_r}" y2="{y}"/>'
        )
        parts.append(
            f'<text class="label" x="{margin_l - 8}" y="{y + 4}" '
            f'text-anchor="end">{int(v):,}</text>'
        )

    # Y axis
    parts.append(
        f'<line class="axis" x1="{margin_l}" y1="{margin_t}" '
        f'x2="{margin_l}" y2="{margin_t + plot_h}"/>'
    )
    # X axis
    parts.append(
        f'<line class="axis" x1="{margin_l}" y1="{margin_t + plot_h}" '
        f'x2="{width - margin_r}" y2="{margin_t + plot_h}"/>'
    )

    # Bars
    for ci, cat in enumerate(categories):
        group_x = margin_l + ci * group_w + group_w / 2
        # category label
        parts.append(
            f'<text class="label" x="{group_x}" y="{margin_t + plot_h + 18}" '
            f'text-anchor="middle">{cat}</text>'
        )
        for si, (_, values, css) in enumerate(series):
            v = values[ci]
            bar_h = (v / y_max) * plot_h
            x = group_x + (si - (n_series - 1) / 2) * (bar_w + bar_gap) - bar_w / 2
            y = margin_t + plot_h - bar_h
            parts.append(
                f'<rect class="{css}" x="{x:.1f}" y="{y:.1f}" '
                f'width="{bar_w:.1f}" height="{bar_h:.1f}"/>'
            )

    # Legend
    legend_x = margin_l
    legend_y = height - 18
    for si, (label, _, css) in enumerate(series):
        sx = legend_x + si * 160
        parts.append(
            f'<rect class="{css}" x="{sx}" y="{legend_y - 11}" width="14" height="14"/>'
        )
        parts.append(
            f'<text class="legend" x="{sx + 20}" y="{legend_y}">{label}</text>'
        )

    parts.extend(svg_close())
    out_path.write_text("\n".join(parts) + "\n")


def line_chart(
    title: str,
    x_values: list[int],
    series: list[tuple[str, list[float], str]],  # (label, y values, color)
    x_label: str,
    y_max: float,
    out_path: pathlib.Path,
) -> None:
    width, height = 880, 380
    margin_l, margin_r, margin_t, margin_b = 70, 20, 60, 60
    plot_w = width - margin_l - margin_r
    plot_h = height - margin_t - margin_b

    parts = svg_open(width, height)
    parts.append(
        f'<text class="title" x="{width/2}" y="22" text-anchor="middle">{title}</text>'
    )

    n_yticks = 5
    for i in range(n_yticks + 1):
        v = y_max * i / n_yticks
        y = margin_t + plot_h - (v / y_max) * plot_h
        parts.append(
            f'<line class="gridline" x1="{margin_l}" y1="{y}" '
            f'x2="{width - margin_r}" y2="{y}"/>'
        )
        parts.append(
            f'<text class="label" x="{margin_l - 8}" y="{y + 4}" '
            f'text-anchor="end">{int(v):,}</text>'
        )

    parts.append(
        f'<line class="axis" x1="{margin_l}" y1="{margin_t}" '
        f'x2="{margin_l}" y2="{margin_t + plot_h}"/>'
    )
    parts.append(
        f'<line class="axis" x1="{margin_l}" y1="{margin_t + plot_h}" '
        f'x2="{width - margin_r}" y2="{margin_t + plot_h}"/>'
    )

    # X positions: log-friendly even spacing
    n = len(x_values)
    xs = [margin_l + (i / max(n - 1, 1)) * plot_w for i in range(n)]

    for i, xv in enumerate(x_values):
        parts.append(
            f'<text class="label" x="{xs[i]}" y="{margin_t + plot_h + 18}" '
            f'text-anchor="middle">{xv}</text>'
        )

    parts.append(
        f'<text class="label" x="{width/2}" y="{height - 28}" '
        f'text-anchor="middle">{x_label}</text>'
    )

    for label, ys, color in series:
        pts = []
        for i, yv in enumerate(ys):
            y = margin_t + plot_h - (yv / y_max) * plot_h
            pts.append(f"{xs[i]:.1f},{y:.1f}")
        parts.append(
            f'<polyline fill="none" stroke="{color}" stroke-width="2.5" '
            f'points="{" ".join(pts)}"/>'
        )
        for i, yv in enumerate(ys):
            y = margin_t + plot_h - (yv / y_max) * plot_h
            parts.append(
                f'<circle cx="{xs[i]:.1f}" cy="{y:.1f}" r="3.5" fill="{color}"/>'
            )

    legend_x = margin_l
    legend_y = height - 8
    for si, (label, _, color) in enumerate(series):
        sx = legend_x + si * 160
        parts.append(
            f'<rect x="{sx}" y="{legend_y - 11}" width="14" height="14" fill="{color}"/>'
        )
        parts.append(
            f'<text class="legend" x="{sx + 20}" y="{legend_y}">{label}</text>'
        )

    parts.extend(svg_close())
    out_path.write_text("\n".join(parts) + "\n")


# ---- HTML report (Chart.js) ------------------------------------------------


HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Oktoplus vs Redis — benchmark report</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.7/dist/chart.umd.min.js"></script>
<style>
  * {{ box-sizing: border-box; margin: 0; padding: 0; }}
  body {{ font-family: -apple-system, "Segoe UI", Helvetica, Arial, sans-serif;
         background: #0d1117; color: #c9d1d9; padding: 2rem; }}
  h1 {{ color: #58a6ff; font-size: 1.8rem; margin-bottom: 0.5rem; }}
  h2 {{ color: #8b949e; font-size: 1rem; font-weight: normal; margin-bottom: 2rem; }}
  h3 {{ color: #58a6ff; font-size: 1.2rem; margin: 2rem 0 1rem;
        border-bottom: 1px solid #21262d; padding-bottom: 0.5rem; }}
  .grid {{ display: grid; grid-template-columns: 1fr 1fr; gap: 2rem; }}
  .full {{ grid-column: 1 / -1; }}
  .box {{ background: #161b22; border: 1px solid #21262d; border-radius: 8px;
          padding: 1.25rem; }}
  canvas {{ max-height: 380px; }}
  .meta {{ color: #6e7681; font-size: 0.85rem; margin-top: 1.5rem; }}
  @media (max-width: 800px) {{ .grid {{ grid-template-columns: 1fr; }} }}
</style>
</head>
<body>
  <h1>Oktoplus vs Redis</h1>
  <h2>{subtitle}</h2>

  <h3>Single client, no pipelining (-P 1) — network-bound</h3>
  <div class="box full"><canvas id="c_p1"></canvas></div>

  <h3>Single client, pipelined (-P 16) — CPU-bound</h3>
  <div class="box full"><canvas id="c_p16"></canvas></div>

  <h3>LPUSH on a hot key, varying clients</h3>
  <div class="box full"><canvas id="c_conc"></canvas></div>

  <p class="meta">Generated by <code>benchmark_results/make_chart.py</code>
     from <code>benchmark_results/raw/*.csv</code>.</p>

<script>
const RED = '#f85149';
const GRN = '#3fb950';
const RED_BG = 'rgba(248,81,73,0.20)';
const GRN_BG = 'rgba(63,185,80,0.20)';

Chart.defaults.color = '#c9d1d9';
Chart.defaults.borderColor = '#21262d';
Chart.defaults.font.family = '-apple-system, "Segoe UI", Helvetica, Arial, sans-serif';

const data = {data_json};

function bar(id, p) {{
  new Chart(document.getElementById(id).getContext('2d'), {{
    type: 'bar',
    data: {{
      labels: p.labels,
      datasets: [
        {{ label: 'Redis',    data: p.redis, backgroundColor: RED_BG, borderColor: RED, borderWidth: 1 }},
        {{ label: 'Oktoplus', data: p.okto,  backgroundColor: GRN_BG, borderColor: GRN, borderWidth: 1 }}
      ]
    }},
    options: {{
      responsive: true, maintainAspectRatio: false,
      scales: {{ y: {{ beginAtZero: true, title: {{ display: true, text: 'rps' }} }} }},
      plugins: {{ legend: {{ position: 'top' }} }}
    }}
  }});
}}

function line(id, p) {{
  new Chart(document.getElementById(id).getContext('2d'), {{
    type: 'line',
    data: {{
      labels: p.labels,
      datasets: [
        {{ label: 'Redis',    data: p.redis, borderColor: RED, backgroundColor: RED_BG, fill: false, tension: 0.2 }},
        {{ label: 'Oktoplus', data: p.okto,  borderColor: GRN, backgroundColor: GRN_BG, fill: false, tension: 0.2 }}
      ]
    }},
    options: {{
      responsive: true, maintainAspectRatio: false,
      scales: {{
        y: {{ beginAtZero: true, title: {{ display: true, text: 'rps' }} }},
        x: {{ title: {{ display: true, text: 'concurrent clients (-c)' }} }}
      }},
      plugins: {{ legend: {{ position: 'top' }} }}
    }}
  }});
}}

bar('c_p1',  data.p1);
bar('c_p16', data.p16);
line('c_conc', data.conc);
</script>
</body>
</html>
"""


def write_html_report(
    out_path: pathlib.Path,
    p1: dict,
    p16: dict,
    conc: dict,
    subtitle: str,
) -> None:
    payload = {"p1": p1, "p16": p16, "conc": conc}
    out_path.write_text(
        HTML_TEMPLATE.format(
            subtitle=subtitle, data_json=json.dumps(payload, indent=2)
        )
    )


# ---- Main ------------------------------------------------------------------


def main() -> int:
    okto_p16 = read_rps(RAW / "speed_oktoplus_p16.csv")
    redis_p16 = read_rps(RAW / "speed_redis_p16.csv")

    # Pick a stable display order matching the README table.
    p16_tests = [
        "LPUSH",
        "SADD",
        "LRANGE_100 (first 100 elements)",
        "RPUSH mylist__rand_int__ val",
        "LPOP mylist__rand_int__",
        "RPOP mylist__rand_int__",
        "LLEN mylist__rand_int__",
        "SCARD myset__rand_int__",
    ]
    cats = [short_name(t) for t in p16_tests]
    okto_vals = [okto_p16[t] for t in p16_tests]
    redis_vals = [redis_p16[t] for t in p16_tests]
    y_max = max(max(okto_vals), max(redis_vals)) * 1.10

    grouped_bar_chart(
        title="Single client, pipelined (-P 16) — rps (higher is better)",
        series=[
            ("Oktoplus", okto_vals, "bar-okto"),
            ("Redis", redis_vals, "bar-redis"),
        ],
        categories=cats,
        y_max=y_max,
        out_path=HERE / "chart_p16.svg",
    )

    # Concurrency sweep on LPUSH (hot key).
    concurrencies = [1, 10, 50, 100, 200]
    okto_lpush, redis_lpush = [], []
    for c in concurrencies:
        okto_lpush.append(read_rps(RAW / f"parallel_oktoplus_c{c}.csv")["LPUSH"])
        redis_lpush.append(read_rps(RAW / f"parallel_redis_c{c}.csv")["LPUSH"])

    line_chart(
        title="LPUSH on a hot key, varying clients (-P 1) — rps (higher is better)",
        x_values=concurrencies,
        series=[
            ("Oktoplus", okto_lpush, "#3fb950"),
            ("Redis", redis_lpush, "#f85149"),
        ],
        x_label="concurrent clients",
        y_max=max(max(okto_lpush), max(redis_lpush)) * 1.10,
        out_path=HERE / "chart_concurrency.svg",
    )

    # Interactive HTML report (Chart.js, viewable via htmlpreview.github.io).
    okto_p1 = read_rps(RAW / "speed_oktoplus_p1.csv")
    redis_p1 = read_rps(RAW / "speed_redis_p1.csv")

    write_html_report(
        out_path=HERE / "report.html",
        subtitle=(
            "Single client unless stated otherwise · 100k ops · 100k key-space · "
            "Oktoplus optimized build (-O3 -march=native -fno-semantic-interposition)"
        ),
        p1={
            "labels": cats,
            "okto":  [okto_p1[t]  for t in p16_tests],
            "redis": [redis_p1[t] for t in p16_tests],
        },
        p16={
            "labels": cats,
            "okto":  okto_vals,
            "redis": redis_vals,
        },
        conc={
            "labels": [str(c) for c in concurrencies],
            "okto":  okto_lpush,
            "redis": redis_lpush,
        },
    )

    print("wrote chart_p16.svg, chart_concurrency.svg, report.html")
    return 0


if __name__ == "__main__":
    sys.exit(main())
