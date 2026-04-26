# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |          21368 |        38200 |          26848 |     172.4 |
| 100000 |    3B | redis    |           9304 |        16212 |          10076 |      70.7 |
| 100000 |   64B | oktoplus |          21428 |        46104 |          27624 |     252.7 |
| 100000 |   64B | redis    |           9300 |        22508 |          10060 |     135.2 |
| 100000 |  256B | oktoplus |          21444 |        69672 |          28616 |     493.9 |
| 100000 |  256B | redis    |           9296 |        45680 |          10072 |     372.6 |
| 100000 | 1024B | oktoplus |          21408 |       164060 |          27752 |    1460.8 |
| 100000 | 1024B | redis    |           9328 |       140416 |          11004 |    1342.3 |
| 1000000 |    3B | oktoplus |          21532 |       239896 |          29840 |     223.6 |
| 1000000 |    3B | redis    |           9292 |        79084 |          11516 |      71.5 |
| 1000000 |   64B | oktoplus |          21396 |       319464 |          30388 |     305.2 |
| 1000000 |   64B | redis    |           9296 |       138764 |          11748 |     132.6 |
| 1000000 |  256B | oktoplus |          21436 |       554224 |          31680 |     545.6 |
| 1000000 |  256B | redis    |           9332 |       375580 |          14180 |     375.0 |
| 1000000 | 1024B | oktoplus |          21424 |      1502908 |          38840 |    1517.0 |
| 1000000 | 1024B | redis    |           9352 |      1322648 |          23752 |    1344.8 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    172.4 |      70.7 |        2.44 |
| 100000 |   64B |    252.7 |     135.2 |        1.87 |
| 100000 |  256B |    493.9 |     372.6 |        1.33 |
| 100000 | 1024B |   1460.8 |    1342.3 |        1.09 |
| 1000000 |    3B |    223.6 |      71.5 |        3.13 |
| 1000000 |   64B |    305.2 |     132.6 |        2.30 |
| 1000000 |  256B |    545.6 |     375.0 |        1.45 |
| 1000000 | 1024B |   1517.0 |    1344.8 |        1.13 |
