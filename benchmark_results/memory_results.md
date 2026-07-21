# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |           8700 |        20988 |           9168 |     125.8 |
| 100000 |    3B | redis    |           8600 |        14744 |           9984 |      62.9 |
| 100000 |   64B | oktoplus |           8632 |        27832 |           9012 |     196.6 |
| 100000 |   64B | redis    |           8596 |        21652 |           9252 |     133.7 |
| 100000 |  256B | oktoplus |           8696 |        47096 |           9928 |     393.2 |
| 100000 |  256B | redis    |           8584 |        45448 |           9668 |     377.5 |
| 100000 | 1024B | oktoplus |           8704 |       124672 |          12200 |    1187.5 |
| 100000 | 1024B | redis    |           8576 |       139136 |           9648 |    1336.9 |
| 1000000 |    3B | oktoplus |           8572 |       154452 |           9408 |     149.4 |
| 1000000 |    3B | redis    |           8600 |        79256 |          11704 |      72.4 |
| 1000000 |   64B | oktoplus |           8600 |       219012 |          11480 |     215.5 |
| 1000000 |   64B | redis    |           8592 |       138640 |          12092 |     133.2 |
| 1000000 |  256B | oktoplus |           8612 |       413324 |          19364 |     414.4 |
| 1000000 |  256B | redis    |           8600 |       375412 |          14296 |     375.6 |
| 1000000 | 1024B | oktoplus |           8684 |      1187480 |          43364 |    1207.1 |
| 1000000 | 1024B | redis    |           8564 |      1322100 |          23152 |    1345.1 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    125.8 |      62.9 |        2.00 |
| 100000 |   64B |    196.6 |     133.7 |        1.47 |
| 100000 |  256B |    393.2 |     377.5 |        1.04 |
| 100000 | 1024B |   1187.5 |    1336.9 |        0.89 |
| 1000000 |    3B |    149.4 |      72.4 |        2.06 |
| 1000000 |   64B |    215.5 |     133.2 |        1.62 |
| 1000000 |  256B |    414.4 |     375.6 |        1.10 |
| 1000000 | 1024B |   1207.1 |    1345.1 |        0.90 |
