
add_library(oktoplus_resp SHARED
  ${CMAKE_SRC_DIR}/Libraries/Resp/resp_parser.cpp
  ${CMAKE_SRC_DIR}/Libraries/Resp/resp_handler.cpp
  ${CMAKE_SRC_DIR}/Libraries/Resp/resp_server.cpp
)

target_link_libraries(oktoplus_resp
  oktoplus_storage
  ${GLOG}
  ${Boost_LIBRARIES}
)
