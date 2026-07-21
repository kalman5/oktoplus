# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |           8676 |        20964 |           8420 |     125.8 |
| 100000 |    3B | redis    |           8592 |        14736 |          10596 |      62.9 |
| 100000 |   64B | oktoplus |           8708 |        27908 |           9140 |     196.6 |
| 100000 |   64B | redis    |           8576 |        21632 |           9840 |     133.7 |
| 100000 |  256B | oktoplus |           8640 |        47040 |           9868 |     393.2 |
| 100000 |  256B | redis    |           8564 |        45428 |          10208 |     377.5 |
| 100000 | 1024B | oktoplus |           7984 |       123952 |          11544 |    1187.5 |
| 100000 | 1024B | redis    |           8600 |       139928 |          10440 |    1344.8 |
| 1000000 |    3B | oktoplus |           8704 |       154584 |          10044 |     149.4 |
| 1000000 |    3B | redis    |           8576 |        78464 |          10900 |      71.6 |
| 1000000 |   64B | oktoplus |           8600 |       219024 |          11484 |     215.5 |
| 1000000 |   64B | redis    |           8676 |       138676 |          11592 |     133.1 |
| 1000000 |  256B | oktoplus |           8636 |       413336 |          18416 |     414.4 |
| 1000000 |  256B | redis    |           8572 |       373844 |          12380 |     374.0 |
| 1000000 | 1024B | oktoplus |           8676 |      1188244 |          43812 |    1207.9 |
| 1000000 | 1024B | redis    |           8588 |      1322348 |          23872 |    1345.3 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    125.8 |      62.9 |        2.00 |
| 100000 |   64B |    196.6 |     133.7 |        1.47 |
| 100000 |  256B |    393.2 |     377.5 |        1.04 |
| 100000 | 1024B |   1187.5 |    1344.8 |        0.88 |
| 1000000 |    3B |    149.4 |      71.6 |        2.09 |
| 1000000 |   64B |    215.5 |     133.1 |        1.62 |
| 1000000 |  256B |    414.4 |     374.0 |        1.11 |
| 1000000 | 1024B |   1207.9 |    1345.3 |        0.90 |
