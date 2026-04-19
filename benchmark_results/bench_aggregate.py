#!/usr/bin/env python3
"""Aggregate N redis-benchmark CSV runs into one median row per test.

Reads CSV rows on stdin (no header — `run_benchmark.sh` strips the
header from each iteration before piping in here). Groups by test
name (column 1), emits one row per test on stdout with the median of
every numeric column. Test order matches first-seen order in the
input so the aggregated CSV looks identical in shape to a single
redis-benchmark run.

Also writes a short stderr line per test reporting the spread, with a
"WARN" prefix when max/min > 1.5 (indicates a noisy or likely-buggy
iteration). That makes the multi-iteration harness do double-duty as
a smoke test: a stalled or crashed server during one iteration
produces an obvious anomaly.
"""

import csv
import statistics
import sys
from collections import OrderedDict


WARN_RATIO = 1.5  # max/min above this triggers a stderr warning


def main() -> int:
    samples: "OrderedDict[str, list[list[str]]]" = OrderedDict()
    for row in csv.reader(sys.stdin):
        if not row:
            continue
        samples.setdefault(row[0], []).append(row)

    writer = csv.writer(sys.stdout, quoting=csv.QUOTE_ALL)
    for name, rows in samples.items():
        cols = list(zip(*rows))
        out = [name]
        rps_vals: list[float] = []
        for ci, col in enumerate(cols[1:], start=1):
            try:
                vals = [float(v) for v in col]
            except ValueError:
                # non-numeric column (shouldn't happen for our format)
                out.append(col[0])
                continue
            out.append(f"{statistics.median(vals):.2f}")
            if ci == 1:
                rps_vals = vals
        writer.writerow(out)

        if len(rps_vals) >= 2:
            lo, hi = min(rps_vals), max(rps_vals)
            spread = hi / lo if lo > 0 else float("inf")
            tag = "WARN" if spread > WARN_RATIO else "ok"
            sys.stderr.write(
                f"  [{tag}] {name[:40]:<40s}  N={len(rps_vals)}  "
                f"min={lo:>10,.0f}  median={statistics.median(rps_vals):>10,.0f}  "
                f"max={hi:>10,.0f}  spread={spread:.2f}x\n"
            )
    return 0


if __name__ == "__main__":
    sys.exit(main())
