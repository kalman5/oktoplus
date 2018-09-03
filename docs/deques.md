

GRP Interface | Equivalent REDIS Command (for Lists)|
---|:---:
dequePushFront | LPUSH
dequePushBack | RPUSH
dequePopFront | LPOP
dequePopBack | RPOP
dequeLength | LLEN
dequeIndex | LINDEX
  _ | BLPOP
 _ | BRPOP
 _ | BRPOPLPUSH
 dequeInsert | LINSERT
 dequePushFrontExist | LPUSHX
 dequeRange | LRANGE
 dequeRemove | LREM
 dequeSet | LSET
 dequeTrim | LTRIM
 dequePopBackPushFront | RPOPLPUSH
 dequePushBackExist | RPUSHX
