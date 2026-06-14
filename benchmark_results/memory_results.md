# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |           8644 |        20932 |          11512 |     125.8 |
| 100000 |    3B | redis    |           8708 |        15620 |          10024 |      70.8 |
| 100000 |   64B | oktoplus |           8656 |        27804 |          13472 |     196.1 |
| 100000 |   64B | redis    |           8720 |        21776 |           9700 |     133.7 |
| 100000 |  256B | oktoplus |           8644 |        47796 |          14188 |     400.9 |
| 100000 |  256B | redis    |           7896 |        44760 |           8972 |     377.5 |
| 100000 | 1024B | oktoplus |           8720 |       126992 |          17980 |    1211.1 |
| 100000 | 1024B | redis    |           8704 |       140032 |          10540 |    1344.8 |
| 1000000 |    3B | oktoplus |           8648 |       161480 |          13132 |     156.5 |
| 1000000 |    3B | redis    |           8672 |        79328 |          11280 |      72.4 |
| 1000000 |   64B | oktoplus |           8616 |       218032 |          15212 |     214.4 |
| 1000000 |   64B | redis    |           8672 |       140000 |          11636 |     134.5 |
| 1000000 |  256B | oktoplus |           8648 |       427636 |          21636 |     429.0 |
| 1000000 |  256B | redis    |           8664 |       374720 |          13256 |     374.8 |
| 1000000 | 1024B | oktoplus |           8668 |      1205024 |          46740 |    1225.1 |
| 1000000 | 1024B | redis    |           7860 |      1320872 |          22216 |    1344.5 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    125.8 |      70.8 |        1.78 |
| 100000 |   64B |    196.1 |     133.7 |        1.47 |
| 100000 |  256B |    400.9 |     377.5 |        1.06 |
| 100000 | 1024B |   1211.1 |    1344.8 |        0.90 |
| 1000000 |    3B |    156.5 |      72.4 |        2.16 |
| 1000000 |   64B |    214.4 |     134.5 |        1.59 |
| 1000000 |  256B |    429.0 |     374.8 |        1.14 |
| 1000000 | 1024B |   1225.1 |    1344.5 |        0.91 |
