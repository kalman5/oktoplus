
add_executable (reads
  ${CMAKE_SRC_DIR}/Benchmarks/Reads/main.cpp
)

target_link_libraries(reads
  oktoplus_commands
  ${Boost_LIBRARIES}
  ${PROTOBUF_LIBRARIES}
)

