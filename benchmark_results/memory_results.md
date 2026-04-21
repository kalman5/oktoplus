# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |          21292 |        37960 |          21464 |     170.7 |
| 100000 |    3B | redis    |           9308 |        16212 |          10268 |      70.7 |
| 100000 |   64B | oktoplus |          21300 |        45864 |          21536 |     251.5 |
| 100000 |   64B | redis    |           9304 |        22516 |          10076 |     135.3 |
| 100000 |  256B | oktoplus |          21272 |        69348 |          21700 |     492.3 |
| 100000 |  256B | redis    |           9296 |        46160 |          10268 |     377.5 |
| 100000 | 1024B | oktoplus |          21288 |       163824 |          22500 |    1459.6 |
| 100000 | 1024B | redis    |           9292 |       140380 |          10964 |    1342.3 |
| 1000000 |    3B | oktoplus |          21244 |       218684 |          23136 |     202.2 |
| 1000000 |    3B | redis    |           9332 |        79112 |          11548 |      71.5 |
| 1000000 |   64B | oktoplus |          21308 |       297724 |          23832 |     283.0 |
| 1000000 |   64B | redis    |           9304 |       138776 |          11756 |     132.6 |
| 1000000 |  256B | oktoplus |          21296 |       534604 |          25728 |     525.6 |
| 1000000 |  256B | redis    |           9312 |       375552 |          14152 |     375.0 |
| 1000000 | 1024B | oktoplus |          21260 |      1478536 |          33420 |    1492.3 |
| 1000000 | 1024B | redis    |           9332 |      1322624 |          23736 |    1344.8 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    170.7 |      70.7 |        2.41 |
| 100000 |   64B |    251.5 |     135.3 |        1.86 |
| 100000 |  256B |    492.3 |     377.5 |        1.30 |
| 100000 | 1024B |   1459.6 |    1342.3 |        1.09 |
| 1000000 |    3B |    202.2 |      71.5 |        2.83 |
| 1000000 |   64B |    283.0 |     132.6 |        2.13 |
| 1000000 |  256B |    525.6 |     375.0 |        1.40 |
| 1000000 | 1024B |   1492.3 |    1344.8 |        1.11 |
