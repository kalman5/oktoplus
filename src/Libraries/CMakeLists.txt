include_directories(
  ${CMAKE_SRC_DIR}/../externals/gtest/googlemock
  ${CMAKE_SRC_DIR}/../externals/gtest/googlemock/include
  ${CMAKE_SRC_DIR}/../externals/gtest/googletest
  ${CMAKE_SRC_DIR}/../externals/gtest/googletest/include
)

add_library (gmock_gtest SHARED
  ${CMAKE_SRC_DIR}/../externals/gtest/googlemock/src/gmock-all.cc
  ${CMAKE_SRC_DIR}/../externals/gtest/googletest/src/gtest-all.cc
)


add_subdirectory(Commands)
add_subdirectory(Configurations)
add_subdirectory(Storage)

