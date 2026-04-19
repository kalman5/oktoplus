
# Includes path
include_directories (${CMAKE_SRC_DIR}/Libraries)
include_directories (${CMAKE_SRC_DIR}/Executables)
include_directories (${Boost_INCLUDE_DIRS})

add_executable (oktoplus
  ${CMAKE_SRC_DIR}/Executables/Oktoplus/main.cpp
)

target_link_libraries(oktoplus
  oktoplus_commands
  oktoplus_configurations
  oktoplus_resp
  oktoplus_storage
  ${GLOG}
  ${Boost_LIBRARIES}
  ${PROTOBUF_LIBRARIES}
  absl::flat_hash_set
)

# Wire jemalloc as the executable's malloc implementation when
# requested. Linking the .so before libc means jemalloc's exported
# malloc/free/realloc symbols win the dynamic-linker race.
if(OKTOPLUS_WITH_JEMALLOC AND JEMALLOC)
  target_link_libraries(oktoplus ${JEMALLOC})
endif()

