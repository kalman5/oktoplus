## Sets — command compatibility

| gRPC Interface | RESP        | REDIS command |
|----------------|:-----------:|:-------------:|
| setAdd         | SADD        | SADD          |
| setCard        | SCARD       | SCARD         |
| setDiff        | SDIFF       | SDIFF         |
| _              | SDIFFSTORE  | SDIFFSTORE    |
| _              | SINTER      | SINTER        |
| _              | SINTERCARD  | SINTERCARD    |
| _              | SINTERSTORE | SINTERSTORE   |
| _              | SISMEMBER   | SISMEMBER     |
| _              | SMISMEMBER  | SMISMEMBER    |
| _              | SMEMBERS    | SMEMBERS      |
| _              | SMOVE       | SMOVE         |
| _              | SPOP        | SPOP          |
| _              | SRANDMEMBER | SRANDMEMBER   |
| _              | SREM        | SREM          |
| _              | SUNION      | SUNION        |
| _              | SUNIONSTORE | SUNIONSTORE   |
| _              | _           | SSCAN         |

`_` indicates "not implemented".
