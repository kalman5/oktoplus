# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |           9520 |        26340 |          13564 |     172.2 |
| 100000 |    3B | redis    |           9316 |        16232 |          10204 |      70.8 |
| 100000 |   64B | oktoplus |           9520 |        34160 |          12064 |     252.3 |
| 100000 |   64B | redis    |           9292 |        22500 |          10004 |     135.2 |
| 100000 |  256B | oktoplus |           9524 |        57720 |          16124 |     493.5 |
| 100000 |  256B | redis    |           9340 |        45704 |          10052 |     372.4 |
| 100000 | 1024B | oktoplus |           9500 |       152112 |          14168 |    1460.3 |
| 100000 | 1024B | redis    |           9308 |       140388 |          10976 |    1342.3 |
| 1000000 |    3B | oktoplus |           9520 |       220032 |          16320 |     215.6 |
| 1000000 |    3B | redis    |           9324 |        79104 |          11548 |      71.5 |
| 1000000 |   64B | oktoplus |           9492 |       301524 |          14196 |     299.0 |
| 1000000 |   64B | redis    |           9292 |       138752 |          11744 |     132.6 |
| 1000000 |  256B | oktoplus |           9500 |       542304 |          19604 |     545.6 |
| 1000000 |  256B | redis    |           9296 |       375544 |          14160 |     375.0 |
| 1000000 | 1024B | oktoplus |           9520 |      1487116 |          26408 |    1513.1 |
| 1000000 | 1024B | redis    |           9344 |      1322644 |          23756 |    1344.8 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    172.2 |      70.8 |        2.43 |
| 100000 |   64B |    252.3 |     135.2 |        1.87 |
| 100000 |  256B |    493.5 |     372.4 |        1.33 |
| 100000 | 1024B |   1460.3 |    1342.3 |        1.09 |
| 1000000 |    3B |    215.6 |      71.5 |        3.02 |
| 1000000 |   64B |    299.0 |     132.6 |        2.25 |
| 1000000 |  256B |    545.6 |     375.0 |        1.45 |
| 1000000 | 1024B |   1513.1 |    1344.8 |        1.13 |
