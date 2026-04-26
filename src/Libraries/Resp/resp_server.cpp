#include "Resp/resp_server.h"
#include "Resp/resp_handler.h"
#include "Resp/resp_parser.h"

#include <boost/asio/steady_timer.hpp>

#include <glog/logging.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <memory>

namespace okts::resp {

namespace {

std::pair<std::string, uint16_t> parseEndpoint(const std::string& aEndpoint) {
  auto myPos = aEndpoint.rfind(':');
  if (myPos == std::string::npos) {
    throw std::runtime_error(
        "Invalid RESP endpoint format, expected host:port");
  }
  return {aEndpoint.substr(0, myPos),
          static_cast<uint16_t>(std::stoi(aEndpoint.substr(myPos + 1)))};
}

bool isQuit(const std::vector<std::string>& aCmd) {
  if (aCmd.empty() || aCmd[0].size() != 4) {
    return false;
  }
  // ASCII case-insensitive compare against "QUIT" without allocating.
  return (aCmd[0][0] | 0x20) == 'q' && (aCmd[0][1] | 0x20) == 'u' &&
         (aCmd[0][2] | 0x20) == 'i' && (aCmd[0][3] | 0x20) == 't';
}

size_t resolveWorkerCount(size_t aRequested) {
  if (aRequested > 0) {
    return aRequested;
  }
  const unsigned myHw = std::thread::hardware_concurrency();
  // Cap at 16 by default — past that we pay scheduling cost without
  // throughput gain on RESP workloads since command execution itself
  // takes per-key locks and the bottleneck shifts to storage.
  return myHw == 0 ? 4 : std::min<unsigned>(myHw, 16);
}

} // namespace

// Per-connection state machine. Lifetime: a shared_ptr is captured by
// every in-flight async op. When the socket is closed (peer EOF, error,
// or shutdown-side cancel) callbacks fire with EOF / operation_aborted,
// drop their captured shared_ptr, and the Connection self-destructs.
//
// Concurrency: a Connection's socket is bound to one io_context driven
// by a single thread, so all callbacks for this Connection run serially
// on that one thread. No strand needed, no work migration between
// cores per command.
class Connection : public std::enable_shared_from_this<Connection>
{
 public:
  Connection(boost::asio::io_context&     aIo,
             boost::asio::ip::tcp::socket aSocket,
             stor::StorageContext&        aStorage)
      : theIo(aIo)
      , theSocket(std::move(aSocket))
      , theHandler(aStorage) {
    theBatchedReply.reserve(1024);
  }

  void start() {
    // Wire the deferred-reply sink the handler uses for blocking
    // commands. Captured by value: a copy lives in every waiter
    // closure registered through this connection. The closure must
    // post onto this io_context (the connection's worker thread) so
    // the actual socket write happens on the right thread.
    auto mySelfWeak = std::weak_ptr<Connection>(shared_from_this());
    auto& myIo = theIo;
    theHandler.setDeferredContext(
        theIo,
        [mySelfWeak, &myIo](std::string aReply) {
          if (auto mySelf = mySelfWeak.lock()) {
            boost::asio::dispatch(
                myIo,
                [mySelf, r = std::move(aReply)]() mutable {
                  mySelf->onDeferredReply(std::move(r));
                });
          }
          // else: connection already gone. The reply is discarded;
          // the storage-side waiter has already been removed.
        });

    boost::asio::dispatch(theIo,
                          [self = shared_from_this()]() { self->doRead(); });
  }

  // Force-close from outside (shutdown path). Posted onto the owning
  // io_context so it doesn't race with an in-flight callback.
  void close() {
    boost::asio::dispatch(theIo, [self = shared_from_this()]() {
      boost::system::error_code myEc;
      self->theSocket.shutdown(boost::asio::ip::tcp::socket::shutdown_both,
                               myEc);
      self->theSocket.close(myEc);
    });
  }

 private:
  void doRead() {
    auto myMutBuf = theReadBuffer.prepare(4096);
    theSocket.async_read_some(
        myMutBuf,
        [self = shared_from_this()](
            const boost::system::error_code& aEc, size_t aN) {
          self->onRead(aEc, aN);
        });
  }

  void onRead(const boost::system::error_code& aEc, size_t aN) {
    if (aEc) {
      // EOF / cancelled / network error — drop. Last shared_from_this
      // ref goes away when this lambda chain unwinds.
      return;
    }
    theReadBuffer.commit(aN);

    // While we're blocked on a deferred reply, do NOT parse/dispatch
    // any new commands — they have to wait until the blocking command
    // returns. Keep an async_read_some pending though, both so we
    // detect a client disconnect (EOF cleans up the Connection) and
    // so we keep at least one shared_ptr ref alive on this object
    // while the deferred reply machinery is in flight.
    if (theBlocked) {
      doRead();
      return;
    }

    drainAndMaybeWrite();
  }

  // Parse and dispatch as many commands as the read buffer holds.
  // Stops early if a blocking command suspends — leaves remaining
  // bytes in the buffer for the next round. Issues a write if any
  // replies accumulated. Decides whether to schedule the next read
  // based on whether we're now suspended or not.
  void drainAndMaybeWrite() {
    bool myQuit     = false;
    bool mySuspend  = false;
    for (;;) {
      auto myParse = RespParser::tryParseCommand(theReadBuffer);
      if (myParse.status == RespParser::ParseStatus::NeedMore) {
        break;
      }
      if (myParse.status == RespParser::ParseStatus::Error) {
        // Malformed framing — close the connection. Matches Redis's
        // behaviour on a protocol error: drop, no reply.
        boost::system::error_code myEc;
        theSocket.close(myEc);
        return;
      }
      std::string myReply;
      try {
        myReply = theHandler.handle(myParse.args);
      } catch (const std::exception& e) {
        LOG(ERROR) << "RESP handler threw: " << e.what();
        boost::system::error_code myEc;
        theSocket.close(myEc);
        return;
      }
      if (myReply.empty()) {
        // Sentinel: blocking command suspended. Whatever we've
        // batched up to this point can still go out; the next
        // command in the buffer (if any) waits until the deferred
        // reply fires and resumes the loop.
        mySuspend = true;
        break;
      }
      theBatchedReply.append(myReply);
      if (isQuit(myParse.args)) {
        myQuit = true;
        break;
      }
    }

    if (theBatchedReply.empty()) {
      if (mySuspend) {
        theBlocked = true;
      }
      // Either way, keep an async_read_some in flight: it's our only
      // signal for client disconnect (EOF) and the only thing keeping
      // a shared_ptr ref alive on this Connection. While blocked the
      // read callback is a no-op other than re-arming.
      doRead();
      return;
    }

    // Move into the in-flight slot so theBatchedReply is free to
    // accumulate the next batch. async_write requires the buffer to
    // stay alive until the completion handler runs; since this
    // Connection runs on one thread, no concurrent re-entry happens.
    theInFlightReply = std::move(theBatchedReply);
    theBatchedReply  = {};
    theBatchedReply.reserve(1024);

    if (mySuspend) {
      theBlocked = true;
    }

    boost::asio::async_write(
        theSocket,
        boost::asio::buffer(theInFlightReply),
        [self = shared_from_this(), myQuit](
            const boost::system::error_code& aEc, size_t) {
          self->onWrite(aEc, myQuit);
        });
  }

  void onWrite(const boost::system::error_code& aEc, bool aQuit) {
    theInFlightReply.clear();
    if (aEc || aQuit) {
      boost::system::error_code myEc;
      theSocket.close(myEc);
      return;
    }
    // Re-arm the read so we both detect client disconnect and keep a
    // shared_ptr ref alive on this Connection. While blocked the
    // read callback is a no-op until the deferred sink fires
    // onDeferredReply().
    doRead();
  }

  // Called (on this io_context's thread) when a blocking command's
  // deferred reply fires — either a producer transferred a value or
  // the BLPOP timeout expired. Writes the deferred reply, drains any
  // commands buffered behind the blocker, then resumes reading.
  void onDeferredReply(std::string aReply) {
    if (!theBlocked) {
      // Should not happen — defensive in case a stale wake fires
      // after the connection already moved on (e.g. client closed).
      return;
    }
    theBlocked = false;
    theBatchedReply.append(std::move(aReply));
    // Continue draining any pipelined commands that were buffered
    // while we were blocked. drainAndMaybeWrite() handles the rest:
    // partial-frame, write, possibly re-enter blocked state.
    drainAndMaybeWrite();
  }

  boost::asio::io_context&     theIo;
  boost::asio::ip::tcp::socket theSocket;
  boost::asio::streambuf       theReadBuffer;
  // Accumulates replies for commands parsed from the current read
  // batch. Once the read batch is drained we move it into
  // theInFlightReply and start one async_write covering the whole
  // batch. Preserves cross-segment coalescing (replies for N
  // pipelined commands go out as one TCP write with Nagle off).
  std::string theBatchedReply;
  std::string theInFlightReply;
  RespHandler theHandler;
  // True while the connection is parked on a blocking command's
  // deferred reply. While true the read loop does not run; the next
  // wake comes from onDeferredReply().
  bool        theBlocked = false;
};

RespServer::IoSlot::IoSlot()
    : io()
    , guard(boost::asio::make_work_guard(io))
    , thread() {
}

RespServer::RespServer(stor::StorageContext& aStorage,
                       const std::string&    aEndpoint,
                       size_t                aWorkerThreads)
    : theStorage(aStorage)
    , theIoSlots()
    , theNextSlot(0)
    , theAcceptor(std::nullopt)
    , theRunning(true) {

  const size_t myWorkers = resolveWorkerCount(aWorkerThreads);
  theIoSlots.reserve(myWorkers);
  for (size_t i = 0; i < myWorkers; ++i) {
    theIoSlots.emplace_back(std::make_unique<IoSlot>());
  }

  // Acceptor lives on slot[0]; bind/listen here.
  auto [myHost, myPort] = parseEndpoint(aEndpoint);
  boost::asio::ip::tcp::resolver myResolver(theIoSlots[0]->io);
  auto myResults = myResolver.resolve(myHost, std::to_string(myPort));
  auto myEndpoint = myResults.begin()->endpoint();

  theAcceptor.emplace(theIoSlots[0]->io);
  theAcceptor->open(myEndpoint.protocol());
  theAcceptor->set_option(
      boost::asio::ip::tcp::acceptor::reuse_address(true));
  theAcceptor->bind(myEndpoint);
  theAcceptor->listen();

  LOG(INFO) << "RESP server listening on " << aEndpoint
            << " (workers=" << myWorkers << ")";

  doAccept();

  for (auto& mySlot : theIoSlots) {
    mySlot->thread = std::thread([raw = mySlot.get()]() {
      try {
        raw->io.run();
      } catch (const std::exception& e) {
        LOG(ERROR) << "RESP io worker threw: " << e.what();
      } catch (...) {
        LOG(ERROR) << "RESP io worker threw: unknown";
      }
    });
  }
}

RespServer::~RespServer() {
  shutdown();
}

void RespServer::shutdown() {
  if (!theRunning.exchange(false)) {
    return;
  }

  // Stop accepting first. cancel() makes any pending async_accept fire
  // with operation_aborted; close() releases the listening fd.
  boost::system::error_code myEc;
  if (theAcceptor) {
    theAcceptor->cancel(myEc);
    theAcceptor->close(myEc);
  }

  // Force-close every live connection. close() posts a task onto the
  // connection's owning io_context which cancels in-flight read/write.
  // Those handlers then drop the last shared_from_this and the
  // Connection self-destructs.
  {
    std::lock_guard<std::mutex> myLock(theConnectionsMutex);
    for (auto& myWeak : theConnections) {
      if (auto myConn = myWeak.lock()) {
        myConn->close();
      }
    }
    theConnections.clear();
  }

  // Release each work guard and stop each io_context, then join.
  for (auto& mySlot : theIoSlots) {
    mySlot->guard.reset();
    mySlot->io.stop();
  }
  for (auto& mySlot : theIoSlots) {
    if (mySlot->thread.joinable()) {
      mySlot->thread.join();
    }
  }
  theIoSlots.clear();

  LOG(INFO) << "RESP server stopped.";
}

void RespServer::doAccept() {
  theAcceptor->async_accept(
      [this](const boost::system::error_code& aEc,
             boost::asio::ip::tcp::socket     aSocket) {
        if (aEc) {
          // Two distinct cases:
          //
          //   1. operation_aborted (shutdown cancelled the
          //      acceptor): we're tearing down; do NOT re-arm.
          //   2. Any other error -- EMFILE (per-process fd limit),
          //      ECONNABORTED (peer RST'd between SYN-ACK and
          //      accept), ENOMEM, etc. These are transient: the
          //      client connection is lost but the listener is
          //      still healthy and must keep accepting. Without
          //      re-arming, a single EMFILE silently kills the
          //      entire RESP endpoint with one log line.
          if (!theRunning ||
              aEc == boost::asio::error::operation_aborted) {
            return;
          }
          // Throttled re-arm. EMFILE in particular can persist for
          // many consecutive accepts (the kernel keeps returning it
          // until enough fds are reclaimed); without a backoff we
          // burn CPU and fill the log spinning. 100ms is fast enough
          // that real clients barely notice but slow enough that a
          // sustained EMFILE only logs ~10 lines/s instead of 100k.
          LOG_EVERY_N(ERROR, 100)
              << "RESP accept error (transient, re-arming): "
              << aEc.message() << " [logged 1/100]";
          auto myTimer = std::make_shared<boost::asio::steady_timer>(
              theIoSlots[0]->io);
          myTimer->expires_after(std::chrono::milliseconds(100));
          myTimer->async_wait(
              [this, myTimer](const boost::system::error_code& aWaitEc) {
                if (aWaitEc == boost::asio::error::operation_aborted ||
                    !theRunning) {
                  return;
                }
                doAccept();
              });
          return;
        }

        boost::system::error_code myNoDelayEc;
        aSocket.set_option(boost::asio::ip::tcp::no_delay(true), myNoDelayEc);

        LOG(INFO) << "RESP client connected";

        // Round-robin onto an io_context. Move the socket onto the
        // target io by extracting the native handle and re-assigning
        // — sockets are bound to one io_context for life and asio
        // doesn't expose a direct "rebind" API.
        const size_t myIdx =
            theNextSlot.fetch_add(1, std::memory_order_relaxed) %
            theIoSlots.size();
        auto& myTargetIo = theIoSlots[myIdx]->io;

        const auto myProtocol = aSocket.local_endpoint(myNoDelayEc).protocol();
        const auto myFd       = aSocket.release(myNoDelayEc);

        boost::asio::ip::tcp::socket myPinned(myTargetIo);
        myPinned.assign(myProtocol, myFd, myNoDelayEc);
        if (myNoDelayEc) {
          LOG(ERROR) << "RESP socket re-pin failed: " << myNoDelayEc.message();
          ::close(myFd);
        } else {
          auto myConn = std::make_shared<Connection>(
              myTargetIo, std::move(myPinned), theStorage);
          {
            std::lock_guard<std::mutex> myLock(theConnectionsMutex);
            pruneFinishedConnectionsLocked();
            theConnections.emplace_back(myConn);
          }
          myConn->start();
        }

        if (theRunning) {
          doAccept();
        }
      });
}

void RespServer::pruneFinishedConnectionsLocked() {
  // Drop weak_ptrs whose Connection has already self-destructed so the
  // list doesn't grow unboundedly under churn.
  for (auto myIt = theConnections.begin(); myIt != theConnections.end();) {
    if (myIt->expired()) {
      myIt = theConnections.erase(myIt);
    } else {
      ++myIt;
    }
  }
}

} // namespace okts::resp
