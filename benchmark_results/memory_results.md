# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe`, record steady-state RSS, FLUSHALL + MEMORY PURGE, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |           9540 |        24768 |          13032 |     155.9 |
| 100000 |    3B | redis    |           9328 |        16240 |          10192 |      70.8 |
| 100000 |   64B | oktoplus |           9536 |        31176 |          13928 |     221.6 |
| 100000 |   64B | redis    |           9308 |        22492 |          10016 |     135.0 |
| 100000 |  256B | oktoplus |           9532 |        50420 |          13372 |     418.7 |
| 100000 |  256B | redis    |           9292 |        45656 |          10052 |     372.4 |
| 100000 | 1024B | oktoplus |           9536 |       127836 |          16820 |    1211.4 |
| 100000 | 1024B | redis    |           9336 |       140428 |          11020 |    1342.4 |
| 1000000 |    3B | oktoplus |           9528 |       214240 |          13652 |     209.6 |
| 1000000 |    3B | redis    |           9368 |        79148 |          11612 |      71.5 |
| 1000000 |   64B | oktoplus |           9532 |       281980 |          16008 |     279.0 |
| 1000000 |   64B | redis    |           9340 |       138820 |          11804 |     132.6 |
| 1000000 |  256B | oktoplus |           9528 |       479020 |          22416 |     480.8 |
| 1000000 |  256B | redis    |           9344 |       375580 |          14196 |     375.0 |
| 1000000 | 1024B | oktoplus |           9512 |      1260412 |          47304 |    1280.9 |
| 1000000 | 1024B | redis    |           9392 |      1322688 |          23800 |    1344.8 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    155.9 |      70.8 |        2.20 |
| 100000 |   64B |    221.6 |     135.0 |        1.64 |
| 100000 |  256B |    418.7 |     372.4 |        1.12 |
| 100000 | 1024B |   1211.4 |    1342.4 |        0.90 |
| 1000000 |    3B |    209.6 |      71.5 |        2.93 |
| 1000000 |   64B |    279.0 |     132.6 |        2.10 |
| 1000000 |  256B |    480.8 |     375.0 |        1.28 |
| 1000000 | 1024B |   1280.9 |    1344.8 |        0.95 |
