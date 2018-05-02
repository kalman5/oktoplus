# oktoplus

![alt text](docs/octopus-free.png "Oktoplus")

###### What is oktoplus
Oktplus is a in-memory data store K:V where V is a container: std::list, std::map, boost::multi_index_container, std::set, you name it. Doing so the client can choose the best container for his own access data pattern.
You can use it as a cache, datastore, etc. A this very first version the only container implemented is std::list and the supported operations are:

- ListPushFront
- ListPushBack
- ListPopFront
- ListPopBack
- ListLength

the server exports a Grpc interface (https://grpc.io/). In src/Libraries/Commands/commands.proto you can find the definition of the exported interface and you can use it to build a client on your favourite language. 

Server is multithread, at the moment two different clients working on different containers (type or name) have a minimal interaction. 

If this reminds you of REDIS then you are right, I was inspired by it, however:

 - Redis is not multithread
 - Redis offers only basic containers
  - For instance the Redis command LINDEX is O(n), so if you need to access a value with an index would be better to use a Vector style container
  - There is no analogue of multi-set in Redis

#### Road Map
- Support all REDIS commands (at least the one relative to data storage)
- Support the following containers: vector, map, multimap, set, multiset, unorderd_map, unordered_multimap, boost::multi_index (up to at least 3 keys)
- Make it distributed using RAFT as consensus protocol

***

[How To Build](docs/howtobuild.md)

*** 
