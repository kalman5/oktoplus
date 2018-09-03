
add_library(oktoplus_configurations SHARED
  ${CMAKE_SRC_DIR}/Libraries/Configurations/defaultconfiguration.cpp
  ${CMAKE_SRC_DIR}/Libraries/Configurations/jsonconfiguration.cpp
  ${CMAKE_SRC_DIR}/Libraries/Configurations/mainconfiguration.cpp
)

target_link_libraries(oktoplus_configurations
 ${Boost_LIBRARIES}
 jsoncpp
)
