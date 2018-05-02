
# Includes path
include_directories (${CMAKE_SRC_DIR}/Libraries)
include_directories (${CMAKE_SRC_DIR}/Executables)
include_directories (${Boost_INCLUDE_DIRS})

add_executable (oktoplus
  ${CMAKE_SRC_DIR}/Executables/Oktoplus/main.cpp
)

target_link_libraries(oktoplus
  oktoplus_storage
  oktoplus_commands
  ${GLOG}
  ${Boost_LIBRARIES}
)

