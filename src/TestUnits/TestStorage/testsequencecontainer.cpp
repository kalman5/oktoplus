#include "Storage/sequencecontainer.h"

#include "gtest/gtest.h"

#include <glog/logging.h>
#include <limits>
#include <optional>

namespace oktss = okts::stor;

template <class>
struct TestSequenceContainer : ::testing::Test {
};

using VectorTypeList = ::testing::Types<std::list<std::string>,
                                        std::deque<std::string>,
                                        std::vector<std::string>>;

TYPED_TEST_SUITE(TestSequenceContainer, VectorTypeList);

TYPED_TEST(TestSequenceContainer, ctor) {
  using Container = oktss::SequenceContainer<TypeParam>;
  Container myContainer;
}

TYPED_TEST(TestSequenceContainer, push_back_front_size) {
  if constexpr (std::is_same_v<TypeParam, std::vector<std::string>>) {
    GTEST_SKIP();
  } else {
    using Container = oktss::SequenceContainer<TypeParam>;
    Container myContainer;

    // Single value
    ASSERT_EQ(1u, myContainer.pushBack("l1", {"5"}));
    ASSERT_EQ(1u, myContainer.pushBack("l2", {"2"}));
    ASSERT_EQ(2u, myContainer.pushFront("l1", {"4"}));
    ASSERT_EQ(2u, myContainer.pushFront("l2", {"1"}));

    // Multiple Value
    ASSERT_EQ(5u, myContainer.pushFront("l1", {"3", "2", "1"}));
    ASSERT_EQ(7u, myContainer.pushBack("l1", {"6", "7"}));

    ASSERT_EQ(7u, myContainer.size("l1"));
    ASSERT_EQ(2u, myContainer.size("l2"));

    // Not existing list
    ASSERT_EQ(0u, myContainer.size("l3"));

    for (size_t i = 0; i < 7; ++i) {
      ASSERT_EQ(std::to_string(i + 1), myContainer.index("l1", i).value());
    }

    ASSERT_EQ("1", myContainer.index("l2", 0).value());
    ASSERT_EQ("2", myContainer.index("l2", 1).value());
  }
}

TYPED_TEST(TestSequenceContainer, push_back_on_existing) {
  using Container = oktss::SequenceContainer<TypeParam>;
  Container myContainer;

  ASSERT_EQ(0u, myContainer.pushBackExist("l1", {"1"}));
  ASSERT_EQ(0u, myContainer.size("l1"));
  ASSERT_EQ(0u, myContainer.hostedKeys());

  // Single value
  ASSERT_EQ(1u, myContainer.pushBack("l1", {"1"}));
  ASSERT_EQ(2u, myContainer.pushBackExist("l1", {"2"}));
  ASSERT_EQ(2u, myContainer.size("l1"));
  ASSERT_EQ(1u, myContainer.hostedKeys());

  // Multiple Value
  ASSERT_EQ(5u, myContainer.pushBackExist("l1", {"3", "4", "5"}));
  ASSERT_EQ(5u, myContainer.size("l1"));
  ASSERT_EQ(1u, myContainer.hostedKeys());

  for (size_t i = 0; i < 5; ++i) {
    ASSERT_EQ(std::to_string(i + 1), myContainer.index("l1", i).value());
  }
}

TYPED_TEST(TestSequenceContainer, push_front_on_existing) {
  if constexpr (std::is_same_v<TypeParam, std::vector<std::string>>) {
    GTEST_SKIP();
  } else {
    using Container = oktss::SequenceContainer<TypeParam>;
    Container myContainer;

    ASSERT_EQ(0u, myContainer.pushFrontExist("l1", {"5"}));
    ASSERT_EQ(0u, myContainer.size("l1"));
    ASSERT_EQ(0u, myContainer.hostedKeys());

    // Single value
    ASSERT_EQ(1u, myContainer.pushFront("l1", {"5"}));
    ASSERT_EQ(2u, myContainer.pushFrontExist("l1", {"4"}));
    ASSERT_EQ(2u, myContainer.size("l1"));
    ASSERT_EQ(1u, myContainer.hostedKeys());

    // Multiple Value
    ASSERT_EQ(5u, myContainer.pushFrontExist("l1", {"3", "2", "1"}));
    ASSERT_EQ(5u, myContainer.size("l1"));
    ASSERT_EQ(1u, myContainer.hostedKeys());

    for (size_t i = 0; i < 5; ++i) {
      ASSERT_EQ(std::to_string(i + 1), myContainer.index("l1", i).value());
    }
  }
}

TYPED_TEST(TestSequenceContainer, pop_front_back) {
  if constexpr (std::is_same_v<TypeParam, std::vector<std::string>>) {
    GTEST_SKIP();
  } else {

    using Container = oktss::SequenceContainer<TypeParam>;
    Container myContainer;

    ASSERT_EQ(7u,
              myContainer.pushFront("l1", {"7", "6", "5", "4", "3", "2", "1"}));

    ASSERT_EQ(std::vector<std::string>{"1"}, myContainer.popFront("l1", 1));
    ASSERT_EQ(6u, myContainer.size("l1"));

    ASSERT_EQ(std::vector<std::string>{"2"}, myContainer.popFront("l1", 1));
    ASSERT_EQ(5u, myContainer.size("l1"));

    ASSERT_EQ((std::vector<std::string>{"3", "4"}),
              myContainer.popFront("l1", 2));
    ASSERT_EQ(3u, myContainer.size("l1"));

    ASSERT_EQ(std::vector<std::string>{"7"}, myContainer.popBack("l1", 1));
    ASSERT_EQ(2u, myContainer.size("l1"));

    ASSERT_EQ((std::vector<std::string>{"6", "5"}), myContainer.popBack("l1", 2));

    // at this point the list is empty
    ASSERT_EQ(0u, myContainer.size("l1"));
    ASSERT_EQ(0u, myContainer.hostedKeys());

    ASSERT_TRUE(myContainer.popBack("l1", 1).empty());
    ASSERT_TRUE(myContainer.popFront("l1", 1).empty());
    ASSERT_TRUE(myContainer.popBack("xxxx", 1).empty());
    ASSERT_TRUE(myContainer.popFront("xxxx", 1).empty());
  }
}

TYPED_TEST(TestSequenceContainer, index) {
  using Container = oktss::SequenceContainer<TypeParam>;
  Container myContainer;

  ASSERT_EQ(1u, myContainer.pushBack("l1", {"one"}));
  ASSERT_EQ(2u, myContainer.pushBack("l1", {"two"}));
  ASSERT_EQ(3u, myContainer.pushBack("l1", {"three"}));

  ASSERT_EQ(3u, myContainer.size("l1"));

  // Index out of bound (positive)
  ASSERT_FALSE(myContainer.index("l1", 3));
  // Index out of bound (negative)
  ASSERT_FALSE(myContainer.index("l1", -4));

  ASSERT_EQ("one", myContainer.index("l1", 0).value());
  ASSERT_EQ("two", myContainer.index("l1", 1).value());
  ASSERT_EQ("three", myContainer.index("l1", 2).value());
  ASSERT_EQ("three", myContainer.index("l1", -1).value());
  ASSERT_EQ("two", myContainer.index("l1", -2).value());
  ASSERT_EQ("one", myContainer.index("l1", -3).value());
}

TYPED_TEST(TestSequenceContainer, insert) {
  using Container = oktss::SequenceContainer<TypeParam>;
  Container myContainer;

  ASSERT_EQ(1u, myContainer.pushBack("l1", {"one"}));
  ASSERT_EQ(2u, myContainer.pushBack("l1", {"two"}));
  ASSERT_EQ(3u, myContainer.pushBack("l1", {"three"}));

  ASSERT_EQ(3u, myContainer.size("l1"));

  ASSERT_EQ(4,
            myContainer.insert("l1", Container::Position::BEFORE, "one", ".5")
                .value());
  ASSERT_EQ(
      5,
      myContainer.insert("l1", Container::Position::BEFORE, "two", "one.5")
          .value());
  ASSERT_EQ(6,
            myContainer.insert("l1", Container::Position::AFTER, "two", "two.5")
                .value());
  ASSERT_EQ(
      7,
      myContainer.insert("l1", Container::Position::AFTER, "three", "three.5")
          .value());

  // not existing pivot
  ASSERT_EQ(
      -1,
      myContainer.insert("l1", Container::Position::AFTER, "xxxxx", "yyyy")
          .value());

  ASSERT_EQ(".5", myContainer.index("l1", 0).value());
  ASSERT_EQ("one", myContainer.index("l1", 1).value());
  ASSERT_EQ("one.5", myContainer.index("l1", 2).value());
  ASSERT_EQ("two", myContainer.index("l1", 3).value());
  ASSERT_EQ("two.5", myContainer.index("l1", 4).value());
  ASSERT_EQ("three", myContainer.index("l1", 5).value());
  ASSERT_EQ("three.5", myContainer.index("l1", 6).value());
}

TYPED_TEST(TestSequenceContainer, range) {
  using Container = oktss::SequenceContainer<TypeParam>;
  Container myContainer;

  ASSERT_EQ(1u, myContainer.pushBack("l1", {"1"}));
  ASSERT_EQ(2u, myContainer.pushBack("l1", {"2"}));
  ASSERT_EQ(3u, myContainer.pushBack("l1", {"3"}));

  ASSERT_EQ(3u, myContainer.size("l1"));

  ASSERT_EQ(std::vector<std::string>({"1", "2", "3"}),
            myContainer.range("l1", 0, 2));

  ASSERT_EQ(std::vector<std::string>({"1", "2", "3"}),
            myContainer.range("l1", 0, 20));

  ASSERT_EQ(std::vector<std::string>({"2", "3"}), myContainer.range("l1", 1, 2));

  ASSERT_EQ(std::vector<std::string>({"2", "3"}), myContainer.range("l1", 1, 20));

  ASSERT_EQ(std::vector<std::string>(), myContainer.range("l1", 10, 20));

  ASSERT_EQ(std::vector<std::string>({"1", "2", "3"}),
            myContainer.range("l1", -3, -1));

  ASSERT_EQ(std::vector<std::string>({"1", "2", "3"}),
            myContainer.range("l1", -30, -1));

  ASSERT_EQ(std::vector<std::string>({"1", "2"}),
            myContainer.range("l1", -30, -2));

  ASSERT_EQ(std::vector<std::string>(), myContainer.range("l1", -30, -10));

  ASSERT_EQ(std::vector<std::string>({"1", "2"}),
            myContainer.range("l1", -30, 1));

  ASSERT_EQ(std::vector<std::string>({"1", "2", "3"}),
            myContainer.range("l1", -30, 30));
}

TYPED_TEST(TestSequenceContainer, remove) {
  using Container = oktss::SequenceContainer<TypeParam>;

  { // Not existing element
    Container myContainer;
    ASSERT_EQ(
        8u,
        myContainer.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));
    ASSERT_EQ(0u, myContainer.remove("l1", 0, "7"));
    ASSERT_EQ(0u, myContainer.remove("l1", -1, "7"));
    ASSERT_EQ(0u, myContainer.remove("l1", 1, "7"));

    ASSERT_EQ(std::vector<std::string>({"1", "3", "3", "3", "4", "3", "6", "6"}),
              myContainer.range("l1", -30, 30));
  }

  { // all occurencies
    Container myContainer;
    ASSERT_EQ(
        8u,
        myContainer.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));
    ASSERT_EQ(4u, myContainer.remove("l1", 0, "3"));

    ASSERT_EQ(std::vector<std::string>({"1", "4", "6", "6"}),
              myContainer.range("l1", -30, 30));
  }

  { // more than in the list from beginning
    Container myContainer;
    ASSERT_EQ(
        8u,
        myContainer.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));
    ASSERT_EQ(4u, myContainer.remove("l1", 20, "3"));

    ASSERT_EQ(std::vector<std::string>({"1", "4", "6", "6"}),
              myContainer.range("l1", -30, 30));
  }

  { // same amount from left
    Container myContainer;
    ASSERT_EQ(
        8u,
        myContainer.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));
    ASSERT_EQ(4u, myContainer.remove("l1", 4, "3"));

    ASSERT_EQ(std::vector<std::string>({"1", "4", "6", "6"}),
              myContainer.range("l1", -30, 30));
  }

  { // less amount from left
    Container myContainer;
    ASSERT_EQ(
        8u,
        myContainer.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));
    ASSERT_EQ(2u, myContainer.remove("l1", 2, "3"));

    ASSERT_EQ(std::vector<std::string>({"1", "3", "4", "3", "6", "6"}),
              myContainer.range("l1", -30, 30));
  }

  { // more than in the list from right
    Container myContainer;
    ASSERT_EQ(
        8u,
        myContainer.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));
    ASSERT_EQ(4u, myContainer.remove("l1", -20, "3"));

    ASSERT_EQ(std::vector<std::string>({"1", "4", "6", "6"}),
              myContainer.range("l1", -30, 30));
  }

  { // same amount from right
    Container myContainer;
    ASSERT_EQ(
        8u,
        myContainer.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));
    ASSERT_EQ(4u, myContainer.remove("l1", -4, "3"));

    ASSERT_EQ(std::vector<std::string>({"1", "4", "6", "6"}),
              myContainer.range("l1", -30, 30));
  }

  { // less amount from right
    Container myContainer;
    ASSERT_EQ(
        8u,
        myContainer.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));
    ASSERT_EQ(3u, myContainer.remove("l1", -3, "3"));

    ASSERT_EQ(std::vector<std::string>({"1", "3", "4", "6", "6"}),
              myContainer.range("l1", -30, 30));
  }
}

TYPED_TEST(TestSequenceContainer, set) {

  using Container = oktss::SequenceContainer<TypeParam>;

  { // Not existing list
    Container myContainer;

    ASSERT_EQ(Container::Status::NOT_FOUND, myContainer.set("xxxx", 2, "3"));
  }

  { // OUT OF RANGE
    Container myContainer;
    ASSERT_EQ(
        8u,
        myContainer.pushBack("l1", {"1", "3", "3", "3", "4", "3", "6", "6"}));

    ASSERT_EQ(Container::Status::OUT_OF_RANGE, myContainer.set("l1", 30, "3"));

    ASSERT_EQ(Container::Status::OUT_OF_RANGE, myContainer.set("l1", -30, "3"));
  }

  { // valid range
    Container myContainer;
    ASSERT_EQ(
        8u,
        myContainer.pushBack("l1", {"1", "2", "3", "4", "5", "6", "7", "8"}));

    ASSERT_EQ(Container::Status::OK, myContainer.set("l1", 3, "40"));

    ASSERT_EQ(std::vector<std::string>({"1", "2", "3", "40", "5", "6", "7", "8"}),
              myContainer.range("l1", -30, 30));

    ASSERT_EQ(Container::Status::OK, myContainer.set("l1", -3, "60"));

    ASSERT_EQ(
        std::vector<std::string>({"1", "2", "3", "40", "5", "60", "7", "8"}),
        myContainer.range("l1", -30, 30));
  }
}

TYPED_TEST(TestSequenceContainer, move) {
  if constexpr (std::is_same_v<TypeParam, std::vector<std::string>>) {
    GTEST_SKIP();
  } else {

    using Container = oktss::SequenceContainer<TypeParam>;

    {
      Container myContainer;

      ASSERT_EQ(std::nullopt,
                myContainer.move("l1",
                                 "l2",
                                 Container::Direction::RIGHT,
                                 Container::Direction::LEFT));

      ASSERT_EQ(0u, myContainer.hostedKeys());
    }

    {
      Container myContainer;

      ASSERT_EQ(1u, myContainer.pushBack("l1", {"0"}));
      ASSERT_EQ("0",
                myContainer
                    .move("l1",
                          "l2",
                          Container::Direction::RIGHT,
                          Container::Direction::LEFT)
                    .value());
      ASSERT_EQ(1u, myContainer.hostedKeys());
    }

    {
      Container myContainer;

      ASSERT_EQ(2u, myContainer.pushBack("l1", {"0", "1"}));
      ASSERT_EQ("1",
                myContainer
                    .move("l1",
                          "l2",
                          Container::Direction::RIGHT,
                          Container::Direction::LEFT)
                    .value());
      ASSERT_EQ(2u, myContainer.hostedKeys());

      ASSERT_EQ("0",
                myContainer
                    .move("l1",
                          "l2",
                          Container::Direction::RIGHT,
                          Container::Direction::LEFT)
                    .value());
      ASSERT_EQ(1u, myContainer.hostedKeys());

      ASSERT_EQ("0", myContainer.popFront("l2", 1).front());
      ASSERT_EQ("1", myContainer.popFront("l2", 1).front());
      ASSERT_EQ(0u, myContainer.hostedKeys());
    }

    {
      Container myContainer;

      ASSERT_EQ(2u, myContainer.pushBack("l1", {"0", "1"}));
      ASSERT_EQ("0",
                myContainer
                    .move("l1",
                          "l2",
                          Container::Direction::LEFT,
                          Container::Direction::RIGHT)
                    .value());
      ASSERT_EQ(2u, myContainer.hostedKeys());

      ASSERT_EQ("1",
                myContainer
                    .move("l1",
                          "l2",
                          Container::Direction::LEFT,
                          Container::Direction::RIGHT)
                    .value());
      ASSERT_EQ(1u, myContainer.hostedKeys());

      ASSERT_EQ("0", myContainer.popFront("l2", 1).front());
      ASSERT_EQ("1", myContainer.popFront("l2", 1).front());
      ASSERT_EQ(0u, myContainer.hostedKeys());
    }

    {
      Container myContainer;

      ASSERT_EQ(3u, myContainer.pushBack("l1", {"0", "1", "2"}));
      ASSERT_EQ("2",
                myContainer
                    .move("l1",
                          "l1",
                          Container::Direction::RIGHT,
                          Container::Direction::LEFT)
                    .value());
      ASSERT_EQ("1",
                myContainer
                    .move("l1",
                          "l1",
                          Container::Direction::RIGHT,
                          Container::Direction::LEFT)
                    .value());

      ASSERT_EQ("1", myContainer.popFront("l1", 1).front());
      ASSERT_EQ("2", myContainer.popFront("l1", 1).front());
      ASSERT_EQ("0", myContainer.popFront("l1", 1).front());
      ASSERT_EQ(0u, myContainer.hostedKeys());
    }
  }
}

TYPED_TEST(TestSequenceContainer, trim) {
  using Container = oktss::SequenceContainer<TypeParam>;

  {
    Container myContainer;
    ASSERT_EQ(
        8u,
        myContainer.pushBack("l1", {"0", "1", "2", "3", "4", "5", "6", "7"}));

    myContainer.trim("l1", 0, 7);

    ASSERT_EQ(8u, myContainer.size("l1"));

    ASSERT_EQ(std::vector<std::string>({"0", "1", "2", "3", "4", "5", "6", "7"}),
              myContainer.range("l1", -30, 30));
  }

  {
    Container myContainer;
    ASSERT_EQ(
        8u,
        myContainer.pushBack("l1", {"0", "1", "2", "3", "4", "5", "6", "7"}));

    myContainer.trim("l1", -8, -1);

    ASSERT_EQ(8u, myContainer.size("l1"));

    ASSERT_EQ(std::vector<std::string>({"0", "1", "2", "3", "4", "5", "6", "7"}),
              myContainer.range("l1", -30, 30));
  }

  {
    Container myContainer;
    ASSERT_EQ(
        8u,
        myContainer.pushBack("l1", {"0", "1", "2", "3", "4", "5", "6", "7"}));

    myContainer.trim("l1", 5, 4);

    ASSERT_EQ(0u, myContainer.size("l1"));

    ASSERT_EQ(std::vector<std::string>({}), myContainer.range("l1", -30, 30));
    ASSERT_EQ(0u, myContainer.hostedKeys());
  }

  {
    Container myContainer;
    ASSERT_EQ(
        8u,
        myContainer.pushBack("l1", {"0", "1", "2", "3", "4", "5", "6", "7"}));

    myContainer.trim("l1", 15, 30);

    ASSERT_EQ(0u, myContainer.size("l1"));

    ASSERT_EQ(std::vector<std::string>({}), myContainer.range("l1", -30, 30));
    ASSERT_EQ(0u, myContainer.hostedKeys());
  }

  {
    Container myContainer;
    ASSERT_EQ(
        8u,
        myContainer.pushBack("l1", {"0", "1", "2", "3", "4", "5", "6", "7"}));

    myContainer.trim("l1", 1, 6);

    ASSERT_EQ(6u, myContainer.size("l1"));

    ASSERT_EQ(std::vector<std::string>({"1", "2", "3", "4", "5", "6"}),
              myContainer.range("l1", -30, 30));
  }

  {
    Container myContainer;
    ASSERT_EQ(
        8u,
        myContainer.pushBack("l1", {"0", "1", "2", "3", "4", "5", "6", "7"}));

    myContainer.trim("l1", 5, 5);

    ASSERT_EQ(1u, myContainer.size("l1"));

    ASSERT_EQ(std::vector<std::string>({"5"}), myContainer.range("l1", -30, 30));
  }

  {
    Container myContainer;
    ASSERT_EQ(
        8u,
        myContainer.pushBack("l1", {"0", "1", "2", "3", "4", "5", "6", "7"}));

    myContainer.trim("l1", 5, 50);

    ASSERT_EQ(std::vector<std::string>({"5", "6", "7"}),
              myContainer.range("l1", -30, 30));
  }
}

TYPED_TEST(TestSequenceContainer, position) {
  using Container = oktss::SequenceContainer<TypeParam>;

  Container myContainer;

  ASSERT_EQ(
      8u, myContainer.pushBack("l1", {"a", "b", "c", "d", "c", "e", "a", "b"}));

  // Rank = 1, Count = 1, MaxLenght = max
  ASSERT_EQ(std::list<uint64_t>({0}),
            myContainer.position(
                "l1", "a", 1, 1, std::numeric_limits<uint64_t>::max()));
  // MaxLength == 0 => max uint64
  ASSERT_EQ(std::list<uint64_t>({0}), myContainer.position("l1", "a", 1, 1, 0));

  // Rank = 1, Count = 2, MaxLenght = max
  ASSERT_EQ(std::list<uint64_t>({0, 6}),
            myContainer.position(
                "l1", "a", 1, 2, std::numeric_limits<uint64_t>::max()));

  // Rank = 2, Count = 1, MaxLenght = max
  ASSERT_EQ(std::list<uint64_t>({6}),
            myContainer.position(
                "l1", "a", 2, 1, std::numeric_limits<uint64_t>::max()));

  // Rank = 1, Count = 1, MaxLenght = 3
  ASSERT_EQ(std::list<uint64_t>({}), myContainer.position("l1", "d", 1, 1, 3));

  // Rank = 1, Count = 1, MaxLenght = 4
  ASSERT_EQ(std::list<uint64_t>({3}), myContainer.position("l1", "d", 1, 1, 4));

  // Rank = -1, Count = 1, MaxLenght = max
  ASSERT_EQ(std::list<uint64_t>({6}),
            myContainer.position(
                "l1", "a", -1, 1, std::numeric_limits<uint64_t>::max()));

  // Rank = -2, Count = 1, MaxLenght = 0
  ASSERT_EQ(std::list<uint64_t>({0}),
            myContainer.position("l1", "a", -2, 1, 0));

  oktss::Lists myListsOneEntry;
  ASSERT_EQ(1u, myContainer.pushBack("l2", {"a"}));

  // Rank = -1/1, Count = 1, MaxLenght = 0
  ASSERT_EQ(std::list<uint64_t>({0}),
            myContainer.position("l2", "a", -1, 1, 0));
  ASSERT_EQ(std::list<uint64_t>({0}), myContainer.position("l2", "a", 1, 1, 0));
}

// Regression: INT64_MIN signed-negation UB in range/trim/index/set
// (fixed in 4ca43f5). Bare `-aStart` and `std::abs(aIndex+1)` overflow
// when a wire client passes the literal `-9223372036854775808`. Each
// of the four entry points must (a) not crash and (b) produce a
// well-defined result. detail::safeAbs is the underlying fix.
TYPED_TEST(TestSequenceContainer, int64_min_indices_dont_UB) {
  using Container = oktss::SequenceContainer<TypeParam>;
  constexpr int64_t kMin = std::numeric_limits<int64_t>::min();

  Container myContainer;
  ASSERT_EQ(1u, myContainer.pushBack("l1", {"a"}));
  ASSERT_EQ(2u, myContainer.pushBack("l1", {"b"}));
  ASSERT_EQ(3u, myContainer.pushBack("l1", {"c"}));

  // index() with INT64_MIN: well-defined out-of-range, returns nullopt.
  ASSERT_FALSE(myContainer.index("l1", kMin));

  // set() with INT64_MIN: returns OUT_OF_RANGE, doesn't mutate the list.
  ASSERT_EQ(Container::Status::OUT_OF_RANGE,
            myContainer.set("l1", kMin, "X"));
  ASSERT_EQ("a", myContainer.index("l1", 0).value());
  ASSERT_EQ("b", myContainer.index("l1", 1).value());
  ASSERT_EQ("c", myContainer.index("l1", 2).value());

  // range() with INT64_MIN start: clamped to 0; range(min, 0) -> [a].
  ASSERT_EQ(std::vector<std::string>({"a"}),
            myContainer.range("l1", kMin, 0));
  // range() with INT64_MIN stop: empty (stop is below any index).
  ASSERT_EQ(std::vector<std::string>{},
            myContainer.range("l1", 0, kMin));
  // range() with INT64_MIN on both: degenerate → empty.
  ASSERT_EQ(std::vector<std::string>{},
            myContainer.range("l1", kMin, kMin));

  // trim() with INT64_MIN start: clamps start to 0, stop stays 0,
  // keeps the first element only.
  myContainer.trim("l1", kMin, 0);
  ASSERT_EQ(1u, myContainer.size("l1"));
  ASSERT_EQ("a", myContainer.index("l1", 0).value());

  // trim() with INT64_MIN stop: well-defined no-op in the current
  // implementation (stop is negative-and-larger-than-size →
  // early return without mutating). The point of this regression
  // test is "no UB"; the precise post-trim contents are an
  // unrelated semantic decision.
  ASSERT_EQ(2u, myContainer.pushBack("l1", {"b"}));
  ASSERT_EQ(3u, myContainer.pushBack("l1", {"c"}));
  const auto mySizeBefore = myContainer.size("l1");
  myContainer.trim("l1", 0, kMin);
  ASSERT_EQ(mySizeBefore, myContainer.size("l1"));
}
