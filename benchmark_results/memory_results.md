# Memory footprint — Oktoplus vs Redis

Each trial: start fresh server, record baseline RSS, load N distinct keys via `RPUSH key:i <value>` piped through `redis-cli --pipe` (deterministic, no random collisions, identical workload to both servers), record steady-state RSS, FLUSHALL, record residual RSS.

`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. `residual` is what the allocator hangs on to after FLUSHALL — lower is better, but allocators legitimately retain pages for reuse.

| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |
|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|
| 100000 |    3B | oktoplus |          21420 |        38244 |          26900 |     172.3 |
| 100000 |    3B | redis    |           9296 |        16172 |           9680 |      70.4 |
| 100000 |   64B | oktoplus |          21420 |        46092 |          27548 |     252.6 |
| 100000 |   64B | redis    |           9304 |        22512 |          10040 |     135.2 |
| 100000 |  256B | oktoplus |          21428 |        69648 |          28596 |     493.8 |
| 100000 |  256B | redis    |           9372 |        46260 |          10388 |     377.7 |
| 100000 | 1024B | oktoplus |          21400 |       164048 |          27888 |    1460.7 |
| 100000 | 1024B | redis    |           9316 |       140412 |          11000 |    1342.4 |
| 1000000 |    3B | oktoplus |          21376 |       238420 |          29852 |     222.3 |
| 1000000 |    3B | redis    |           9300 |        79092 |          11544 |      71.5 |
| 1000000 |   64B | oktoplus |          21304 |       319356 |          30312 |     305.2 |
| 1000000 |   64B | redis    |           9340 |       138816 |          11796 |     132.6 |
| 1000000 |  256B | oktoplus |          21392 |       554196 |          31604 |     545.6 |
| 1000000 |  256B | redis    |           9340 |       375596 |          14200 |     375.0 |
| 1000000 | 1024B | oktoplus |          21384 |      1500232 |          38824 |    1514.3 |
| 1000000 | 1024B | redis    |           9324 |      1322624 |          23720 |    1344.8 |

## Bytes/key ratio (Oktoplus / Redis)

| N keys | value | okto bpk | redis bpk | okto / redis |
|-------:|------:|---------:|----------:|-------------:|
| 100000 |    3B |    172.3 |      70.4 |        2.45 |
| 100000 |   64B |    252.6 |     135.2 |        1.87 |
| 100000 |  256B |    493.8 |     377.7 |        1.31 |
| 100000 | 1024B |   1460.7 |    1342.4 |        1.09 |
| 1000000 |    3B |    222.3 |      71.5 |        3.11 |
| 1000000 |   64B |    305.2 |     132.6 |        2.30 |
| 1000000 |  256B |    545.6 |     375.0 |        1.45 |
| 1000000 | 1024B |   1514.3 |    1344.8 |        1.13 |
