
include_directories (${CMAKE_SRC_DIR}/Libraries)

add_library (testunit_teststorage SHARED
  ${CMAKE_SRC_DIR}/TestUnits/TestStorage/testlists.cpp
)

target_link_libraries (testunit_teststorage
  oktoplus_storage

  ${Boost_LIBRARIES}
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
