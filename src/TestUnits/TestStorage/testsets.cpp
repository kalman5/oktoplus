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
