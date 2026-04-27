if(OKTOPLUS_WITH_GRPC)
  add_subdirectory(TestCommands)
endif()
add_subdirectory(TestConfigurations)
add_subdirectory(TestResp)
add_subdirectory(TestStorage)

add_executable (testunit
  ${CMAKE_SRC_DIR}/TestUnits/testsubunit.cpp
)

# TestCommands + protobuf link are gRPC-only. With OKTOPLUS_WITH_GRPC=OFF
# the unit-test binary mirrors the production binary's lean dep set.
set(testunit_LIBS
  testunit_testconfigurations
  testunit_testresp
  testunit_teststorage
)
if(OKTOPLUS_WITH_GRPC)
  list(APPEND testunit_LIBS testunit_testcommands)
endif()

target_link_libraries (testunit
  ${testunit_LIBS}

  ${GLOG}
  ${GTEST}
)
if(OKTOPLUS_WITH_GRPC)
  target_link_libraries (testunit ${PROTOBUF_LIBRARIES})
endif()
