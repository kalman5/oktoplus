#include "Storage/okt_string.h"

#include "gtest/gtest.h"

#include <string>
#include <string_view>
#include <utility>

namespace okts::stor {

TEST(TestOktString, layout_is_sixteen_bytes) {
  // The whole point of okt_string is being half the size of
  // libstdc++'s std::string. Lock the size in.
  EXPECT_EQ(sizeof(okt_string), std::size_t{16});
}

TEST(TestOktString, default_constructed_is_empty) {
  okt_string s;
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);
  EXPECT_EQ(std::string_view(s), "");
}

TEST(TestOktString, sso_round_trip_at_boundary) {
  // 15 bytes is the inline cap. 16+ must spill to heap.
  for (std::size_t myLen : {std::size_t{0}, std::size_t{1}, std::size_t{14},
                            std::size_t{15}, std::size_t{16}, std::size_t{17},
                            std::size_t{255}, std::size_t{1024}}) {
    std::string myExpected(myLen, 'a');
    okt_string  myStr(myExpected);
    EXPECT_EQ(myStr.size(), myLen);
    EXPECT_EQ(std::string_view(myStr), std::string_view(myExpected));
  }
}

TEST(TestOktString, copy_preserves_content_and_independence) {
  // Heap-mode string -- copy must allocate a new buffer.
  okt_string mySrc(std::string(64, 'x'));
  okt_string myCopy(mySrc);
  EXPECT_EQ(std::string_view(mySrc),  std::string(64, 'x'));
  EXPECT_EQ(std::string_view(myCopy), std::string(64, 'x'));
  // Modifying mySrc (via reassignment) must not corrupt myCopy.
  mySrc = okt_string("short");
  EXPECT_EQ(std::string_view(mySrc),  "short");
  EXPECT_EQ(std::string_view(myCopy), std::string(64, 'x'));
}

TEST(TestOktString, move_steals_heap_and_empties_source) {
  okt_string mySrc(std::string(100, 'q'));
  okt_string myDst(std::move(mySrc));
  EXPECT_EQ(myDst.size(), 100u);
  EXPECT_EQ(std::string_view(myDst), std::string(100, 'q'));
  // Moved-from string is reset to empty (NOT just "valid but
  // unspecified") -- our pop paths rely on this so the source slot
  // can be reused without re-clearing.
  EXPECT_TRUE(mySrc.empty());
  EXPECT_EQ(mySrc.size(), 0u);
}

TEST(TestOktString, move_assignment_releases_existing_extent) {
  okt_string myDst(std::string(80, 'a'));
  okt_string mySrc(std::string(120, 'b'));
  myDst = std::move(mySrc);
  EXPECT_EQ(myDst.size(), 120u);
  EXPECT_EQ(std::string_view(myDst), std::string(120, 'b'));
  EXPECT_TRUE(mySrc.empty());
  // Round-trip through SSO and back to verify the dtor + assign
  // path doesn't double-free (caught by ASAN if it did).
  myDst = okt_string("inline");
  myDst = okt_string(std::string(50, 'c'));
}

TEST(TestOktString, equality_with_each_supported_type) {
  okt_string myStr("hello");
  EXPECT_TRUE(myStr  == std::string_view("hello"));
  EXPECT_TRUE("hello" == std::string_view(myStr));
  EXPECT_TRUE(myStr  == okt_string("hello"));
  EXPECT_TRUE(myStr  == std::string("hello"));
  EXPECT_FALSE(myStr == std::string_view("world"));
}

TEST(TestOktString, to_string_returns_owning_copy) {
  okt_string myStr(std::string(32, 'z'));
  std::string myCopy = myStr.toString();
  EXPECT_EQ(myCopy, std::string(32, 'z'));
  EXPECT_EQ(myStr.size(), 32u); // original is unchanged
}

TEST(TestOktString, self_assign_is_safe) {
  okt_string myStr(std::string(40, 'k'));
  okt_string& myRef = myStr;
  myStr = myRef;            // copy self
  EXPECT_EQ(std::string_view(myStr), std::string(40, 'k'));
  myStr = std::move(myRef); // move self -- must not free our own buffer
  EXPECT_EQ(std::string_view(myStr), std::string(40, 'k'));
}

} // namespace okts::stor
