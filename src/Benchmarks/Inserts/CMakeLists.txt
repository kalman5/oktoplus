
# Includes path
include_directories (${CMAKE_SRC_DIR}/Libraries)
include_directories (${CMAKE_SRC_DIR}/Benchmarks)
include_directories (${Boost_INCLUDE_DIRS})

add_executable (inserts
  ${CMAKE_SRC_DIR}/Benchmarks/Inserts/main.cpp
)

target_link_libraries(inserts
  oktoplus_commands
  ${Boost_LIBRARIES}
  ${PROTOBUF_LIBRARIES}
)

