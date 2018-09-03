

GRP Interface | Equivalent REDIS Command (for Lists) |
---|:---:
 vectorPushBack | RPUSH
 vectorPopBack | RPOP
 vectorLength | LLEN
 vectorIndex | LINDEX
  _ | BLPOP
  _ | BRPOP
  _ | BRPOPLPUSH
 vectorInsert | LINSERT
 vectorRange | LRANGE
 vectorRemove | LREM
 vectorSet | LSET
 vectorTrim | LTRIM
 vectorPushBackExist | RPUSHX
