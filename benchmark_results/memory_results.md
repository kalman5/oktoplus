# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe`, record steady-state RSS, FLUSHALL + MEMORY PURGE, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |           9540 |        22540 |          12964 |     133.1 |
| 100000 |    3B | redis    |           9304 |        16216 |          10236 |      70.8 |
| 100000 |   64B | oktoplus |           9500 |        28928 |          13740 |     198.9 |
| 100000 |   64B | redis    |           9304 |        22500 |          10064 |     135.1 |
| 100000 |  256B | oktoplus |           9536 |        48296 |          13324 |     396.9 |
| 100000 |  256B | redis    |           9308 |        45808 |          10088 |     373.8 |
| 100000 | 1024B | oktoplus |           9516 |       125756 |          16708 |    1190.3 |
| 100000 | 1024B | redis    |           9308 |       140396 |          10988 |    1342.3 |
| 1000000 |    3B | oktoplus |           9512 |       163008 |          13620 |     157.2 |
| 1000000 |    3B | redis    |           9304 |        79092 |          11532 |      71.5 |
| 1000000 |   64B | oktoplus |           9512 |       231468 |          15884 |     227.3 |
| 1000000 |   64B | redis    |           9324 |       138792 |          11884 |     132.6 |
| 1000000 |  256B | oktoplus |           9532 |       428400 |          22352 |     428.9 |
| 1000000 |  256B | redis    |           9304 |       375540 |          14160 |     375.0 |
| 1000000 | 1024B | oktoplus |           9512 |      1204980 |          47216 |    1224.2 |
| 1000000 | 1024B | redis    |           9324 |      1322628 |          23732 |    1344.8 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    133.1 |      70.8 |        1.88 |
| 100000 |   64B |    198.9 |     135.1 |        1.47 |
| 100000 |  256B |    396.9 |     373.8 |        1.06 |
| 100000 | 1024B |   1190.3 |    1342.3 |        0.89 |
| 1000000 |    3B |    157.2 |      71.5 |        2.20 |
| 1000000 |   64B |    227.3 |     132.6 |        1.71 |
| 1000000 |  256B |    428.9 |     375.0 |        1.14 |
| 1000000 | 1024B |   1224.2 |    1344.8 |        0.91 |
