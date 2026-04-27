# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |          17612 |        34504 |          23232 |     173.0 |
| 100000 |    3B | redis    |           9340 |        16248 |          10196 |      70.7 |
| 100000 |   64B | oktoplus |          17652 |        42416 |          21992 |     253.6 |
| 100000 |   64B | redis    |           9372 |        22576 |          10144 |     135.2 |
| 100000 |  256B | oktoplus |          17660 |        65976 |          26480 |     494.8 |
| 100000 |  256B | redis    |           9320 |        45684 |          10120 |     372.4 |
| 100000 | 1024B | oktoplus |          17624 |       160276 |          24004 |    1460.8 |
| 100000 | 1024B | redis    |           9248 |       140348 |          10976 |    1342.5 |
| 1000000 |    3B | oktoplus |          17548 |       234652 |          26228 |     222.3 |
| 1000000 |    3B | redis    |           9388 |        79184 |          11672 |      71.5 |
| 1000000 |   64B | oktoplus |          17628 |       315684 |          24068 |     305.2 |
| 1000000 |   64B | redis    |           9320 |       138772 |          11772 |     132.6 |
| 1000000 |  256B | oktoplus |          17620 |       550432 |          29468 |     545.6 |
| 1000000 |  256B | redis    |           9308 |       375532 |          14160 |     375.0 |
| 1000000 | 1024B | oktoplus |          17572 |      1497800 |          36224 |    1515.8 |
| 1000000 | 1024B | redis    |           9348 |      1322648 |          23812 |    1344.8 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    173.0 |      70.7 |        2.45 |
| 100000 |   64B |    253.6 |     135.2 |        1.88 |
| 100000 |  256B |    494.8 |     372.4 |        1.33 |
| 100000 | 1024B |   1460.8 |    1342.5 |        1.09 |
| 1000000 |    3B |    222.3 |      71.5 |        3.11 |
| 1000000 |   64B |    305.2 |     132.6 |        2.30 |
| 1000000 |  256B |    545.6 |     375.0 |        1.45 |
| 1000000 | 1024B |   1515.8 |    1344.8 |        1.13 |
