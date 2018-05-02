# Includes path
include_directories (${CMAKE_SRC_DIR}/Libraries)

add_subdirectory(TestCommands)
add_subdirectory(TestStorage)

add_executable (testunit
  ${CMAKE_SRC_DIR}/TestUnits/testunit.cpp
)

set(testunit_LIBS
  testunit_testcommands
  testunit_teststorage
)

target_link_libraries (testunit
  ${testunit_LIBS}

  ${GLOG}
  ${GTEST}
  ${PROTOBUF_LIBRARIES}
)
