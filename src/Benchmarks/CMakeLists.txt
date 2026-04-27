include_directories (${CMAKE_SRC_DIR}/Benchmarks)

# Both micro-benchmark binaries are gRPC clients (they call oktoplus
# through the protobuf API). Skip them when gRPC is disabled.
if(OKTOPLUS_WITH_GRPC)
  add_subdirectory(Inserts)
  add_subdirectory(Reads)
endif()