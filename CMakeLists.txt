cmake_minimum_required(VERSION 3.0.0)
project(SDFSS VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
enable_testing()

## load in pkg-config support
find_package(PkgConfig)
## use pkg-config to get hints for 0mq locations
pkg_check_modules(PC_ZeroMQ QUIET zmq)

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

find_package(jsoncpp REQUIRED)
message("FOUND CONFIG: ${jsoncpp_CONFIG}")
message("INCLUDE DIRS: ${jsoncpp_INCLUDE_DIRS}")
message("LIBRARIES: ${jsoncpp_LIBRARIES}")

# Include jsoncpp headers
include_directories("${JSONCPP_INCLUDE_DIRS}")
#add_executable(${PROJECT_NAME} main.cpp node_filesystem.cpp node.cpp)

#TESTING
add_executable(node_test_one nodestest.cpp node_filesystem.cpp node.cpp)
#add_executable(server_test_one zmqtestone.cpp)
#add_executable(server_test_two zmqtesttwo.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

#target_link_libraries(${PROJECT_NAME} ${ZeroMQ_LIBRARY} fmt::fmt)

#TESTING
target_link_libraries(node_test_one ${ZeroMQ_LIBRARY} fmt::fmt ${JSONCPP_LIBRARIES})
#target_link_libraries(${PROJECT_NAME} torrent-rasterbar ${ZeroMQ_LIBRARY})
#target_link_libraries(server_test_one ${ZeroMQ_LIBRARY} fmt::fmt)
#target_link_libraries(server_test_two ${ZeroMQ_LIBRARY} fmt::fmt)