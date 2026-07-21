# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |           8652 |        20940 |           8264 |     125.8 |
| 100000 |    3B | redis    |           8584 |        15496 |           9644 |      70.8 |
| 100000 |   64B | oktoplus |           8640 |        27840 |           9104 |     196.6 |
| 100000 |   64B | redis    |           8576 |        21632 |           9240 |     133.7 |
| 100000 |  256B | oktoplus |           8704 |        47104 |           9936 |     393.2 |
| 100000 |  256B | redis    |           8600 |        44696 |           9560 |     369.6 |
| 100000 | 1024B | oktoplus |           8628 |       124596 |          12132 |    1187.5 |
| 100000 | 1024B | redis    |           8572 |       139132 |           9648 |    1336.9 |
| 1000000 |    3B | oktoplus |           8740 |       155428 |          10380 |     150.2 |
| 1000000 |    3B | redis    |           8580 |        79236 |          11180 |      72.4 |
| 1000000 |   64B | oktoplus |           8668 |       219080 |          11544 |     215.5 |
| 1000000 |   64B | redis    |           8600 |       140696 |          11588 |     135.3 |
| 1000000 |  256B | oktoplus |           8692 |       413400 |          18488 |     414.4 |
| 1000000 |  256B | redis    |           8600 |       375412 |          13944 |     375.6 |
| 1000000 | 1024B | oktoplus |           8740 |      1188388 |          43968 |    1208.0 |
| 1000000 | 1024B | redis    |           8580 |      1321568 |          24204 |    1344.5 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    125.8 |      70.8 |        1.78 |
| 100000 |   64B |    196.6 |     133.7 |        1.47 |
| 100000 |  256B |    393.2 |     369.6 |        1.06 |
| 100000 | 1024B |   1187.5 |    1336.9 |        0.89 |
| 1000000 |    3B |    150.2 |      72.4 |        2.07 |
| 1000000 |   64B |    215.5 |     135.3 |        1.59 |
| 1000000 |  256B |    414.4 |     375.6 |        1.10 |
| 1000000 | 1024B |   1208.0 |    1344.5 |        0.90 |
