## Lists — command compatibility

| gRPC Interface       | RESP   | REDIS command  |
|----------------------|:------:|:--------------:|
| _                    | BLMOVE | BLMOVE         |
| _                    | BLMPOP | BLMPOP         |
| _                    | BLPOP  | BLPOP          |
| _                    | BRPOP  | BRPOP          |
| _                    | BRPOPLPUSH | BRPOPLPUSH (deprecated since 6.2; alias for BLMOVE src dst RIGHT LEFT) |
| listIndex            | LINDEX | LINDEX         |
| listInsert           | LINSERT | LINSERT       |
| listLength           | LLEN   | LLEN           |
| listMove             | LMOVE  | LMOVE          |
| listMultiplePop      | LMPOP  | LMPOP          |
| listPopFront         | LPOP   | LPOP           |
| listPosition         | LPOS   | LPOS           |
| listPushFront        | LPUSH  | LPUSH          |
| listExistPushFront   | LPUSHX | LPUSHX         |
| listRange            | LRANGE | LRANGE         |
| listRemove           | LREM   | LREM           |
| listSet              | LSET   | LSET           |
| listTrim             | LTRIM  | LTRIM          |
| listPopBack          | RPOP   | RPOP           |
| listPushBack         | RPUSH  | RPUSH          |
| listExistPushBack    | RPUSHX | RPUSHX         |

`_` indicates "not implemented".
