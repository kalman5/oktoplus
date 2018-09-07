
include_directories (${CMAKE_SRC_DIR}/Libraries)

add_library (testunit_testcommands SHARED
  ${CMAKE_SRC_DIR}/TestUnits/TestCommands/testcommands_server.cpp
)

target_link_libraries (testunit_testcommands
  oktoplus_commands

  ${Boost_LIBRARIES}
  ${GTEST}
  ${GLOG}
  ${PROTOBUF_LIBRARIES}
)

add_executable (testcommandsunit
  ${CMAKE_SRC_DIR}/TestUnits/testsubunit.cpp
)

target_link_libraries (testcommandsunit
  testunit_testcommands
)
