
add_library(oktoplus_storage SHARED
  ${CMAKE_SRC_DIR}/Libraries/Storage/sets.cpp
)

target_link_libraries(oktoplus_storage
 ${Boost_LIBRARIES}
 absl::flat_hash_map
 absl::flat_hash_set
)
