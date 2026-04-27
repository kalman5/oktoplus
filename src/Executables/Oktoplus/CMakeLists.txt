
# Includes path
include_directories (${CMAKE_SRC_DIR}/Libraries)
include_directories (${CMAKE_SRC_DIR}/Executables)
include_directories (${Boost_INCLUDE_DIRS})

add_executable (oktoplus
  ${CMAKE_SRC_DIR}/Executables/Oktoplus/main.cpp
)

target_link_libraries(oktoplus
  oktoplus_configurations
  oktoplus_resp
  oktoplus_storage
  ${GLOG}
  ${Boost_LIBRARIES}
  absl::flat_hash_set
)

# gRPC-only deps. With OKTOPLUS_WITH_GRPC=OFF the binary skips
# linking against libgrpc / libprotobuf and their dependency closure
# (libcrypto / libicu / libabsl-flow / libupb / ...), trimming the
# baseline RSS that those .so text/rodata mappings would otherwise
# pin into every running oktoplus process.
if(OKTOPLUS_WITH_GRPC)
  target_link_libraries(oktoplus
    oktoplus_commands
    ${PROTOBUF_LIBRARIES}
  )
endif()

# Wire jemalloc as the executable's malloc implementation when
# requested. Linking the .so before libc means jemalloc's exported
# malloc/free/realloc symbols win the dynamic-linker race.
if(OKTOPLUS_WITH_JEMALLOC AND JEMALLOC)
  target_link_libraries(oktoplus ${JEMALLOC})
endif()

