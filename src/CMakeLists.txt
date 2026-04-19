set(CMAKE_SRC_DIR "${Oktoplus_SOURCE_DIR}/src")

#include path for protobuf generated headers
include_directories(${CMAKE_CURRENT_BINARY_DIR}/Libraries)

# Libraries
find_library(GLOG     glog)
find_library(JSONCPP  jsoncpp)

# jemalloc replaces glibc malloc transparently when linked into the
# executable. Big wins on alloc-heavy paths (LRANGE_100, RPUSH random
# key) — see benchmark_results/PERF_TODO.md item H. Optional: skip the
# link by passing -DOKTOPLUS_WITH_JEMALLOC=OFF to cmake.
option(OKTOPLUS_WITH_JEMALLOC "Link the oktoplus binary against jemalloc" ON)
if(OKTOPLUS_WITH_JEMALLOC)
  # Some distros ship only the runtime .so without the unversioned
  # symlink (jemalloc-devel provides it). Search for both names.
  find_library(JEMALLOC NAMES jemalloc libjemalloc.so.2)
  if(NOT JEMALLOC)
    message(WARNING "jemalloc requested but not found; falling back to glibc malloc")
    set(OKTOPLUS_WITH_JEMALLOC OFF)
  else()
    message("Linking oktoplus binary with jemalloc: ${JEMALLOC}")
  endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${Oktoplus_SOURCE_DIR}/cmake")

# GMOCK/GTEST (system)
find_package(GTest REQUIRED)
set(GTEST GTest::gtest GTest::gmock)

# ABSEIL (system)
find_package(absl REQUIRED)

# Boost libraries
set(Boost_USE_STATIC_LIBS FALSE)
set(Boost_MULTITHREADED TRUE)
find_package(Boost 1.68.0 REQUIRED COMPONENTS regex date_time filesystem program_options system thread chrono)
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
endif()

# c++20
set (CMAKE_CXX_STANDARD 20)

set(PRJ_COMMONS "-Wall -Wextra -Wpedantic -Werror -pthread -fPIC -fno-strict-aliasing ${CLANG_SPECIFIC_WARNINGS} -Wcast-qual")

#enable thread safety analysis
add_compile_definitions(BOOST_THREAD_ENABLE_THREAD_SAFETY_ANALYSIS)

set(CMAKE_CXX_FLAGS_DEBUG     "-O0 -g3 ${PRJ_COMMONS}"                                    )
set(CMAKE_CXX_FLAGS_CLANG     "${CMAKE_CXX_FLAGS_DEBUG}"                                  )
set(CMAKE_CXX_FLAGS_OPTIMIZED "-O3 -g3 ${PRJ_COMMONS} -DNDEBUG -funroll-loops -ffast-math -march=native -mtune=native -fno-semantic-interposition")

set(CMAKE_EXE_LINKER_FLAGS "-Wl,--no-as-needed")

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
include_directories (${Boost_INCLUDE_DIRS})

add_subdirectory(Libraries)
add_subdirectory(Executables)
add_subdirectory(Benchmarks)
add_subdirectory(TestUnits)

