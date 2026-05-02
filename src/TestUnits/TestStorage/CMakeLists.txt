add_library (testunit_teststorage SHARED
  testokt_string.cpp
  testsequencecontainer.cpp
  testsets.cpp
  teststorageconcurrency.cpp
)

target_link_libraries (testunit_teststorage
  oktoplus_storage

  ${GTEST}
  ${GLOG}
  ${PROTOBUF_LIBRARIES}
)

add_executable (teststorageunit
  ${CMAKE_SRC_DIR}/TestUnits/testsubunit.cpp
)

target_link_libraries (teststorageunit
  testunit_teststorage
)
