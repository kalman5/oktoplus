add_subdirectory(TestCommands)
add_subdirectory(TestConfigurations)
add_subdirectory(TestStorage)

add_executable (testunit
  ${CMAKE_SRC_DIR}/TestUnits/testsubunit.cpp
)

set(testunit_LIBS
  testunit_testcommands
  testunit_testconfigurations
  testunit_teststorage
)

target_link_libraries (testunit
  ${testunit_LIBS}

  ${GLOG}
  ${GTEST}
  ${PROTOBUF_LIBRARIES}
)
