
add_executable (inserts
  ${CMAKE_SRC_DIR}/Benchmarks/Inserts/main.cpp
)

target_link_libraries(inserts
  oktoplus_commands
  ${Boost_LIBRARIES}
  ${PROTOBUF_LIBRARIES}
)

