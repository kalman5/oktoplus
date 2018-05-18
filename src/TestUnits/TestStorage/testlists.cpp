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

  ASSERT_EQ(1u, myLists.pushBack("l1", {"one"}));
  ASSERT_EQ(2u, myLists.pushBack("l1", {"two"}));
  ASSERT_EQ(3u, myLists.pushBack("l1", {"three"}));

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

TEST_F(TestLists, insert) {
  okst::Lists myLists;

  ASSERT_EQ(1u, myLists.pushBack("l1", {"one"}));
  ASSERT_EQ(2u, myLists.pushBack("l1", {"two"}));
  ASSERT_EQ(3u, myLists.pushBack("l1", {"three"}));

  ASSERT_EQ(3u, myLists.size("l1").get());

  ASSERT_EQ(4, myLists.insert("l1", okst::Lists::Position::BEFORE, "one",   ".5").get());
  ASSERT_EQ(5, myLists.insert("l1", okst::Lists::Position::BEFORE, "two",   "one.5").get());
  ASSERT_EQ(6, myLists.insert("l1", okst::Lists::Position::AFTER,  "two",   "two.5").get());
  ASSERT_EQ(7, myLists.insert("l1", okst::Lists::Position::AFTER,  "three", "three.5").get());

  //not existing pivot
  ASSERT_EQ(-1, myLists.insert("l1", okst::Lists::Position::AFTER, "xxxxx", "yyyy").get());

  ASSERT_EQ(".5",      myLists.index("l1",  0).get());
  ASSERT_EQ("one",     myLists.index("l1",  1).get());
  ASSERT_EQ("one.5",   myLists.index("l1",  2).get());
  ASSERT_EQ("two",     myLists.index("l1",  3).get());
  ASSERT_EQ("two.5",   myLists.index("l1",  4).get());
  ASSERT_EQ("three",   myLists.index("l1",  5).get());
  ASSERT_EQ("three.5", myLists.index("l1",  6).get());
}

