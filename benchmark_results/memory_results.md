# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |          17616 |        34444 |          23188 |     172.3 |
| 100000 |    3B | redis    |           9388 |        16300 |          10292 |      70.8 |
| 100000 |   64B | oktoplus |          17520 |        42276 |          21856 |     253.5 |
| 100000 |   64B | redis    |           9372 |        22580 |          10124 |     135.2 |
| 100000 |  256B | oktoplus |          17600 |        65904 |          25984 |     494.6 |
| 100000 |  256B | redis    |           9332 |        45692 |          10092 |     372.3 |
| 100000 | 1024B | oktoplus |          17636 |       160352 |          24264 |    1461.4 |
| 100000 | 1024B | redis    |           9324 |       140416 |          11004 |    1342.4 |
| 1000000 |    3B | oktoplus |          17620 |       234660 |          26280 |     222.2 |
| 1000000 |    3B | redis    |           9332 |        79128 |          11580 |      71.5 |
| 1000000 |   64B | oktoplus |          17596 |       315716 |          24120 |     305.3 |
| 1000000 |   64B | redis    |           9320 |       138784 |          11780 |     132.6 |
| 1000000 |  256B | oktoplus |          17592 |       550456 |          29528 |     545.7 |
| 1000000 |  256B | redis    |           9320 |       375544 |          14168 |     375.0 |
| 1000000 | 1024B | oktoplus |          17604 |      1497832 |          36256 |    1515.8 |
| 1000000 | 1024B | redis    |           9304 |      1322592 |          23708 |    1344.8 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    172.3 |      70.8 |        2.43 |
| 100000 |   64B |    253.5 |     135.2 |        1.88 |
| 100000 |  256B |    494.6 |     372.3 |        1.33 |
| 100000 | 1024B |   1461.4 |    1342.4 |        1.09 |
| 1000000 |    3B |    222.2 |      71.5 |        3.11 |
| 1000000 |   64B |    305.3 |     132.6 |        2.30 |
| 1000000 |  256B |    545.7 |     375.0 |        1.46 |
| 1000000 | 1024B |   1515.8 |    1344.8 |        1.13 |
