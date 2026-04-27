#pragma once

#include "Storage/storage_context.h"
#include "Resp/resp_parser.h"

#include <boost/asio/io_context.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace okts::resp {

class RespHandler
{
 public:
  explicit RespHandler(stor::StorageContext& aStorage);

  // Sink the connection installs once at construction so blocking
  // commands can deliver a reply asynchronously after the handle()
  // call has already returned. The sink is responsible for posting
  // the reply onto the connection's owning io_context (so the
  // actual socket write happens on the right thread).
  //
  // The second argument is a recovery callable invoked iff the
  // connection / socket is gone by the time the dispatched lambda
  // runs (or already gone when the sink itself is called). It owns
  // putting any value the wake transferred out of storage back into
  // the source list, so a client disconnect mid-BLPOP does not
  // silently lose the pushed data. May be nullptr (e.g. for nil
  // BLPOP timeout replies, where there is nothing to recover).
  using DeferredOnDead    = std::function<void()>;
  using DeferredReplySink = std::function<void(std::string, DeferredOnDead)>;

  // Wire the connection-side context the handler needs for blocking
  // commands. `aIo` is used to construct steady_timers for BLPOP
  // timeouts. `aSink` is invoked when a blocking command finally has
  // a reply ready.
  void setDeferredContext(boost::asio::io_context& aIo,
                          DeferredReplySink        aSink);

  // Returns the immediate RESP reply for a non-blocking command, or
  // an empty string when the command suspended (BLPOP / BRPOP / …).
  // For suspended commands the reply will be delivered later via the
  // DeferredReplySink installed by setDeferredContext(); the empty
  // string acts as a sentinel because legitimate RESP replies are
  // never empty (every protocol-level reply is at least 4 bytes).
  std::string handle(const std::vector<std::string>& aArgs);

  using Args        = std::vector<std::string>;

  // Per-command handler implementations below are public so the
  // file-scope dispatch table in resp_handler.cpp can call them
  // without a friend dance. They are NOT intended as a stable client
  // API -- handle() is the entry point. Direct calls bypass dispatch
  // bookkeeping (case folding, blocking-command intercept).
 private:
  using HandlerFn   = std::string (*)(RespHandler&, const Args&);

 public:
  // Helpers
  static std::string validateMinArgs(const Args& aArgs,
                                     size_t aMin,
                                     std::string_view aCommand);
  static std::vector<std::string_view> extractValues(const Args& aArgs,
                                                     size_t aFrom);
  // Format `aValues` as a RESP bulk-string array directly into one
  // pre-reserved std::string. Avoids the intermediate
  // std::vector<std::string> + N small std::string allocations the old
  // implementation did per multi-element reply.
  template <typename Container>
  static std::string formatBulkStringArray(const Container& aValues) {
    size_t myReserved = 16; // "*N\r\n" plus margin
    for (const auto& myVal : aValues) {
      myReserved += 8 + myVal.size(); // "$LL\r\nDATA\r\n" overhead ≈ 8
    }
    std::string myResult;
    myResult.reserve(myReserved);
    RespParser::appendArrayHeader(myResult, aValues.size());
    for (const auto& myVal : aValues) {
      RespParser::appendBulkString(myResult, myVal);
    }
    return myResult;
  }

  // Generic push: parameterized by storage method
  using PushMethod = size_t (stor::Lists::*)(
      const std::string&, const std::vector<std::string_view>&);
  std::string handlePush(const Args& aArgs,
                         std::string_view aCommand,
                         PushMethod aMethod);

  // Generic pop with optional count arg (single vs multi return).
  // aAllowNegativeCount: SRANDMEMBER accepts a negative count to mean
  // "selection with replacement"; LPOP/RPOP/SPOP must reject negatives.
  template <typename PopFunc>
  std::string handlePopWithOptionalCount(const Args& aArgs,
                                         std::string_view aCommand,
                                         bool             aAllowNegativeCount,
                                         PopFunc&&        aPopFunc);

  // Generic multi-key set operation -> bulk string array
  template <typename SetOpFunc>
  std::string handleSetOp(const Args& aArgs,
                          std::string_view aCommand,
                          size_t aKeysFrom,
                          SetOpFunc&& aFunc);

  // General
  std::string handlePing(const Args& aArgs);
  std::string handleQuit(const Args& aArgs);
  std::string handleCommand(const Args& aArgs);
  std::string handleClient(const Args& aArgs);
  std::string handleSelect(const Args& aArgs);
  std::string handleInfo(const Args& aArgs);
  std::string handleFlush(const Args& aArgs);
  std::string handleDbsize(const Args& aArgs);
  std::string handleMemory(const Args& aArgs);

  // List commands
  std::string handleLlen(const Args& aArgs);
  std::string handleLindex(const Args& aArgs);
  std::string handleLinsert(const Args& aArgs);
  std::string handleLrange(const Args& aArgs);
  std::string handleLrem(const Args& aArgs);
  std::string handleLset(const Args& aArgs);
  std::string handleLtrim(const Args& aArgs);
  std::string handleLmove(const Args& aArgs);
  std::string handleLpos(const Args& aArgs);
  std::string handleLmpop(const Args& aArgs);

  // Blocking list commands. Each returns std::nullopt if it
  // suspended; the deferred sink will deliver the reply when a
  // producer wakes the waiter or the timeout expires.
  std::optional<std::string> handleBlpop(const Args& aArgs);
  std::optional<std::string> handleBrpop(const Args& aArgs);
  std::optional<std::string> handleBlmove(const Args& aArgs);
  std::optional<std::string> handleBlmpop(const Args& aArgs);
  std::optional<std::string> handleBrpoplpush(const Args& aArgs);

  // Helpers for blocking commands
  std::optional<std::string> handleBlockingPop(const Args&     aArgs,
                                               std::string_view aCommand,
                                               bool             aFront);
  static std::string formatBlpopReply(const std::string& aKey,
                                      const std::string& aValue);
  static std::string formatBlpopNilReply();
  static std::string formatLmpopReply(const std::string&              aKey,
                                      const std::vector<std::string>& aValues);

  // Set commands
  std::string handleSadd(const Args& aArgs);
  std::string handleScard(const Args& aArgs);
  std::string handleSdiffstore(const Args& aArgs);
  std::string handleSintercard(const Args& aArgs);
  std::string handleSinterstore(const Args& aArgs);
  std::string handleSismember(const Args& aArgs);
  std::string handleSmismember(const Args& aArgs);
  std::string handleSmove(const Args& aArgs);
  std::string handleSrem(const Args& aArgs);
  std::string handleSunionstore(const Args& aArgs);

  // Streaming SMEMBERS: avoids the intermediate flat_hash_set copy
  // and the double iteration in the generic handleSetOp path. Reply
  // string is reserved exactly once based on the cardinality
  // observed under the same per-key lock the iteration uses.
  std::string handleSmembers(const Args& aArgs);

  // Storage accessors: dispatch-table lambdas live at file scope
  // (anonymous namespace) so they reach the storage through these
  // public references rather than poking at theStorage directly.
  stor::Lists& lists() { return theStorage.lists; }
  stor::Sets&  sets()  { return theStorage.sets;  }

 private:
  stor::StorageContext&     theStorage;
  // Blocking-command context. Owned by the Connection (lifetime tied
  // to the connection's io_context); set once at construction. nullptr
  // / null sink means blocking commands are not available (e.g. unit
  // tests that drive the handler synchronously) — the handler returns
  // an error reply in that case.
  boost::asio::io_context*  theIo   = nullptr;
  DeferredReplySink         theSink;
};

} // namespace okts::resp
