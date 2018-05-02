#include "Storage/lists.h"

#include "gtest/gtest.h"

#include <glog/logging.h>

namespace okst = oktoplus::storage;

class TestLists : public ::testing::Test
{
 public:
  TestLists()
  {
  }

  void SetUp() override {

  }
};

TEST_F(TestLists, ctor) {
  okst::Lists myLists;
}


TEST_F(TestLists, index) {
  okst::Lists myLists;

  myLists.pushBack("l1", {"one"});
  myLists.pushBack("l1", {"two"});
  myLists.pushBack("l1", {"three"});

  ASSERT_EQ(3u, myLists.size("l1").get());

  // Index out of bound (positive)
  ASSERT_FALSE(myLists.index("l1",  3));
  // Index out of bound (negative)
  ASSERT_FALSE(myLists.index("l1", -4));

  ASSERT_EQ("one",   myLists.index("l1",  0).get());
  ASSERT_EQ("two",   myLists.index("l1",  1).get());
  ASSERT_EQ("three", myLists.index("l1",  2).get());
  ASSERT_EQ("three", myLists.index("l1", -1).get());
  ASSERT_EQ("two",   myLists.index("l1", -2).get());
  ASSERT_EQ("one",   myLists.index("l1", -3).get());
}

