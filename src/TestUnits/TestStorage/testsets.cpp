#include "Storage/sets.h"

#include "gtest/gtest.h"

#include <glog/logging.h>

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
