#include "Storage/sets.h"

#include "gtest/gtest.h"

#include <glog/logging.h>

#include <limits>

namespace oktss = okts::stor;

class TestSets : public ::testing::Test
{
 public:
};

TEST_F(TestSets, ctor) {
  oktss::Sets mySets;
}

TEST_F(TestSets, add_cardinality) {
  oktss::Sets mySets;
  ASSERT_EQ(0u, mySets.cardinality("not existing set"));

  mySets.add("s1", {});
  ASSERT_EQ(0u, mySets.cardinality("s1"));

  mySets.add("s1", {"1", "2", "3"});
  ASSERT_EQ(3u, mySets.cardinality("s1"));
}

TEST_F(TestSets, diff) {
  oktss::Sets mySets;

  mySets.add("s1", {"1", "2", "3", "4", "5", "6", "7"});
  mySets.add("s2", {"1", "3"});
  mySets.add("s3", {"2", "4", "7"});

  auto myResult = mySets.diff({"s1", "s2", "s3"});

  ASSERT_EQ(2u, myResult.size());
  ASSERT_NE(myResult.end(), myResult.find("5"));
  ASSERT_NE(myResult.end(), myResult.find("6"));
}

TEST_F(TestSets, diffStore) {
  oktss::Sets mySets;

  mySets.add("s1", {"1", "2", "3", "4"});
  mySets.add("s2", {"1", "3"});

  auto mySize = mySets.diffStore("dest", {"s1", "s2"});
  ASSERT_EQ(2u, mySize);
  ASSERT_EQ(2u, mySets.cardinality("dest"));
  ASSERT_TRUE(mySets.isMember("dest", "2"));
  ASSERT_TRUE(mySets.isMember("dest", "4"));
}

TEST_F(TestSets, isMember) {
  oktss::Sets mySets;

  mySets.add("s1", {"a", "b", "c"});
  ASSERT_TRUE(mySets.isMember("s1", "a"));
  ASSERT_TRUE(mySets.isMember("s1", "b"));
  ASSERT_FALSE(mySets.isMember("s1", "z"));
  ASSERT_FALSE(mySets.isMember("nonexistent", "a"));
}

TEST_F(TestSets, misMember) {
  oktss::Sets mySets;

  mySets.add("s1", {"a", "b", "c"});
  auto myResult = mySets.misMember("s1", {"a", "z", "c", "x"});
  ASSERT_EQ(4u, myResult.size());
  ASSERT_TRUE(myResult[0]);
  ASSERT_FALSE(myResult[1]);
  ASSERT_TRUE(myResult[2]);
  ASSERT_FALSE(myResult[3]);
}

TEST_F(TestSets, members) {
  oktss::Sets mySets;

  auto myEmpty = mySets.members("nonexistent");
  ASSERT_TRUE(myEmpty.empty());

  mySets.add("s1", {"x", "y", "z"});
  auto myResult = mySets.members("s1");
  ASSERT_EQ(3u, myResult.size());
  ASSERT_NE(myResult.end(), myResult.find("x"));
  ASSERT_NE(myResult.end(), myResult.find("y"));
  ASSERT_NE(myResult.end(), myResult.find("z"));
}

TEST_F(TestSets, remove) {
  oktss::Sets mySets;

  mySets.add("s1", {"a", "b", "c", "d"});
  auto myRemoved = mySets.remove("s1", {"b", "d", "nonexistent"});
  ASSERT_EQ(2u, myRemoved);
  ASSERT_EQ(2u, mySets.cardinality("s1"));
  ASSERT_TRUE(mySets.isMember("s1", "a"));
  ASSERT_TRUE(mySets.isMember("s1", "c"));
}

TEST_F(TestSets, inter) {
  oktss::Sets mySets;

  mySets.add("s1", {"a", "b", "c", "d"});
  mySets.add("s2", {"b", "c", "e"});
  mySets.add("s3", {"c", "d", "e"});

  auto myResult = mySets.inter({"s1", "s2", "s3"});
  ASSERT_EQ(1u, myResult.size());
  ASSERT_NE(myResult.end(), myResult.find("c"));
}

TEST_F(TestSets, interStore) {
  oktss::Sets mySets;

  mySets.add("s1", {"a", "b", "c"});
  mySets.add("s2", {"b", "c", "d"});

  auto mySize = mySets.interStore("dest", {"s1", "s2"});
  ASSERT_EQ(2u, mySize);
  ASSERT_TRUE(mySets.isMember("dest", "b"));
  ASSERT_TRUE(mySets.isMember("dest", "c"));
}

TEST_F(TestSets, unionSets) {
  oktss::Sets mySets;

  mySets.add("s1", {"a", "b"});
  mySets.add("s2", {"b", "c"});
  mySets.add("s3", {"c", "d"});

  auto myResult = mySets.unionSets({"s1", "s2", "s3"});
  ASSERT_EQ(4u, myResult.size());
  ASSERT_NE(myResult.end(), myResult.find("a"));
  ASSERT_NE(myResult.end(), myResult.find("b"));
  ASSERT_NE(myResult.end(), myResult.find("c"));
  ASSERT_NE(myResult.end(), myResult.find("d"));
}

TEST_F(TestSets, unionStore) {
  oktss::Sets mySets;

  mySets.add("s1", {"a", "b"});
  mySets.add("s2", {"c"});

  auto mySize = mySets.unionStore("dest", {"s1", "s2"});
  ASSERT_EQ(3u, mySize);
  ASSERT_EQ(3u, mySets.cardinality("dest"));
}

TEST_F(TestSets, moveMember) {
  oktss::Sets mySets;

  mySets.add("src", {"a", "b", "c"});
  mySets.add("dst", {"x"});

  ASSERT_TRUE(mySets.moveMember("src", "dst", "b"));
  ASSERT_EQ(2u, mySets.cardinality("src"));
  ASSERT_FALSE(mySets.isMember("src", "b"));
  ASSERT_EQ(2u, mySets.cardinality("dst"));
  ASSERT_TRUE(mySets.isMember("dst", "b"));

  ASSERT_FALSE(mySets.moveMember("src", "dst", "nonexistent"));
}

TEST_F(TestSets, pop) {
  oktss::Sets mySets;

  mySets.add("s1", {"a", "b", "c", "d", "e"});

  auto myPopped = mySets.pop("s1", 2);
  ASSERT_EQ(2u, myPopped.size());
  ASSERT_EQ(3u, mySets.cardinality("s1"));

  for (const auto& myVal : myPopped) {
    ASSERT_FALSE(mySets.isMember("s1", myVal));
  }
}

TEST_F(TestSets, randMember_positive) {
  oktss::Sets mySets;

  mySets.add("s1", {"a", "b", "c", "d", "e"});

  auto myResult = mySets.randMember("s1", 3);
  ASSERT_EQ(3u, myResult.size());
  ASSERT_EQ(5u, mySets.cardinality("s1"));

  for (const auto& myVal : myResult) {
    ASSERT_TRUE(mySets.isMember("s1", myVal));
  }
}

TEST_F(TestSets, randMember_negative) {
  oktss::Sets mySets;

  mySets.add("s1", {"a", "b"});

  auto myResult = mySets.randMember("s1", -5);
  ASSERT_EQ(5u, myResult.size());
  ASSERT_EQ(2u, mySets.cardinality("s1"));
}

TEST_F(TestSets, add_returns_newly_inserted_count) {
  oktss::Sets mySets;

  EXPECT_EQ(3u, mySets.add("s1", {"a", "b", "c"}));
  EXPECT_EQ(0u, mySets.add("s1", {"a", "b", "c"}));
  EXPECT_EQ(2u, mySets.add("s1", {"a", "d", "e"}));
  EXPECT_EQ(0u, mySets.add("s2", {}));
}

TEST_F(TestSets, diffStore_overwrites_destination_when_empty) {
  oktss::Sets mySets;

  mySets.add("s1", {"a", "b"});
  mySets.add("s2", {"a", "b"});
  mySets.add("dest", {"stale"});

  auto mySize = mySets.diffStore("dest", {"s1", "s2"});
  EXPECT_EQ(0u, mySize);
  EXPECT_EQ(0u, mySets.cardinality("dest"));
  EXPECT_FALSE(mySets.isMember("dest", "stale"));
}

TEST_F(TestSets, interStore_overwrites_destination_when_empty) {
  oktss::Sets mySets;

  mySets.add("s1", {"a"});
  mySets.add("s2", {"b"});
  mySets.add("dest", {"stale"});

  auto mySize = mySets.interStore("dest", {"s1", "s2"});
  EXPECT_EQ(0u, mySize);
  EXPECT_EQ(0u, mySets.cardinality("dest"));
  EXPECT_FALSE(mySets.isMember("dest", "stale"));
}

TEST_F(TestSets, unionStore_overwrites_destination_when_empty) {
  oktss::Sets mySets;

  mySets.add("dest", {"stale"});

  auto mySize = mySets.unionStore("dest", {"missing-a", "missing-b"});
  EXPECT_EQ(0u, mySize);
  EXPECT_EQ(0u, mySets.cardinality("dest"));
  EXPECT_FALSE(mySets.isMember("dest", "stale"));
}

// Regression: SINTER with a missing key must collapse to empty,
// matching Redis semantics ("missing key == empty set"). Pre-fix,
// performOnExisting was a no-op for the missing key and the filter
// step was skipped, leaving the prior intersection un-narrowed.
TEST_F(TestSets, inter_missing_later_key_is_empty) {
  oktss::Sets mySets;
  mySets.add("s1", {"a", "b", "c"});
  mySets.add("s2", {"a", "b"});
  // "s_missing" does not exist anywhere in the set namespace.

  // Sanity: real intersection works.
  EXPECT_EQ(2u, mySets.inter({"s1", "s2"}).size());

  // The bug case: any missing later input must collapse the result.
  EXPECT_TRUE(mySets.inter({"s1", "s_missing"}).empty());
  EXPECT_TRUE(mySets.inter({"s1", "s2", "s_missing"}).empty());
  EXPECT_TRUE(mySets.inter({"s1", "s_missing", "s2"}).empty());

  // Missing first key: was already correct (myRet stays empty),
  // but assert anyway so the contract is documented.
  EXPECT_TRUE(mySets.inter({"s_missing", "s1"}).empty());
  EXPECT_TRUE(mySets.inter({"s_missing"}).empty());
}

// Regression: INT64_MIN signed-negation UB in randMember (fixed in
// aad49f3). The "with replacement" branch did `static_cast<size_t>
// (-aCount)` which overflows int64_t when a wire client passes
// `-9223372036854775808`. The fix routes through detail::safeAbs.
//
// We can't actually allocate ~9 quintillion picks, so the test
// verifies (a) the call returns rather than crashing the process,
// and (b) it either yields a (truncated) result or throws a
// recoverable std::length_error -- both are well-defined whereas
// the pre-fix behaviour was UB.
TEST_F(TestSets, randMember_INT64_MIN_does_not_UB) {
  oktss::Sets mySets;
  mySets.add("s", {"a", "b", "c", "d", "e"});

  constexpr int64_t kMin = std::numeric_limits<int64_t>::min();
  bool myCompleted = false;
  try {
    auto myPicks = mySets.randMember("s", kMin);
    // If reserve happened to succeed (e.g. on a tiny set the
    // allocation went through), every pick must be from "s".
    for (const auto& myPick : myPicks) {
      EXPECT_TRUE(mySets.isMember("s", myPick));
    }
    myCompleted = true;
  } catch (const std::length_error&) {
    // The 9-quintillion-element reserve trips the allocator's
    // length check. Acceptable -- the point of the regression test
    // is that we never enter UB territory; throwing a typed C++
    // exception is well-defined behaviour.
    myCompleted = true;
  } catch (const std::bad_alloc&) {
    myCompleted = true;
  }
  EXPECT_TRUE(myCompleted);

  // The set is unchanged after the call.
  EXPECT_EQ(5u, mySets.cardinality("s"));
}
