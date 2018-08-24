
add_library(oktoplus_storage SHARED
  ${CMAKE_SRC_DIR}/Libraries/Storage/lists.cpp
  ${CMAKE_SRC_DIR}/Libraries/Storage/vectors.cpp
)

target_link_libraries(oktoplus_storage
 ${Boost_LIBRARIES}
)
