# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |           8648 |        20936 |          11640 |     125.8 |
| 100000 |    3B | redis    |           8600 |        14744 |          10028 |      62.9 |
| 100000 |   64B | oktoplus |           8652 |        27852 |          13204 |     196.6 |
| 100000 |   64B | redis    |           8588 |        21644 |           9964 |     133.7 |
| 100000 |  256B | oktoplus |           8652 |        47052 |          11920 |     393.2 |
| 100000 |  256B | redis    |           8588 |        45452 |          10500 |     377.5 |
| 100000 | 1024B | oktoplus |           8684 |       124652 |          15728 |    1187.5 |
| 100000 | 1024B | redis    |           8564 |       136820 |           7328 |    1313.3 |
| 1000000 |    3B | oktoplus |           8672 |       162272 |          13996 |     157.3 |
| 1000000 |    3B | redis    |           8584 |        79240 |          11684 |      72.4 |
| 1000000 |   64B | oktoplus |           8656 |       229840 |          15616 |     226.5 |
| 1000000 |   64B | redis    |           8584 |       138648 |          11564 |     133.2 |
| 1000000 |  256B | oktoplus |           8648 |       427976 |          21820 |     429.4 |
| 1000000 |  256B | redis    |           8548 |       374964 |          13988 |     375.2 |
| 1000000 | 1024B | oktoplus |           8712 |      1204404 |          45772 |    1224.4 |
| 1000000 | 1024B | redis    |           8584 |      1321912 |          23628 |    1344.8 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    125.8 |      62.9 |        2.00 |
| 100000 |   64B |    196.6 |     133.7 |        1.47 |
| 100000 |  256B |    393.2 |     377.5 |        1.04 |
| 100000 | 1024B |   1187.5 |    1313.3 |        0.90 |
| 1000000 |    3B |    157.3 |      72.4 |        2.17 |
| 1000000 |   64B |    226.5 |     133.2 |        1.70 |
| 1000000 |  256B |    429.4 |     375.2 |        1.14 |
| 1000000 | 1024B |   1224.4 |    1344.8 |        0.91 |
