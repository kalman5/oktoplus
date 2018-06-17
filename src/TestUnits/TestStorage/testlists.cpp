#include "Storage/lists.h"

#include "gtest/gtest.h"

#include <glog/logging.h>

namespace okst = oktoplus::storage;

class TestLists : public ::testing::Test
{
 public:
  TestLists() {
  }

  void SetUp() override {
  }
};

TEST_F(TestLists, ctor) {
  okst::Lists myLists;
}

TEST_F(TestLists, push_back_front_size) {
  okst::Lists myLists;

  // Single value
  ASSERT_EQ(1u, myLists.pushBack("l1", {"5"}));
  ASSERT_EQ(1u, myLists.pushBack("l2", {"2"}));
  ASSERT_EQ(2u, myLists.pushFront("l1", {"4"}));
  ASSERT_EQ(2u, myLists.pushFront("l2", {"1"}));

  // Multiple Value
  ASSERT_EQ(5u, myLists.pushFront("l1", {"3", "2", "1"}));
  ASSERT_EQ(7u, myLists.pushBack("l1", {"6", "7"}));

  ASSERT_EQ(7u, myLists.size("l1"));
  ASSERT_EQ(2u, myLists.size("l2"));

  // Not existing list
  ASSERT_EQ(0u, myLists.size("l3"));

  for (size_t i = 0; i < 7; ++i) {
    ASSERT_EQ(std::to_string(i + 1), myLists.index("l1", i).get());
  }

  ASSERT_EQ("1", myLists.index("l2", 0).get());
  ASSERT_EQ("2", myLists.index("l2", 1).get());
}

TEST_F(TestLists, pop_back) {
  okst::Lists myLists;

  ASSERT_EQ(5u, myLists.pushFront("l1", {"5", "4", "3", "2", "1"}));

  ASSERT_EQ("1", myLists.popFront("l1").get());
  ASSERT_EQ(4u, myLists.size("l1"));

  ASSERT_EQ("2", myLists.popFront("l1").get());
  ASSERT_EQ(3u, myLists.size("l1"));

  ASSERT_EQ("5", myLists.popBack("l1").get());
  ASSERT_EQ(2u, myLists.size("l1"));

  ASSERT_EQ("4", myLists.popBack("l1").get());
  ASSERT_EQ(1u, myLists.size("l1"));

  ASSERT_EQ("3", myLists.popBack("l1").get());

  // at this point the list is empty
  ASSERT_EQ(0u, myLists.size("l1"));
  ASSERT_EQ(0u, myLists.hostedKeys());

  ASSERT_FALSE(myLists.popBack("l1"));
  ASSERT_FALSE(myLists.popFront("l1"));
  ASSERT_FALSE(myLists.popBack("xxxx"));
  ASSERT_FALSE(myLists.popFront("xxxx"));
}

TEST_F(TestLists, index) {
  okst::Lists myLists;

  ASSERT_EQ(1u, myLists.pushBack("l1", {"one"}));
  ASSERT_EQ(2u, myLists.pushBack("l1", {"two"}));
  ASSERT_EQ(3u, myLists.pushBack("l1", {"three"}));

  ASSERT_EQ(3u, myLists.size("l1"));

  // Index out of bound (positive)
  ASSERT_FALSE(myLists.index("l1", 3));
  // Index out of bound (negative)
  ASSERT_FALSE(myLists.index("l1", -4));

  ASSERT_EQ("one", myLists.index("l1", 0).get());
  ASSERT_EQ("two", myLists.index("l1", 1).get());
  ASSERT_EQ("three", myLists.index("l1", 2).get());
  ASSERT_EQ("three", myLists.index("l1", -1).get());
  ASSERT_EQ("two", myLists.index("l1", -2).get());
  ASSERT_EQ("one", myLists.index("l1", -3).get());
}

TEST_F(TestLists, insert) {
  okst::Lists myLists;

  ASSERT_EQ(1u, myLists.pushBack("l1", {"one"}));
  ASSERT_EQ(2u, myLists.pushBack("l1", {"two"}));
  ASSERT_EQ(3u, myLists.pushBack("l1", {"three"}));

  ASSERT_EQ(3u, myLists.size("l1"));

  ASSERT_EQ(
      4,
      myLists.insert("l1", okst::Lists::Position::BEFORE, "one", ".5").get());
  ASSERT_EQ(5,
            myLists.insert("l1", okst::Lists::Position::BEFORE, "two", "one.5")
                .get());
  ASSERT_EQ(
      6,
      myLists.insert("l1", okst::Lists::Position::AFTER, "two", "two.5").get());
  ASSERT_EQ(
      7,
      myLists.insert("l1", okst::Lists::Position::AFTER, "three", "three.5")
          .get());

  // not existing pivot
  ASSERT_EQ(-1,
            myLists.insert("l1", okst::Lists::Position::AFTER, "xxxxx", "yyyy")
                .get());

  ASSERT_EQ(".5", myLists.index("l1", 0).get());
  ASSERT_EQ("one", myLists.index("l1", 1).get());
  ASSERT_EQ("one.5", myLists.index("l1", 2).get());
  ASSERT_EQ("two", myLists.index("l1", 3).get());
  ASSERT_EQ("two.5", myLists.index("l1", 4).get());
  ASSERT_EQ("three", myLists.index("l1", 5).get());
  ASSERT_EQ("three.5", myLists.index("l1", 6).get());
}

TEST_F(TestLists, range) {
  okst::Lists myLists;

  ASSERT_EQ(1u, myLists.pushBack("l1", {"1"}));
  ASSERT_EQ(2u, myLists.pushBack("l1", {"2"}));
  ASSERT_EQ(3u, myLists.pushBack("l1", {"3"}));

  ASSERT_EQ(3u, myLists.size("l1"));

  ASSERT_EQ(
    std::vector<std::string>({"1", "2", "3"}),
    myLists.range("l1", 0, 2));

  ASSERT_EQ(
    std::vector<std::string>({"1", "2", "3"}),
    myLists.range("l1", 0, 20));

  ASSERT_EQ(
    std::vector<std::string>({"2", "3"}),
    myLists.range("l1", 1, 2));

  ASSERT_EQ(
    std::vector<std::string>({"2", "3"}),
    myLists.range("l1", 1, 20));

  ASSERT_EQ(
    std::vector<std::string>(),
    myLists.range("l1", 10, 20));

  ASSERT_EQ(
    std::vector<std::string>({"1", "2", "3"}),
    myLists.range("l1", -3, -1));

  ASSERT_EQ(
    std::vector<std::string>({"1", "2", "3"}),
    myLists.range("l1", -30, -1));

  ASSERT_EQ(
    std::vector<std::string>({"1", "2"}),
    myLists.range("l1", -30, -2));

  ASSERT_EQ(
    std::vector<std::string>(),
    myLists.range("l1", -30, -10));

  ASSERT_EQ(
    std::vector<std::string>({"1", "2"}),
    myLists.range("l1", -30, 1));

  ASSERT_EQ(
    std::vector<std::string>({"1", "2", "3"}),
    myLists.range("l1", -30, 30));
}


TEST_F(TestLists, remove) {

  { // Not existing element
    okst::Lists myLists;
    ASSERT_EQ(8u, myLists.pushBack("l1", {"1","3","3","3","4","3","6","6"}));
    ASSERT_EQ(0u, myLists.remove("l1", 0, "7"));
    ASSERT_EQ(0u, myLists.remove("l1", -1, "7"));
    ASSERT_EQ(0u, myLists.remove("l1", 1, "7"));

    ASSERT_EQ(
      std::vector<std::string>({"1","3","3","3","4","3","6","6"}),
      myLists.range("l1", -30, 30));
  }

  { // all occurencies
    okst::Lists myLists;
    ASSERT_EQ(8u, myLists.pushBack("l1", {"1","3","3","3","4","3","6","6"}));
    ASSERT_EQ(4u, myLists.remove("l1", 0, "3"));


    ASSERT_EQ(
      std::vector<std::string>({"1","4","6","6"}),
      myLists.range("l1", -30, 30));
  }

  { // more than in the list from beginning
    okst::Lists myLists;
    ASSERT_EQ(8u, myLists.pushBack("l1", {"1","3","3","3","4","3","6","6"}));
    ASSERT_EQ(4u, myLists.remove("l1", 20, "3"));


    ASSERT_EQ(
      std::vector<std::string>({"1","4","6","6"}),
      myLists.range("l1", -30, 30));
  }

  { // same amount from left
    okst::Lists myLists;
    ASSERT_EQ(8u, myLists.pushBack("l1", {"1","3","3","3","4","3","6","6"}));
    ASSERT_EQ(4u, myLists.remove("l1", 4, "3"));


    ASSERT_EQ(
      std::vector<std::string>({"1","4","6","6"}),
      myLists.range("l1", -30, 30));
  }

  { // less amount from left
    okst::Lists myLists;
    ASSERT_EQ(8u, myLists.pushBack("l1", {"1","3","3","3","4","3","6","6"}));
    ASSERT_EQ(2u, myLists.remove("l1", 2, "3"));


    ASSERT_EQ(
      std::vector<std::string>({"1","3","4","3","6","6"}),
      myLists.range("l1", -30, 30));
  }

  { // more than in the list from right
    okst::Lists myLists;
    ASSERT_EQ(8u, myLists.pushBack("l1", {"1","3","3","3","4","3","6","6"}));
    ASSERT_EQ(4u, myLists.remove("l1", -20, "3"));


    ASSERT_EQ(
      std::vector<std::string>({"1","4","6","6"}),
      myLists.range("l1", -30, 30));
  }

  { // same amount from right
    okst::Lists myLists;
    ASSERT_EQ(8u, myLists.pushBack("l1", {"1","3","3","3","4","3","6","6"}));
    ASSERT_EQ(4u, myLists.remove("l1", -4, "3"));


    ASSERT_EQ(
      std::vector<std::string>({"1","4","6","6"}),
      myLists.range("l1", -30, 30));
  }

  { // less amount from right
    okst::Lists myLists;
    ASSERT_EQ(8u, myLists.pushBack("l1", {"1","3","3","3","4","3","6","6"}));
    ASSERT_EQ(3u, myLists.remove("l1", -3, "3"));


    ASSERT_EQ(
      std::vector<std::string>({"1","3","4","6","6"}),
      myLists.range("l1", -30, 30));
  }
}

TEST_F(TestLists, set) {

  { // Not existing list
    okst::Lists myLists;

    ASSERT_EQ(okst::Lists::Status::NOT_FOUND,
              myLists.set("xxxx", 2, "3"));
  }

  { // OUT OF RANGE
    okst::Lists myLists;
    ASSERT_EQ(8u, myLists.pushBack("l1", {"1","3","3","3","4","3","6","6"}));

    ASSERT_EQ(okst::Lists::Status::OUT_OF_RANGE,
              myLists.set("l1", 30, "3"));

    ASSERT_EQ(okst::Lists::Status::OUT_OF_RANGE,
              myLists.set("l1", -30, "3"));
  }

  { // valid range
    okst::Lists myLists;
    ASSERT_EQ(8u, myLists.pushBack("l1", {"1","2","3","4","5","6","7","8"}));

    ASSERT_EQ(okst::Lists::Status::OK,
              myLists.set("l1", 3, "40"));

    ASSERT_EQ(
      std::vector<std::string>({"1","2","3","40","5","6","7","8"}),
      myLists.range("l1", -30, 30));

    ASSERT_EQ(okst::Lists::Status::OK,
              myLists.set("l1", -3, "60"));

    ASSERT_EQ(
      std::vector<std::string>({"1","2","3","40","5","60","7","8"}),
      myLists.range("l1", -30, 30));
  }


}
