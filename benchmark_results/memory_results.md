# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |           8584 |        21640 |           8964 |     133.7 |
| 100000 |    3B | redis    |           8592 |        15504 |          11428 |      70.8 |
| 100000 |   64B | oktoplus |           8684 |        27884 |           9064 |     196.6 |
| 100000 |   64B | redis    |           8580 |        21636 |           9256 |     133.7 |
| 100000 |  256B | oktoplus |           8684 |        47852 |          10668 |     401.1 |
| 100000 |  256B | redis    |           8564 |        45428 |           9648 |     377.5 |
| 100000 | 1024B | oktoplus |           8700 |       124668 |          12196 |    1187.5 |
| 100000 | 1024B | redis    |           8576 |       139904 |          10416 |    1344.8 |
| 1000000 |    3B | oktoplus |           8712 |       155372 |          10588 |     150.2 |
| 1000000 |    3B | redis    |           8568 |        78456 |          11408 |      71.6 |
| 1000000 |   64B | oktoplus |           8668 |       219084 |          11544 |     215.5 |
| 1000000 |   64B | redis    |           8588 |       140684 |          11564 |     135.3 |
| 1000000 |  256B | oktoplus |           8688 |       413372 |          18456 |     414.4 |
| 1000000 |  256B | redis    |           8632 |       375448 |          13984 |     375.6 |
| 1000000 | 1024B | oktoplus |           8688 |      1187468 |          43048 |    1207.1 |
| 1000000 | 1024B | redis    |           8568 |      1322296 |          23860 |    1345.3 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    133.7 |      70.8 |        1.89 |
| 100000 |   64B |    196.6 |     133.7 |        1.47 |
| 100000 |  256B |    401.1 |     377.5 |        1.06 |
| 100000 | 1024B |   1187.5 |    1344.8 |        0.88 |
| 1000000 |    3B |    150.2 |      71.6 |        2.10 |
| 1000000 |   64B |    215.5 |     135.3 |        1.59 |
| 1000000 |  256B |    414.4 |     375.6 |        1.10 |
| 1000000 | 1024B |   1207.1 |    1345.3 |        0.90 |
