add_library (testunit_testresp SHARED
  testresp_parser.cpp
  testresp_handler.cpp
  testresp_server.cpp
)

target_link_libraries (testunit_testresp
  oktoplus_resp
  oktoplus_storage

  ${GTEST}
  ${GLOG}
  ${Boost_LIBRARIES}
  ${PROTOBUF_LIBRARIES}
)

add_executable (testrespunit
  ${CMAKE_SRC_DIR}/TestUnits/testsubunit.cpp
)

target_link_libraries (testrespunit
  testunit_testresp
)
