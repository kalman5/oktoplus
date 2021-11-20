add_library (testunit_teststorage SHARED
  testlists.cpp
  testsets.cpp
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
