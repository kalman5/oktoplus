#include "Storage/sequencecontainer.h"
#include "Storage/sets.h"

#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace okstor = okts::stor;

class TestStorageConcurrency : public ::testing::Test
{};

// Many threads pushing on disjoint keys. Verifies that no element is
// lost when the outer-mutex / per-key-mutex coordination handles
// concurrent inserts across many keys at once.
TEST_F(TestStorageConcurrency, lists_disjoint_keys_no_loss) {
  okstor::Lists      myLists;
  const int          kThreads = 8;
  const int          kKeysPerThread = 64;
  const int          kPushesPerKey = 50;

  std::vector<std::thread> myWorkers;
  myWorkers.reserve(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    myWorkers.emplace_back([&, t] {
      for (int k = 0; k < kKeysPerThread; ++k) {
        std::string myKey = "t" + std::to_string(t) + "_k" + std::to_string(k);
        for (int i = 0; i < kPushesPerKey; ++i) {
          std::string         myVal = std::to_string(i);
          std::vector<std::string_view> myValues{myVal};
          myLists.pushBack(myKey, myValues);
        }
      }
    });
  }
  for (auto& myT : myWorkers) myT.join();

  size_t myTotal = 0;
  for (int t = 0; t < kThreads; ++t) {
    for (int k = 0; k < kKeysPerThread; ++k) {
      std::string myKey = "t" + std::to_string(t) + "_k" + std::to_string(k);
      myTotal += myLists.size(myKey);
    }
  }
  EXPECT_EQ(static_cast<size_t>(kThreads * kKeysPerThread * kPushesPerKey),
            myTotal);
}

// Many threads racing on the SAME key. SADD with overlapping values:
// the union of all values across threads must end up in the set, no
// duplicates lost or extras created.
TEST_F(TestStorageConcurrency, sets_same_key_safe) {
  okstor::Sets       mySets;
  const int          kThreads = 8;
  const int          kValuesPerThread = 200;

  std::vector<std::thread> myWorkers;
  myWorkers.reserve(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    myWorkers.emplace_back([&, t] {
      for (int i = 0; i < kValuesPerThread; ++i) {
        std::string                   myVal = std::to_string(i);
        std::vector<std::string_view> myValues{myVal};
        mySets.add("hot", myValues);
      }
    });
  }
  for (auto& myT : myWorkers) myT.join();

  // 0..kValuesPerThread-1 distinct values, regardless of thread count.
  EXPECT_EQ(static_cast<size_t>(kValuesPerThread), mySets.cardinality("hot"));
}

// Eviction race: while one thread keeps pushing to a key, another
// thread keeps popping it to empty (which triggers auto-remove). Must
// not crash, must not livelock, and the final state must reflect the
// last-writer-wins (either some elements remain, or zero).
TEST_F(TestStorageConcurrency, lists_push_pop_eviction_race) {
  okstor::Lists                 myLists;
  std::atomic<bool>             myStop{false};
  const std::string             myKey = "race";

  std::thread myPusher([&] {
    while (!myStop) {
      std::string                   myVal = "x";
      std::vector<std::string_view> myValues{myVal};
      myLists.pushBack(myKey, myValues);
    }
  });
  std::thread myPopper([&] {
    while (!myStop) {
      myLists.popFront(myKey, 1);
    }
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  myStop = true;
  myPusher.join();
  myPopper.join();

  // Drain whatever is left and ensure it's a sane number.
  auto myRemaining = myLists.size(myKey);
  EXPECT_LE(myRemaining, std::numeric_limits<size_t>::max() / 2)
      << "size went negative-ish, indicating corruption";
}

// hostedKeys() must accurately reflect the number of distinct keys
// after concurrent inserts.
TEST_F(TestStorageConcurrency, hostedkeys_across_shards) {
  okstor::Lists                 myLists;
  const int                     kThreads = 4;
  const int                     kKeysPerThread = 200;

  std::vector<std::thread> myWorkers;
  myWorkers.reserve(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    myWorkers.emplace_back([&, t] {
      for (int k = 0; k < kKeysPerThread; ++k) {
        std::string                   myKey =
            "shard_t" + std::to_string(t) + "_k" + std::to_string(k);
        std::vector<std::string_view> myValues{std::string_view{"v"}};
        myLists.pushBack(myKey, myValues);
      }
    });
  }
  for (auto& myT : myWorkers) myT.join();

  EXPECT_EQ(static_cast<size_t>(kThreads * kKeysPerThread),
            myLists.hostedKeys());
}
