# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |           8644 |        20932 |           8256 |     125.8 |
| 100000 |    3B | redis    |           8580 |        15492 |           9848 |      70.8 |
| 100000 |   64B | oktoplus |           8656 |        27856 |           9036 |     196.6 |
| 100000 |   64B | redis    |           8600 |        21656 |           9324 |     133.7 |
| 100000 |  256B | oktoplus |           8700 |        47100 |           9932 |     393.2 |
| 100000 |  256B | redis    |           8600 |        45464 |           9668 |     377.5 |
| 100000 | 1024B | oktoplus |           8560 |       124528 |          12056 |    1187.5 |
| 100000 | 1024B | redis    |           8568 |       139896 |          10408 |    1344.8 |
| 1000000 |    3B | oktoplus |           8596 |       154464 |           9428 |     149.4 |
| 1000000 |    3B | redis    |           8588 |        78476 |          11516 |      71.6 |
| 1000000 |   64B | oktoplus |           8692 |       219100 |          11560 |     215.5 |
| 1000000 |   64B | redis    |           8600 |       139928 |          11576 |     134.5 |
| 1000000 |  256B | oktoplus |           8696 |       412628 |          17712 |     413.6 |
| 1000000 |  256B | redis    |           8600 |       375412 |          13748 |     375.6 |
| 1000000 | 1024B | oktoplus |           8696 |      1188272 |          43848 |    1207.9 |
| 1000000 | 1024B | redis    |           8600 |      1321592 |          22748 |    1344.5 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    125.8 |      70.8 |        1.78 |
| 100000 |   64B |    196.6 |     133.7 |        1.47 |
| 100000 |  256B |    393.2 |     377.5 |        1.04 |
| 100000 | 1024B |   1187.5 |    1344.8 |        0.88 |
| 1000000 |    3B |    149.4 |      71.6 |        2.09 |
| 1000000 |   64B |    215.5 |     134.5 |        1.60 |
| 1000000 |  256B |    413.6 |     375.6 |        1.10 |
| 1000000 | 1024B |   1207.9 |    1344.5 |        0.90 |
