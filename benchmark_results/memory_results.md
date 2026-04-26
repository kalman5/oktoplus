# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |          21432 |        38256 |          26852 |     172.3 |
| 100000 |    3B | redis    |           9392 |        16272 |           9780 |      70.5 |
| 100000 |   64B | oktoplus |          21584 |        46260 |          27708 |     252.7 |
| 100000 |   64B | redis    |           9308 |        22516 |           9988 |     135.2 |
| 100000 |  256B | oktoplus |          21480 |        69704 |          28648 |     493.8 |
| 100000 |  256B | redis    |           9340 |        45700 |          10080 |     372.3 |
| 100000 | 1024B | oktoplus |          21468 |       164108 |          27944 |    1460.6 |
| 100000 | 1024B | redis    |           9300 |       140388 |          10976 |    1342.3 |
| 1000000 |    3B | oktoplus |          21424 |       238468 |          29968 |     222.3 |
| 1000000 |    3B | redis    |           9324 |        79116 |          11556 |      71.5 |
| 1000000 |   64B | oktoplus |          21500 |       319556 |          30496 |     305.2 |
| 1000000 |   64B | redis    |           9376 |       138852 |          11832 |     132.6 |
| 1000000 |  256B | oktoplus |          21480 |       555612 |          31740 |     547.0 |
| 1000000 |  256B | redis    |           9344 |       375580 |          14192 |     375.0 |
| 1000000 | 1024B | oktoplus |          21468 |      1501628 |          38868 |    1515.7 |
| 1000000 | 1024B | redis    |           9300 |      1322588 |          23700 |    1344.8 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    172.3 |      70.5 |        2.44 |
| 100000 |   64B |    252.7 |     135.2 |        1.87 |
| 100000 |  256B |    493.8 |     372.3 |        1.33 |
| 100000 | 1024B |   1460.6 |    1342.3 |        1.09 |
| 1000000 |    3B |    222.3 |      71.5 |        3.11 |
| 1000000 |   64B |    305.2 |     132.6 |        2.30 |
| 1000000 |  256B |    547.0 |     375.0 |        1.46 |
| 1000000 | 1024B |   1515.7 |    1344.8 |        1.13 |
