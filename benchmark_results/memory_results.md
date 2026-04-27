# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |          17620 |        34440 |          22996 |     172.2 |
| 100000 |    3B | redis    |           9336 |        16220 |           9724 |      70.5 |
| 100000 |   64B | oktoplus |          17620 |        42300 |          21824 |     252.7 |
| 100000 |   64B | redis    |           9348 |        22556 |          10092 |     135.2 |
| 100000 |  256B | oktoplus |          17536 |        65824 |          25800 |     494.5 |
| 100000 |  256B | redis    |           9348 |        45708 |          10052 |     372.3 |
| 100000 | 1024B | oktoplus |          17548 |       160260 |          23900 |    1461.4 |
| 100000 | 1024B | redis    |           9328 |       140424 |          11004 |    1342.4 |
| 1000000 |    3B | oktoplus |          17636 |       236084 |          26212 |     223.7 |
| 1000000 |    3B | redis    |           9296 |        79080 |          11536 |      71.5 |
| 1000000 |   64B | oktoplus |          17624 |       315688 |          23968 |     305.2 |
| 1000000 |   64B | redis    |           9288 |       138756 |          11744 |     132.6 |
| 1000000 |  256B | oktoplus |          17596 |       551712 |          29360 |     546.9 |
| 1000000 |  256B | redis    |           9316 |       375568 |          14176 |     375.0 |
| 1000000 | 1024B | oktoplus |          17620 |      1497860 |          36316 |    1515.8 |
| 1000000 | 1024B | redis    |           9284 |      1322572 |          23680 |    1344.8 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    172.2 |      70.5 |        2.44 |
| 100000 |   64B |    252.7 |     135.2 |        1.87 |
| 100000 |  256B |    494.5 |     372.3 |        1.33 |
| 100000 | 1024B |   1461.4 |    1342.4 |        1.09 |
| 1000000 |    3B |    223.7 |      71.5 |        3.13 |
| 1000000 |   64B |    305.2 |     132.6 |        2.30 |
| 1000000 |  256B |    546.9 |     375.0 |        1.46 |
| 1000000 | 1024B |   1515.8 |    1344.8 |        1.13 |
