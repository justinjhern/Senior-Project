cmake_minimum_required(VERSION 3.0.0)
project(SDFSS VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
enable_testing()

## load in pkg-config support
find_package(PkgConfig)
## use pkg-config to get hints for ZeroMQ locations
pkg_check_modules(PC_ZeroMQ QUIET zmq)

## use pkg-config to get hints for JSONCPP locations
pkg_check_modules(JSONCPP jsoncpp)
link_libraries(${JSONCPP_LIBRARIES})

## use the hint from above to find where 'zmq.hpp' is located
find_path(ZeroMQ_INCLUDE_DIR
        NAMES zmq.hpp
        PATHS ${PC_ZeroMQ_INCLUDE_DIRS}
        )

## use the hint from above to find the location of libzmq
find_library(ZeroMQ_LIBRARY
        NAMES zmq
        PATHS ${PC_ZeroMQ_LIBRARY_DIRS}
        )

find_package(fmt REQUIRED)  # Search for fmt package
find_package(jsoncpp REQUIRED)  # Search for jsoncpp package

# Include JSONCPP headers
include_directories(
    "${JSONCPP_INCLUDE_DIRS}"
    "/usr/include/jsoncpp/json"
)

add_executable(config_creator jsoncreator.cpp)
add_executable(SDFSS main.cpp node_filesystem.cpp node.cpp )

add_executable(test1 maintest1.cpp node_filesystem.cpp node.cpp )
add_executable(test2 maintest2.cpp node_filesystem.cpp node.cpp )
add_executable(test3 maintest3.cpp node_filesystem.cpp node.cpp )


target_link_libraries(SDFSS ${ZeroMQ_LIBRARY} fmt::fmt ${JSONCPP_LIBRARIES})
target_link_libraries(config_creator ${JSONCPP_LIBRARIES})

target_link_libraries(test1 ${ZeroMQ_LIBRARY} fmt::fmt ${JSONCPP_LIBRARIES})
target_link_libraries(test2 ${ZeroMQ_LIBRARY} fmt::fmt ${JSONCPP_LIBRARIES})
target_link_libraries(test3 ${ZeroMQ_LIBRARY} fmt::fmt ${JSONCPP_LIBRARIES})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION}) 
include(CPack)