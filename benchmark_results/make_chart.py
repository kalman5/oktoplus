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
    """Strip redis-benchmark's verbose annotations to a short label.

    redis-benchmark's built-in tests (LPUSH, SADD, LRANGE_100) use a
    fixed key — the workload hammers a single hot container. Our
    explicit per-command rows (RPUSH/LPOP/RPOP/LLEN/SCARD on
    mylist__rand_int__) substitute __rand_int__ with a random integer,
    spreading load across 100k distinct keys. Tag the random-key
    variants explicitly so the chart doesn't silently compare apples
    and oranges.
    """
    if test.startswith("LPUSH (needed"):
        return "LPUSH (warmup)"
    if test.startswith("LRANGE_100"):
        return "LRANGE_100"
    if "__rand_int__" in test:
        # "RPUSH mylist__rand_int__ val" -> "RPUSH (rand)"
        return f"{test.split()[0]} (rand)"
    return test


# ---- SVG helpers ------------------------------------------------------------


def svg_open(width: int, height: int) -> list[str]:
    # GitHub serves SVG via <img> and sanitises away <style>/@media
    # queries, so we can't theme via prefers-color-scheme. Instead we
    # paint a fixed white background with dark text — always readable
    # regardless of the surrounding page theme. All colors are inline
    # attributes (no class refs).
    return [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}" font-family="-apple-system, Segoe UI, '
        'Helvetica, Arial, sans-serif" font-size="12">',
        f'<rect width="{width}" height="{height}" fill="#ffffff"/>',
    ]


def svg_close() -> list[str]:
    return ["</svg>"]


TEXT = "#24292f"
AXIS = "#57606a"
GRID = "#d0d7de"


def grouped_bar_chart(
    title: str,
    series: list[tuple[str, list[float], str]],  # (label, values, hex color)
    categories: list[str],
    y_max: float,
    out_path: pathlib.Path,
    # Optional horizontal reference lines (e.g. baseline RSS). Each
    # entry is (legend label, y value, hex color). Drawn as dashed
    # lines spanning the plot area; labelled in the legend so the
    # reader knows what each line means.
    reference_lines: list[tuple[str, float, str]] | None = None,
) -> None:
    width, height = 880, 420
    margin_l, margin_r, margin_t, margin_b = 70, 20, 60, 70
    plot_w = width - margin_l - margin_r
    plot_h = height - margin_t - margin_b
    n_series = len(series)
    group_w = plot_w / len(categories)
    bar_w = group_w * 0.36
    bar_gap = group_w * 0.05

    parts = svg_open(width, height)
    parts.append(
        f'<text x="{width/2}" y="22" text-anchor="middle" '
        f'fill="{TEXT}" font-size="15" font-weight="600">{title}</text>'
    )

    # Y gridlines + tick labels
    n_yticks = 5
    for i in range(n_yticks + 1):
        v = y_max * i / n_yticks
        y = margin_t + plot_h - (v / y_max) * plot_h
        parts.append(
            f'<line x1="{margin_l}" y1="{y}" x2="{width - margin_r}" y2="{y}" '
            f'stroke="{GRID}" stroke-width="1"/>'
        )
        parts.append(
            f'<text x="{margin_l - 8}" y="{y + 4}" text-anchor="end" '
            f'fill="{TEXT}">{int(v):,}</text>'
        )

    # Axes
    parts.append(
        f'<line x1="{margin_l}" y1="{margin_t}" x2="{margin_l}" '
        f'y2="{margin_t + plot_h}" stroke="{AXIS}" stroke-width="1"/>'
    )
    parts.append(
        f'<line x1="{margin_l}" y1="{margin_t + plot_h}" '
        f'x2="{width - margin_r}" y2="{margin_t + plot_h}" '
        f'stroke="{AXIS}" stroke-width="1"/>'
    )

    # Bars
    for ci, cat in enumerate(categories):
        group_x = margin_l + ci * group_w + group_w / 2
        parts.append(
            f'<text x="{group_x}" y="{margin_t + plot_h + 18}" '
            f'text-anchor="middle" fill="{TEXT}">{cat}</text>'
        )
        for si, (_, values, color) in enumerate(series):
            v = values[ci]
            bar_h = (v / y_max) * plot_h
            x = group_x + (si - (n_series - 1) / 2) * (bar_w + bar_gap) - bar_w / 2
            y = margin_t + plot_h - bar_h
            parts.append(
                f'<rect x="{x:.1f}" y="{y:.1f}" width="{bar_w:.1f}" '
                f'height="{bar_h:.1f}" fill="{color}"/>'
            )

    # Reference lines (e.g. baseline RSS). Drawn after bars so they
    # sit on top, dashed so they're visually distinct from the axes.
    if reference_lines:
        for label, val, color in reference_lines:
            if val > y_max or val < 0:
                continue
            y = margin_t + plot_h - (val / y_max) * plot_h
            parts.append(
                f'<line x1="{margin_l}" y1="{y:.1f}" '
                f'x2="{width - margin_r}" y2="{y:.1f}" '
                f'stroke="{color}" stroke-width="1.5" '
                f'stroke-dasharray="6,4"/>'
            )

    # Legend
    legend_y = height - 18
    legend_entries = list(series) + [
        (lbl, None, col) for (lbl, _v, col) in (reference_lines or [])
    ]
    for si, (label, _v, color) in enumerate(legend_entries):
        sx = margin_l + si * 200
        # Bars get a filled swatch, reference lines a dashed line so the
        # legend matches what's drawn in the plot.
        if _v is None:
            parts.append(
                f'<line x1="{sx}" y1="{legend_y - 4}" '
                f'x2="{sx + 14}" y2="{legend_y - 4}" '
                f'stroke="{color}" stroke-width="2" '
                f'stroke-dasharray="4,3"/>'
            )
        else:
            parts.append(
                f'<rect x="{sx}" y="{legend_y - 11}" width="14" height="14" '
                f'fill="{color}"/>'
            )
        parts.append(
            f'<text x="{sx + 20}" y="{legend_y}" fill="{TEXT}">{label}</text>'
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
        f'<text x="{width/2}" y="22" text-anchor="middle" '
        f'fill="{TEXT}" font-size="15" font-weight="600">{title}</text>'
    )

    n_yticks = 5
    for i in range(n_yticks + 1):
        v = y_max * i / n_yticks
        y = margin_t + plot_h - (v / y_max) * plot_h
        parts.append(
            f'<line x1="{margin_l}" y1="{y}" x2="{width - margin_r}" y2="{y}" '
            f'stroke="{GRID}" stroke-width="1"/>'
        )
        parts.append(
            f'<text x="{margin_l - 8}" y="{y + 4}" text-anchor="end" '
            f'fill="{TEXT}">{int(v):,}</text>'
        )

    parts.append(
        f'<line x1="{margin_l}" y1="{margin_t}" x2="{margin_l}" '
        f'y2="{margin_t + plot_h}" stroke="{AXIS}" stroke-width="1"/>'
    )
    parts.append(
        f'<line x1="{margin_l}" y1="{margin_t + plot_h}" '
        f'x2="{width - margin_r}" y2="{margin_t + plot_h}" '
        f'stroke="{AXIS}" stroke-width="1"/>'
    )

    n = len(x_values)
    xs = [margin_l + (i / max(n - 1, 1)) * plot_w for i in range(n)]

    for i, xv in enumerate(x_values):
        parts.append(
            f'<text x="{xs[i]}" y="{margin_t + plot_h + 18}" '
            f'text-anchor="middle" fill="{TEXT}">{xv}</text>'
        )

    parts.append(
        f'<text x="{width/2}" y="{height - 28}" text-anchor="middle" '
        f'fill="{TEXT}">{x_label}</text>'
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

    legend_y = height - 8
    for si, (label, _, color) in enumerate(series):
        sx = margin_l + si * 160
        parts.append(
            f'<rect x="{sx}" y="{legend_y - 11}" width="14" height="14" fill="{color}"/>'
        )
        parts.append(
            f'<text x="{sx + 20}" y="{legend_y}" fill="{TEXT}">{label}</text>'
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

  <h3>Single client, pipelined (-P 16) — CPU-bound (small values)</h3>
  <div class="box full"><canvas id="c_p16"></canvas></div>

  <h3>Single client, pipelined (-P 16) — large 256-byte values</h3>
  <div class="box full"><canvas id="c_p16_d256"></canvas></div>

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
bar('c_p16_d256', data.p16_d256);
line('c_conc', data.conc);
</script>
</body>
</html>
"""


def write_html_report(
    out_path: pathlib.Path,
    p1: dict,
    p16: dict,
    p16_d256: dict,
    conc: dict,
    subtitle: str,
) -> None:
    payload = {"p1": p1, "p16": p16, "p16_d256": p16_d256, "conc": conc}
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
            ("Oktoplus", okto_vals, "#3fb950"),
            ("Redis", redis_vals, "#f85149"),
        ],
        categories=cats,
        y_max=y_max,
        out_path=HERE / "chart_p16.svg",
    )

    def lookup(d, prefix):
        for k, v in d.items():
            if k.startswith(prefix):
                return v
        raise KeyError(prefix)

    # Concurrency sweep on LPUSH (hot key).
    concurrencies = [1, 10, 50, 100, 200]
    okto_lpush, redis_lpush = [], []
    for c in concurrencies:
        okto_lpush.append(read_rps(RAW / f"parallel_oktoplus_c{c}.csv")["LPUSH"])
        redis_lpush.append(read_rps(RAW / f"parallel_redis_c{c}.csv")["LPUSH"])

    line_chart(
        title="LPUSH (hot key, fixed name), varying clients (-P 1) — rps (higher is better)",
        x_values=concurrencies,
        series=[
            ("Oktoplus", okto_lpush, "#3fb950"),
            ("Redis", redis_lpush, "#f85149"),
        ],
        x_label="concurrent clients",
        y_max=max(max(okto_lpush), max(redis_lpush)) * 1.10,
        out_path=HERE / "chart_concurrency.svg",
    )

    # Concurrent + pipelined + random key scaling chart, on RPUSH.
    # Optional — only emit if the CSVs from PART 4 exist.
    rand_concurrencies = [10, 50, 100, 200]
    okto_rand_rpush, redis_rand_rpush = [], []
    have_rand = True
    for c in rand_concurrencies:
        okto_path = RAW / f"concurrent_random_oktoplus_c{c}_p16.csv"
        redis_path = RAW / f"concurrent_random_redis_c{c}_p16.csv"
        if not okto_path.exists() or not redis_path.exists():
            have_rand = False
            break
        okto_rand_rpush.append(lookup(read_rps(okto_path), "RPUSH "))
        redis_rand_rpush.append(lookup(read_rps(redis_path), "RPUSH "))
    if have_rand:
        line_chart(
            title="RPUSH (random key, -P 16), varying clients — rps (higher is better)",
            x_values=rand_concurrencies,
            series=[
                ("Oktoplus", okto_rand_rpush, "#3fb950"),
                ("Redis", redis_rand_rpush, "#f85149"),
            ],
            x_label="concurrent clients",
            y_max=max(max(okto_rand_rpush), max(redis_rand_rpush)) * 1.10,
            out_path=HERE / "chart_concurrency_random.svg",
        )

    # Large-value (-d 256) speed test, same shape as p16 but the value
    # is 256 bytes. The custom RPUSH row's name contains the value
    # ("aaaa…"), so look it up by prefix.
    okto_p16_d256 = read_rps(RAW / "speed_oktoplus_p16_d256.csv")
    redis_p16_d256 = read_rps(RAW / "speed_redis_p16_d256.csv")

    okto_d256 = [
        okto_p16_d256["LPUSH"],
        okto_p16_d256["SADD"],
        okto_p16_d256["LRANGE_100 (first 100 elements)"],
        lookup(okto_p16_d256, "RPUSH "),
        okto_p16_d256["LPOP mylist__rand_int__"],
        okto_p16_d256["RPOP mylist__rand_int__"],
        okto_p16_d256["LLEN mylist__rand_int__"],
        okto_p16_d256["SCARD myset__rand_int__"],
    ]
    redis_d256 = [
        redis_p16_d256["LPUSH"],
        redis_p16_d256["SADD"],
        redis_p16_d256["LRANGE_100 (first 100 elements)"],
        lookup(redis_p16_d256, "RPUSH "),
        redis_p16_d256["LPOP mylist__rand_int__"],
        redis_p16_d256["RPOP mylist__rand_int__"],
        redis_p16_d256["LLEN mylist__rand_int__"],
        redis_p16_d256["SCARD myset__rand_int__"],
    ]
    y_max_d256 = max(max(okto_d256), max(redis_d256)) * 1.10

    grouped_bar_chart(
        title="Single client, -P 16, large 256-byte values — rps (higher is better)",
        series=[
            ("Oktoplus", okto_d256, "#3fb950"),
            ("Redis", redis_d256, "#f85149"),
        ],
        categories=cats,
        y_max=y_max_d256,
        out_path=HERE / "chart_p16_d256.svg",
    )

    # -P 1 single-client SVG chart. Matches the rows in the README's
    # -P 1 table exactly (RPUSH-rand and the LRANGE-seed LPUSH are
    # omitted there, so they're omitted here too).
    okto_p1 = read_rps(RAW / "speed_oktoplus_p1.csv")
    redis_p1 = read_rps(RAW / "speed_redis_p1.csv")

    p1_tests = [
        "LPUSH",
        "SADD",
        "LRANGE_100 (first 100 elements)",
        "LPOP mylist__rand_int__",
        "RPOP mylist__rand_int__",
        "LLEN mylist__rand_int__",
        "SCARD myset__rand_int__",
    ]
    p1_cats = [short_name(t) for t in p1_tests]
    okto_p1_vals = [okto_p1[t] for t in p1_tests]
    redis_p1_vals = [redis_p1[t] for t in p1_tests]
    grouped_bar_chart(
        title="Single client, no pipelining (-P 1) — rps (higher is better)",
        series=[
            ("Oktoplus", okto_p1_vals, "#3fb950"),
            ("Redis", redis_p1_vals, "#f85149"),
        ],
        categories=p1_cats,
        y_max=max(max(okto_p1_vals), max(redis_p1_vals)) * 1.10,
        out_path=HERE / "chart_p1.svg",
    )

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
        p16_d256={
            "labels": cats,
            "okto":  okto_d256,
            "redis": redis_d256,
        },
        conc={
            "labels": [str(c) for c in concurrencies],
            "okto":  okto_lpush,
            "redis": redis_lpush,
        },
    )

    # Memory charts (only emit if run_memory.sh has produced raw/memory.csv).
    #   chart_memory.svg          : steady-state bytes/key per value size
    #   chart_memory_residual.svg : residual RSS (KiB) after FLUSHALL,
    #                               per (N, value-size) — shows the
    #                               allocator-retention behaviour
    mem_csv = RAW / "memory.csv"
    extra_lines = []
    if mem_csv.exists():
        import csv as _csv
        from collections import defaultdict
        per_server_bpk = defaultdict(lambda: defaultdict(list))
        per_server_rows = defaultdict(list)
        with mem_csv.open() as fh:
            for row in _csv.DictReader(fh):
                vs = int(row["value_size_b"])
                per_server_bpk[row["server"]][vs].append(float(row["bytes_per_key"]))
                per_server_rows[row["server"]].append(row)

        sizes = sorted({vs for d in per_server_bpk.values() for vs in d})
        if sizes and "oktoplus" in per_server_bpk and "redis" in per_server_bpk:
            okto_bpk = [sum(per_server_bpk["oktoplus"][s]) / len(per_server_bpk["oktoplus"][s])
                        for s in sizes]
            redis_bpk = [sum(per_server_bpk["redis"][s]) / len(per_server_bpk["redis"][s])
                         for s in sizes]
            line_chart(
                title="Memory footprint — bytes per key (lower is better)",
                x_values=sizes,
                series=[
                    ("Oktoplus", okto_bpk, "#3fb950"),
                    ("Redis",    redis_bpk, "#f85149"),
                ],
                x_label="value size (bytes)",
                y_max=max(max(okto_bpk), max(redis_bpk)) * 1.10,
                out_path=HERE / "chart_memory.svg",
            )
            extra_lines.append("chart_memory.svg")

        # Residual chart: index trials by (n_keys, value_size) so the
        # two servers line up per case. Only emit if both servers have
        # at least one trial per case.
        def _label(n: int, size: int) -> str:
            n_lbl = f"{n // 1000}k" if n < 1_000_000 else f"{n // 1_000_000}M"
            return f"{n_lbl}/{size}B"

        cases = {}  # (n, size) -> {server: residual_kib}
        for server, rows in per_server_rows.items():
            for r in rows:
                key = (int(r["n_keys"]), int(r["value_size_b"]))
                cases.setdefault(key, {})[server] = float(r["residual_rss_kib"])
        complete_cases = sorted(
            k for k, v in cases.items() if "oktoplus" in v and "redis" in v
        )
        if complete_cases:
            cats = [_label(n, s) for (n, s) in complete_cases]
            okto_residual = [cases[k]["oktoplus"] for k in complete_cases]
            redis_residual = [cases[k]["redis"] for k in complete_cases]
            # Mean baseline RSS per server across the sweep. Baselines
            # are essentially constant per server (server framework
            # cost, no payload), so the mean is a reasonable summary.
            # Drawn as horizontal reference lines so the reader can see
            # "residual is X above baseline" at a glance.
            def _baseline(server: str) -> float:
                xs = [float(r["baseline_rss_kib"])
                      for r in per_server_rows[server]]
                return sum(xs) / len(xs) if xs else 0.0
            okto_baseline  = _baseline("oktoplus")
            redis_baseline = _baseline("redis")
            grouped_bar_chart(
                title="Residual RSS after FLUSHALL — KiB (lower is better)",
                series=[
                    ("Oktoplus",          okto_residual, "#3fb950"),
                    ("Redis",             redis_residual, "#f85149"),
                ],
                categories=cats,
                y_max=max(max(okto_residual), max(redis_residual)) * 1.10,
                out_path=HERE / "chart_memory_residual.svg",
                reference_lines=[
                    (f"Oktoplus baseline ({okto_baseline/1024:.1f} MiB)",
                     okto_baseline,  "#3fb950"),
                    (f"Redis baseline ({redis_baseline/1024:.1f} MiB)",
                     redis_baseline, "#f85149"),
                ],
            )
            extra_lines.append("chart_memory_residual.svg")

    # Parallelism-advantage chart: LPOS scan on a list of N elements,
    # multi-key, varying clients. Pure CPU per command (~5 bytes wire),
    # so Redis stays capped at single-core ceiling and Oktoplus scales
    # with cores. Only emit if run_parallelism_advantage_bench.sh has
    # produced raw/parallelism_*.csv at -P 16 with N=10000.
    par_okto = RAW / "parallelism_oktoplus.csv"
    par_redis = RAW / "parallelism_redis.csv"
    if par_okto.exists() and par_redis.exists():
        import csv as _csv
        def _load_par(path):
            # Returns clients -> (rps, cpu_cores) where cpu_cores =
            # server_cpu_pct / 100 (i.e. how many full cores were
            # saturated on average). cpu column is optional -- older
            # CSVs without it just yield 0.0.
            out = {}
            with path.open() as fh:
                for row in _csv.DictReader(fh):
                    if row["test"] != "LPOS_miss":
                        continue
                    if int(row["N_elements"]) != 10000:
                        continue
                    cpu_pct = float(row.get("server_cpu_pct") or 0)
                    out[int(row["clients"])] = (float(row["rps"]),
                                                cpu_pct / 100.0)
            return out
        po = _load_par(par_okto)
        pr = _load_par(par_redis)
        clients = sorted(set(po) & set(pr))
        if clients:
            okto_par  = [po[c][0] for c in clients]
            redis_par = [pr[c][0] for c in clients]
            line_chart(
                title=("LPOS scan on 10K-element lists, multi-key, varying "
                       "clients (-P 16) — rps (higher is better)"),
                x_values=clients,
                series=[
                    ("Oktoplus", okto_par,  "#3fb950"),
                    ("Redis",    redis_par, "#f85149"),
                ],
                x_label="concurrent clients",
                y_max=max(max(okto_par), max(redis_par)) * 1.10,
                out_path=HERE / "chart_parallelism.svg",
            )
            extra_lines.append("chart_parallelism.svg")

            # Companion chart: cores-saturated per concurrency. Same
            # workload, same axis, but plots `server_cpu_pct / 100`
            # so the architectural story is visible on a hardware-
            # independent metric (cores used scales with the design,
            # not with the host's clock speed). Redis caps near 1
            # (single-thread); Oktoplus scales with -c.
            okto_cores  = [po[c][1] for c in clients]
            redis_cores = [pr[c][1] for c in clients]
            cores_max = max(max(okto_cores), max(redis_cores)) * 1.10
            if cores_max > 0:
                line_chart(
                    title=("Server cores saturated during LPOS scan "
                           "(server_cpu_pct / 100, higher = more parallelism)"),
                    x_values=clients,
                    series=[
                        ("Oktoplus", okto_cores,  "#3fb950"),
                        ("Redis",    redis_cores, "#f85149"),
                    ],
                    x_label="concurrent clients",
                    y_max=cores_max,
                    out_path=HERE / "chart_parallelism_cpu.svg",
                )
                extra_lines.append("chart_parallelism_cpu.svg")

    print(
        "wrote chart_p1.svg, chart_p16.svg, chart_p16_d256.svg, "
        "chart_concurrency.svg, chart_concurrency_random.svg, report.html"
        + (", " + ", ".join(extra_lines) if extra_lines else "")
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
