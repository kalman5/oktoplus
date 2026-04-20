# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |          21208 |        37872 |          21376 |     170.6 |
| 100000 |    3B | redis    |           9384 |        16264 |           9768 |      70.5 |
| 100000 |   64B | oktoplus |          21236 |        45812 |          21504 |     251.7 |
| 100000 |   64B | redis    |           9304 |        22516 |          10064 |     135.3 |
| 100000 |  256B | oktoplus |          21172 |        69288 |          21632 |     492.7 |
| 100000 |  256B | redis    |           9300 |        45660 |          10080 |     372.3 |
| 100000 | 1024B | oktoplus |          21264 |       163808 |          22464 |    1459.7 |
| 100000 | 1024B | redis    |           9296 |       140380 |          10964 |    1342.3 |
| 1000000 |    3B | oktoplus |          21196 |       218636 |          23116 |     202.2 |
| 1000000 |    3B | redis    |           9308 |        79104 |          11536 |      71.5 |
| 1000000 |   64B | oktoplus |          21156 |       297616 |          23704 |     283.1 |
| 1000000 |   64B | redis    |           9392 |       138860 |          11844 |     132.6 |
| 1000000 |  256B | oktoplus |          21244 |       534548 |          25668 |     525.6 |
| 1000000 |  256B | redis    |           9308 |       375552 |          14156 |     375.0 |
| 1000000 | 1024B | oktoplus |          21272 |      1478524 |          33420 |    1492.2 |
| 1000000 | 1024B | redis    |           9308 |      1322592 |          23700 |    1344.8 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    170.6 |      70.5 |        2.42 |
| 100000 |   64B |    251.7 |     135.3 |        1.86 |
| 100000 |  256B |    492.7 |     372.3 |        1.32 |
| 100000 | 1024B |   1459.7 |    1342.3 |        1.09 |
| 1000000 |    3B |    202.2 |      71.5 |        2.83 |
| 1000000 |   64B |    283.1 |     132.6 |        2.13 |
| 1000000 |  256B |    525.6 |     375.0 |        1.40 |
| 1000000 | 1024B |   1492.2 |    1344.8 |        1.11 |
