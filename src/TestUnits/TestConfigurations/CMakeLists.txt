add_library (testunit_testconfigurations SHARED
  testconfiguration.cpp
)

target_link_libraries (testunit_testconfigurations
  oktoplus_configurations

  ${Boost_LIBRARIES}
  ${GTEST}
  ${GLOG}
  ${PROTOBUF_LIBRARIES}
)

add_executable (testconfigurationsunit
  ${CMAKE_SRC_DIR}/TestUnits/testsubunit.cpp
)

target_link_libraries (testconfigurationsunit
  testunit_testconfigurations
)
