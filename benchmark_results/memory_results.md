# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |          21180 |        97680 |          23532 |     783.4 |
| 100000 |    3B | redis    |           9328 |        16240 |          10152 |      70.8 |
| 100000 |   64B | oktoplus |          21272 |       105576 |          23628 |     863.3 |
| 100000 |   64B | redis    |           9292 |        22500 |          10044 |     135.2 |
| 100000 |  256B | oktoplus |          21212 |       129140 |          23760 |    1105.2 |
| 100000 |  256B | redis    |           9356 |        46240 |          10308 |     377.7 |
| 100000 | 1024B | oktoplus |          21188 |       223692 |          24560 |    2073.6 |
| 100000 | 1024B | redis    |           9308 |       140404 |          10988 |    1342.4 |
| 1000000 |    3B | oktoplus |          21160 |       816144 |          44320 |     814.1 |
| 1000000 |    3B | redis    |           9296 |        79084 |          11540 |      71.5 |
| 1000000 |   64B | oktoplus |          21264 |       895640 |          45000 |     895.4 |
| 1000000 |   64B | redis    |           9384 |       138868 |          11844 |     132.6 |
| 1000000 |  256B | oktoplus |          21180 |      1131360 |          46808 |    1136.8 |
| 1000000 |  256B | redis    |           9340 |       375588 |          14188 |     375.0 |
| 1000000 | 1024B | oktoplus |          21160 |      2076624 |          54544 |    2104.8 |
| 1000000 | 1024B | redis    |           9348 |      1322636 |          23740 |    1344.8 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    783.4 |      70.8 |       11.06 |
| 100000 |   64B |    863.3 |     135.2 |        6.39 |
| 100000 |  256B |   1105.2 |     377.7 |        2.93 |
| 100000 | 1024B |   2073.6 |    1342.4 |        1.54 |
| 1000000 |    3B |    814.1 |      71.5 |       11.39 |
| 1000000 |   64B |    895.4 |     132.6 |        6.75 |
| 1000000 |  256B |   1136.8 |     375.0 |        3.03 |
| 1000000 | 1024B |   2104.8 |    1344.8 |        1.57 |
