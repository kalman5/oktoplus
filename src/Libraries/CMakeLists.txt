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

####################################################################
add_compile_options("-Wno-shadow")
add_compile_options("-Wno-shadow-uncaptured-local")
add_compile_options("-Wno-tautological-type-limit-compare")
#add_compile_options("-Wno-tautological-value-range-compare")
add_compile_options("-Wno-unused-template")
add_compile_options("-Wno-pedantic")
#add_compile_options("-Wno-duplicated-branches")
add_compile_options("-Wno-null-dereference")
add_compile_options("-Wno-anon-enum-enum-conversion")

add_subdirectory(${Oktoplus_SOURCE_DIR}/externals/abseil-cpp ${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp)
####################################################################

