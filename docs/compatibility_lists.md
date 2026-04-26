## Lists — command compatibility

| gRPC Interface       | RESP   | REDIS command  |
|----------------------|:------:|:--------------:|
| _                    | _      | BLMOVE         |
| _                    | _      | BLMPOP         |
| _                    | BLPOP  | BLPOP          |
| _                    | BRPOP  | BRPOP          |
| _                    | _      | BRPOPLPUSH     |
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
