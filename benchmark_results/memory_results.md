# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |           9504 |        26328 |          14984 |     172.3 |
| 100000 |    3B | redis    |           9288 |        16168 |           9676 |      70.5 |
| 100000 |   64B | oktoplus |           9540 |        34232 |          13808 |     252.8 |
| 100000 |   64B | redis    |           9304 |        22504 |          10040 |     135.2 |
| 100000 |  256B | oktoplus |           9564 |        57788 |          18000 |     493.8 |
| 100000 |  256B | redis    |           9316 |        46212 |          10332 |     377.8 |
| 100000 | 1024B | oktoplus |           9480 |       152124 |          16004 |    1460.7 |
| 100000 | 1024B | redis    |           9332 |       140420 |          11008 |    1342.3 |
| 1000000 |    3B | oktoplus |           9552 |       207292 |          18084 |     202.5 |
| 1000000 |    3B | redis    |           9344 |        79132 |          11568 |      71.5 |
| 1000000 |   64B | oktoplus |           9524 |       307580 |          15960 |     305.2 |
| 1000000 |   64B | redis    |           9296 |       138772 |          11764 |     132.6 |
| 1000000 |  256B | oktoplus |           9508 |       544964 |          21344 |     548.3 |
| 1000000 |  256B | redis    |           9292 |       375536 |          14140 |     375.0 |
| 1000000 | 1024B | oktoplus |           9528 |      1491020 |          28100 |    1517.0 |
| 1000000 | 1024B | redis    |           9316 |      1322612 |          23720 |    1344.8 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    172.3 |      70.5 |        2.44 |
| 100000 |   64B |    252.8 |     135.2 |        1.87 |
| 100000 |  256B |    493.8 |     377.8 |        1.31 |
| 100000 | 1024B |   1460.7 |    1342.3 |        1.09 |
| 1000000 |    3B |    202.5 |      71.5 |        2.83 |
| 1000000 |   64B |    305.2 |     132.6 |        2.30 |
| 1000000 |  256B |    548.3 |     375.0 |        1.46 |
| 1000000 | 1024B |   1517.0 |    1344.8 |        1.13 |
