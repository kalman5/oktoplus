set(CMAKE_SRC_DIR "${Oktoplus_SOURCE_DIR}/src")

#include path for protobuf generated headers
include_directories(${CMAKE_CURRENT_BINARY_DIR}/Libraries)


# Libraries
find_library(GLOG     glog)
#find_library(CRYPTO++ crypto++)
find_library(JSONCPP    jsoncpp)   # This will permit to store jsons
#find_library(FFTW3      fftw3f)    # This will permit eventually to perform FFT operations on series

#find_path(FFTW3_INCLUDE_PATH fftw3.h)
#INCLUDE_DIRECTORIES(${FFTW3_INCLUDE_PATH})

list(APPEND CMAKE_MODULE_PATH "${Oktoplus_SOURCE_DIR}/cmake")

# GMOCK/GTEST
set(GTEST gmock_gtest)

include_directories(${Oktoplus_SOURCE_DIR}/externals/gtest/googlemock/include)
include_directories(${Oktoplus_SOURCE_DIR}/externals/gtest/googletest/include)

# Boost libraries
set(Boost_USE_STATIC_LIBS FALSE)
set(Boost_MULTITHREADED TRUE)
find_package(Boost 1.55.0 REQUIRED COMPONENTS regex date_time filesystem program_options system signals thread chrono)
message("***** Boost Libraries *****")
message("${Boost_LIBRARIES}")
message("***************************")
link_directories(${Boost_LIBRARY_DIR})
include_directories(${Boost_INCLUDE_DIR})

# Protobuf libraries
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS})
find_package(GRPC REQUIRED)

# Compiler flags
if(${CMAKE_CXX_COMPILER} MATCHES "(.*)clang(.*)")
  message("\nSPECIFIC WARNINGS FOR CLANG")
  SET(CLANG_SPECIFIC_WARNINGS "-Wthread-safety")
else()
  message("\nSPECIFIC WARNINGS FOR GCC")
  #SET(PRJ_NO_WARNINGS "-Wno-overloaded-virtual -Wno-deprecated-declarations -Wno-vla -Wno-vla-extension -Wno-long-long -Wno-maybe-uninitialized -Wno-unused-local-typedefs")
endif()

#set (CMAKE_CXX_STANDARD 17) # c++17 (old version of cmake does not supports c++17

set(PRJ_COMMONS "-Wall -Wextra -Wpedantic -Werror -pthread -fPIC -fno-strict-aliasing ${CLANG_SPECIFIC_WARNINGS} -Wcast-qual -std=c++17")

set(CMAKE_CXX_FLAGS_DEBUG     "-O0 -g3 ${PRJ_COMMONS}"                                    )
set(CMAKE_CXX_FLAGS_OPTIMIZED "-O3 -g3 ${PRJ_COMMONS} -DNDEBUG -funroll-loops -ffast-math")

if(NOT APPLE)
  set(CMAKE_EXE_LINKER_FLAGS    "-Wl,--no-as-needed")
endif()
#################################################################

# summary
message("")
message("")
message("=================== SUMMARY ======================")
message("CMAKE_SOURCE_DIR:        ${CMAKE_SRC_DIR}"         )
message("COMPILER:                ${CMAKE_CXX_COMPILER}"    )
message("CMAKE_BUILD_TYPE:        ${CMAKE_BUILD_TYPE}"      )
message("Boost Libraries Path:    ${Boost_LIBRARY_DIRS}"    )
message("Boost Includes Path:     ${Boost_INCLUDE_DIRS}"    )
message("Boost version is:        ${Boost_VERSION}"         )
message("Protobuf Library:        ${PROTOBUF_LIBRARIES}"    )
message("Protobuf Includes Path:  ${PROTOBUF_INCLUDE_DIRS}" )
message("Grpc Library:            ${GRPC_LIBRARIES}"        )
message("==================================================")
message("")
message("")
#################################################################

include_directories (${CMAKE_SRC_DIR}/Libraries)

add_subdirectory(Libraries)
add_subdirectory(Executables)
add_subdirectory(TestUnits)

