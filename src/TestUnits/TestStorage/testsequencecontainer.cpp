#include "Storage/sequencecontainer.h"

#include "gtest/gtest.h"

#include <glog/logging.h>
#include <limits>
#include <optional>

namespace oktss = okts::stor;

class TestSequenceContainer : public ::testing::Test
{
 public:
};

TEST_F(TestSequenceContainer, ctor) {
  oktss::Lists myLists;
}

TEST_F(TestSequenceContainer, push_back_front_size) {
  oktss::Lists myLists;

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
    ASSERT_EQ(std::to_string(i + 1), myLists.index("l1", i).value());
  }

  ASSERT_EQ("1", myLists.index("l2", 0).value());
  ASSERT_EQ("2", myLists.index("l2", 1).value());
}

TEST_F(TestSequenceContainer, push_back_on_existing) {
  oktss::Lists myLists;

  ASSERT_EQ(0u, myLists.pushBackExist("l1", {"1"}));
  ASSERT_EQ(0u, myLists.size("l1"));
  ASSERT_EQ(0u, myLists.hostedKeys());

  // Single value
  ASSERT_EQ(1u, myLists.pushBack("l1", {"1"}));
  ASSERT_EQ(2u, myLists.pushBackExist("l1", {"2"}));
  ASSERT_EQ(2u, myLists.size("l1"));
  ASSERT_EQ(1u, myLists.hostedKeys());

  // Multiple Value
  ASSERT_EQ(5u, myLists.pushBackExist("l1", {"3", "4", "5"}));
  ASSERT_EQ(5u, myLists.size("l1"));
  ASSERT_EQ(1u, myLists.hostedKeys());

  for (size_t i = 0; i < 5; ++i) {
    ASSERT_EQ(std::to_string(i + 1), myLists.index("l1", i).value());
  }
}

TEST_F(TestSequenceContainer, push_front_on_existing) {
  oktss::Lists myLists;

  ASSERT_EQ(0u, myLists.pushFrontExist("l1", {"5"}));
  ASSERT_EQ(0u, myLists.size("l1"));
  ASSERT_EQ(0u, myLists.hostedKeys());

  // Single value
  ASSERT_EQ(1u, myLists.pushFront("l1", {"5"}));
  ASSERT_EQ(2u, myLists.pushFrontExist("l1", {"4"}));
  ASSERT_EQ(2u, myLists.size("l1"));
  ASSERT_EQ(1u, myLists.hostedKeys());

  // Multiple Value
  ASSERT_EQ(5u, myLists.pushFrontExist("l1", {"3", "2", "1"}));
  ASSERT_EQ(5u, myLists.size("l1"));
  ASSERT_EQ(1u, myLists.hostedKeys());

  for (size_t i = 0; i < 5; ++i) {
    ASSERT_EQ(std::to_string(i + 1), myLists.index("l1", i).value());
  }
}

TEST_F(TestSequenceContainer, pop_front_back) {
  oktss::Lists myLists;

  ASSERT_EQ(7u, myLists.pushFront("l1", {"7", "6", "5", "4", "3", "2", "1"}));

  ASSERT_EQ(std::list<std::string>{"1"}, myLists.popFront("l1", 1));
  ASSERT_EQ(6u, myLists.size("l1"));

  ASSERT_EQ(std::list<std::string>{"2"}, myLists.popFront("l1", 1));
  ASSERT_EQ(5u, myLists.size("l1"));

  ASSERT_EQ((std::list<std::string>{"3", "4"}), myLists.popFront("l1", 2));
  ASSERT_EQ(3u, myLists.size("l1"));

  ASSERT_EQ(std::list<std::string>{"7"}, myLists.popBack("l1", 1));
  ASSERT_EQ(2u, myLists.size("l1"));

  ASSERT_EQ((std::list<std::string>{"6", "5"}), myLists.popBack("l1", 2));

  // at this point the list is empty
  ASSERT_EQ(0u, myLists.size("l1"));
  ASSERT_EQ(0u, myLists.hostedKeys());

  ASSERT_TRUE(myLists.popBack("l1", 1).empty());
  ASSERT_TRUE(myLists.popFront("l1", 1).empty());
  ASSERT_TRUE(myLists.popBack("xxxx", 1).empty());
  ASSERT_TRUE(myLists.popFront("xxxx", 1).empty());
}

TEST_F(TestSequenceContainer, index) {
  oktss::Lists myLists;

  ASSERT_EQ(1u, myLists.pushBack("l1", {"one"}));
  ASSERT_EQ(2u, myLists.pushBack("l1", {"two"}));
  ASSERT_EQ(3u, myLists.pushBack("l1", {"three"}));

  ASSERT_EQ(3u, myLists.size("l1"));

  // Index out of bound (positive)
  ASSERT_FALSE(myLists.index("l1", 3));
  // Index out of bound (negative)
  ASSERT_FALSE(myLists.index("l1", -4));

  ASSERT_EQ("one", myLists.index("l1", 0).value());
  ASSERT_EQ("two", myLists.index("l1", 1).value());
  ASSERT_EQ("three", myLists.index("l1", 2).value());
  ASSERT_EQ("three", myLists.index("l1", -1).value());
  ASSERT_EQ("two", myLists.index("l1", -2).value());
  ASSERT_EQ("one", myLists.index("l1", -3).value());
}

TEST_F(TestSequenceContainer, insert) {
  oktss::Lists myLists;

  ASSERT_EQ(1u, myLists.pushBack("l1", {"one"}));
  ASSERT_EQ(2u, myLists.pushBack("l1", {"two"}));
  ASSERT_EQ(3u, myLists.pushBack("l1", {"three"}));

  ASSERT_EQ(3u, myLists.size("l1"));

  ASSERT_EQ(4,
            myLists.insert("l1", oktss::Lists::Position::BEFORE, "one", ".5")
                .value());
  ASSERT_EQ(5,
            myLists.insert("l1", oktss::Lists::Position::BEFORE, "two", "one.5")
                .value());
  ASSERT_EQ(6,
            myLists.insert("l1", oktss::Lists::Position::AFTER, "two", "two.5")
                .value());
  ASSERT_EQ(
      7,
      myLists.insert("l1", oktss::Lists::Position::AFTER, "three", "three.5")
          .value());

  // not existing pivot
  ASSERT_EQ(-1,
            myLists.insert("l1", oktss::Lists::Position::AFTER, "xxxxx", "yyyy")
                .value());

  ASSERT_EQ(".5", myLists.index("l1", 0).value());
  ASSERT_EQ("one", myLists.index("l1", 1).value());
  ASSERT_EQ("one.5", myLists.index("l1", 2).value());
  ASSERT_EQ("two", myLists.index("l1", 3).value());
  ASSERT_EQ("two.5", myLists.index("l1", 4).value());
  ASSERT_EQ("three", myLists.index("l1", 5).value());
  ASSERT_EQ("three.5", myLists.index("l1", 6).value());
}

TEST_F(TestSequenceContainer, range) {
  oktss::Lists myLists;

  ASSERT_EQ(1u, myLists.pushBack("l1", {"1"}));
  ASSERT_EQ(2u, myLists.pushBack("l1", {"2"}));
  ASSERT_EQ(3u, myLists.pushBack("l1", {"3"}));

  ASSERT_EQ(3u, myLists.size("l1"));

  ASSERT_EQ(std::list<std::string>({"1", "2", "3"}), myLists.range("l1", 0, 2));

  ASSERT_EQ(std::list<std::string>({"1", "2", "3"}),
            myLists.range("l1", 0, 20));

  ASSERT_EQ(std::list<std::string>({"2", "3"}), myLists.range("l1", 1, 2));

  ASSERT_EQ(std::list<std::string>({"2", "3"}), myLists.range("l1", 1, 20));

  ASSERT_EQ(std::list<std::string>(), myLists.range("l1", 10, 20));

  ASSERT_EQ(std::list<std::string>({"1", "2", "3"}),
            myLists.range("l1", -3, -1));

  ASSERT_EQ(std::list<std::string>({"1", "2", "3"}),
            myLists.range("l1", -30, -1));

  ASSERT_EQ(std::list<std::string>({"1", "2"}), myLists.range("l1", -30, -2));

  ASSERT_EQ(std::list<std::string>(), myLists.range("l1", -30, -10));

  ASSERT_EQ(std::list<std::string>({"1", "2"}), myLists.range("l1", -30, 1));

  ASSERT_EQ(std::list<std::string>({"1", "2", "3"}),
            myLists.range("l1", -30, 30));
}

TEST_F(TestSequenceContainer, remove) {

  { // Not existing element
    oktss::Lists myLists;
    ASSERT_EQ(8u,
              myLists.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));
    ASSERT_EQ(0u, myLists.remove("l1", 0, "7"));
    ASSERT_EQ(0u, myLists.remove("l1", -1, "7"));
    ASSERT_EQ(0u, myLists.remove("l1", 1, "7"));

    ASSERT_EQ(std::list<std::string>({"1", "3", "3", "3", "4", "3", "6", "6"}),
              myLists.range("l1", -30, 30));
  }

  { // all occurencies
    oktss::Lists myLists;
    ASSERT_EQ(8u,
              myLists.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));
    ASSERT_EQ(4u, myLists.remove("l1", 0, "3"));

    ASSERT_EQ(std::list<std::string>({"1", "4", "6", "6"}),
              myLists.range("l1", -30, 30));
  }

  { // more than in the list from beginning
    oktss::Lists myLists;
    ASSERT_EQ(8u,
              myLists.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));
    ASSERT_EQ(4u, myLists.remove("l1", 20, "3"));

    ASSERT_EQ(std::list<std::string>({"1", "4", "6", "6"}),
              myLists.range("l1", -30, 30));
  }

  { // same amount from left
    oktss::Lists myLists;
    ASSERT_EQ(8u,
              myLists.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));
    ASSERT_EQ(4u, myLists.remove("l1", 4, "3"));

    ASSERT_EQ(std::list<std::string>({"1", "4", "6", "6"}),
              myLists.range("l1", -30, 30));
  }

  { // less amount from left
    oktss::Lists myLists;
    ASSERT_EQ(8u,
              myLists.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));
    ASSERT_EQ(2u, myLists.remove("l1", 2, "3"));

    ASSERT_EQ(std::list<std::string>({"1", "3", "4", "3", "6", "6"}),
              myLists.range("l1", -30, 30));
  }

  { // more than in the list from right
    oktss::Lists myLists;
    ASSERT_EQ(8u,
              myLists.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));
    ASSERT_EQ(4u, myLists.remove("l1", -20, "3"));

    ASSERT_EQ(std::list<std::string>({"1", "4", "6", "6"}),
              myLists.range("l1", -30, 30));
  }

  { // same amount from right
    oktss::Lists myLists;
    ASSERT_EQ(8u,
              myLists.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));
    ASSERT_EQ(4u, myLists.remove("l1", -4, "3"));

    ASSERT_EQ(std::list<std::string>({"1", "4", "6", "6"}),
              myLists.range("l1", -30, 30));
  }

  { // less amount from right
    oktss::Lists myLists;
    ASSERT_EQ(8u,
              myLists.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));
    ASSERT_EQ(3u, myLists.remove("l1", -3, "3"));

    ASSERT_EQ(std::list<std::string>({"1", "3", "4", "6", "6"}),
              myLists.range("l1", -30, 30));
  }
}

TEST_F(TestSequenceContainer, set) {

  { // Not existing list
    oktss::Lists myLists;

    ASSERT_EQ(oktss::Lists::Status::NOT_FOUND, myLists.set("xxxx", 2, "3"));
  }

  { // OUT OF RANGE
    oktss::Lists myLists;
    ASSERT_EQ(8u,
              myLists.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));

    ASSERT_EQ(oktss::Lists::Status::OUT_OF_RANGE, myLists.set("l1", 30, "3"));

    ASSERT_EQ(oktss::Lists::Status::OUT_OF_RANGE, myLists.set("l1", -30, "3"));
  }

  { // valid range
    oktss::Lists myLists;
    ASSERT_EQ(8u,
              myLists.pushBack("l1", {"1", "2", "3", "4", "5", "6", "7", "8"}));

    ASSERT_EQ(oktss::Lists::Status::OK, myLists.set("l1", 3, "40"));

    ASSERT_EQ(std::list<std::string>({"1", "2", "3", "40", "5", "6", "7", "8"}),
              myLists.range("l1", -30, 30));

    ASSERT_EQ(oktss::Lists::Status::OK, myLists.set("l1", -3, "60"));

    ASSERT_EQ(
        std::list<std::string>({"1", "2", "3", "40", "5", "60", "7", "8"}),
        myLists.range("l1", -30, 30));
  }
}

TEST_F(TestSequenceContainer, move) {
  {
    oktss::Lists myLists;

    ASSERT_EQ(std::nullopt,
              myLists.move("l1",
                           "l2",
                           oktss::Lists::Direction::RIGHT,
                           oktss::Lists::Direction::LEFT));

    ASSERT_EQ(0u, myLists.hostedKeys());
  }

  {
    oktss::Lists myLists;

    ASSERT_EQ(1u, myLists.pushBack("l1", {"0"}));
    ASSERT_EQ("0",
              myLists
                  .move("l1",
                        "l2",
                        oktss::Lists::Direction::RIGHT,
                        oktss::Lists::Direction::LEFT)
                  .value());
    ASSERT_EQ(1u, myLists.hostedKeys());
  }

  {
    oktss::Lists myLists;

    ASSERT_EQ(2u, myLists.pushBack("l1", {"0", "1"}));
    ASSERT_EQ("1",
              myLists
                  .move("l1",
                        "l2",
                        oktss::Lists::Direction::RIGHT,
                        oktss::Lists::Direction::LEFT)
                  .value());
    ASSERT_EQ(2u, myLists.hostedKeys());

    ASSERT_EQ("0",
              myLists
                  .move("l1",
                        "l2",
                        oktss::Lists::Direction::RIGHT,
                        oktss::Lists::Direction::LEFT)
                  .value());
    ASSERT_EQ(1u, myLists.hostedKeys());

    ASSERT_EQ("0", myLists.popFront("l2", 1).front());
    ASSERT_EQ("1", myLists.popFront("l2", 1).front());
    ASSERT_EQ(0u, myLists.hostedKeys());
  }

  {
    oktss::Lists myLists;

    ASSERT_EQ(2u, myLists.pushBack("l1", {"0", "1"}));
    ASSERT_EQ("0",
              myLists
                  .move("l1",
                        "l2",
                        oktss::Lists::Direction::LEFT,
                        oktss::Lists::Direction::RIGHT)
                  .value());
    ASSERT_EQ(2u, myLists.hostedKeys());

    ASSERT_EQ("1",
              myLists
                  .move("l1",
                        "l2",
                        oktss::Lists::Direction::LEFT,
                        oktss::Lists::Direction::RIGHT)
                  .value());
    ASSERT_EQ(1u, myLists.hostedKeys());

    ASSERT_EQ("0", myLists.popFront("l2", 1).front());
    ASSERT_EQ("1", myLists.popFront("l2", 1).front());
    ASSERT_EQ(0u, myLists.hostedKeys());
  }

  {
    oktss::Lists myLists;

    ASSERT_EQ(3u, myLists.pushBack("l1", {"0", "1", "2"}));
    ASSERT_EQ("2",
              myLists
                  .move("l1",
                        "l1",
                        oktss::Lists::Direction::RIGHT,
                        oktss::Lists::Direction::LEFT)
                  .value());
    ASSERT_EQ("1",
              myLists
                  .move("l1",
                        "l1",
                        oktss::Lists::Direction::RIGHT,
                        oktss::Lists::Direction::LEFT)
                  .value());

    ASSERT_EQ("1", myLists.popFront("l1", 1).front());
    ASSERT_EQ("2", myLists.popFront("l1", 1).front());
    ASSERT_EQ("0", myLists.popFront("l1", 1).front());
    ASSERT_EQ(0u, myLists.hostedKeys());
  }
}

TEST_F(TestSequenceContainer, trim) {

  {
    oktss::Lists myLists;
    ASSERT_EQ(8u,
              myLists.pushBack("l1", {"0", "1", "2", "3", "4", "5", "6", "7"}));

    myLists.trim("l1", 0, 7);

    ASSERT_EQ(std::list<std::string>({"0", "1", "2", "3", "4", "5", "6", "7"}),
              myLists.range("l1", -30, 30));
  }

  {
    oktss::Lists myLists;
    ASSERT_EQ(8u,
              myLists.pushBack("l1", {"0", "1", "2", "3", "4", "5", "6", "7"}));

    myLists.trim("l1", -8, -1);

    ASSERT_EQ(std::list<std::string>({"0", "1", "2", "3", "4", "5", "6", "7"}),
              myLists.range("l1", -30, 30));
  }

  {
    oktss::Lists myLists;
    ASSERT_EQ(8u,
              myLists.pushBack("l1", {"0", "1", "2", "3", "4", "5", "6", "7"}));

    myLists.trim("l1", 5, 4);

    ASSERT_EQ(std::list<std::string>({}), myLists.range("l1", -30, 30));
    ASSERT_EQ(0u, myLists.hostedKeys());
  }

  {
    oktss::Lists myLists;
    ASSERT_EQ(8u,
              myLists.pushBack("l1", {"0", "1", "2", "3", "4", "5", "6", "7"}));

    myLists.trim("l1", 15, 30);

    ASSERT_EQ(std::list<std::string>({}), myLists.range("l1", -30, 30));
    ASSERT_EQ(0u, myLists.hostedKeys());
  }

  {
    oktss::Lists myLists;
    ASSERT_EQ(8u,
              myLists.pushBack("l1", {"0", "1", "2", "3", "4", "5", "6", "7"}));

    myLists.trim("l1", 1, 6);

    ASSERT_EQ(std::list<std::string>({"1", "2", "3", "4", "5", "6"}),
              myLists.range("l1", -30, 30));
  }

  {
    oktss::Lists myLists;
    ASSERT_EQ(8u,
              myLists.pushBack("l1", {"0", "1", "2", "3", "4", "5", "6", "7"}));

    myLists.trim("l1", 5, 5);

    ASSERT_EQ(std::list<std::string>({"5"}), myLists.range("l1", -30, 30));
  }

  {
    oktss::Lists myLists;
    ASSERT_EQ(8u,
              myLists.pushBack("l1", {"0", "1", "2", "3", "4", "5", "6", "7"}));

    myLists.trim("l1", 5, 50);

    ASSERT_EQ(std::list<std::string>({"5", "6", "7"}),
              myLists.range("l1", -30, 30));
  }
}

TEST_F(TestSequenceContainer, position) {
  oktss::Lists myLists;
  ASSERT_EQ(8u,
            myLists.pushBack("l1", {"a", "b", "c", "d", "c", "e", "a", "b"}));

  // Rank = 1, Count = 1, MaxLenght = max
  ASSERT_EQ(
      std::list<uint64_t>({0}),
      myLists.position("l1", "a", 1, 1, std::numeric_limits<uint64_t>::max()));
  // MaxLength == 0 => max uint64
  ASSERT_EQ(std::list<uint64_t>({0}), myLists.position("l1", "a", 1, 1, 0));

  // Rank = 1, Count = 2, MaxLenght = max
  ASSERT_EQ(
      std::list<uint64_t>({0, 6}),
      myLists.position("l1", "a", 1, 2, std::numeric_limits<uint64_t>::max()));

  // Rank = 2, Count = 1, MaxLenght = max
  ASSERT_EQ(
      std::list<uint64_t>({6}),
      myLists.position("l1", "a", 2, 1, std::numeric_limits<uint64_t>::max()));

  // Rank = 1, Count = 1, MaxLenght = 3
  ASSERT_EQ(std::list<uint64_t>({}), myLists.position("l1", "d", 1, 1, 3));

  // Rank = 1, Count = 1, MaxLenght = 4
  ASSERT_EQ(std::list<uint64_t>({3}), myLists.position("l1", "d", 1, 1, 4));

  // Rank = -1, Count = 1, MaxLenght = max
  ASSERT_EQ(
      std::list<uint64_t>({6}),
      myLists.position("l1", "a", -1, 1, std::numeric_limits<uint64_t>::max()));

  // Rank = -2, Count = 1, MaxLenght = 0
  ASSERT_EQ(std::list<uint64_t>({0}), myLists.position("l1", "a", -2, 1, 0));

  oktss::Lists myListsOneEntry;
  ASSERT_EQ(1u, myLists.pushBack("l2", {"a"}));

  // Rank = -1/1, Count = 1, MaxLenght = 0
  ASSERT_EQ(std::list<uint64_t>({0}), myLists.position("l2", "a", -1, 1, 0));
  ASSERT_EQ(std::list<uint64_t>({0}), myLists.position("l2", "a", 1, 1, 0));
}
