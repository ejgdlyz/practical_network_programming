cmake_minimum_required(VERSION 3.0)
project(practical_network_programming)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")

include_directories(.)

set (LIB_SRC
    util/address.cc
    util/socket.cc
    util/util.cc
)

add_library(lambda SHARED ${LIB_SRC})

set(LIBS
    lambda
    pthread
)

# 1 ttcp
find_package(Boost REQUIRED)
if(Boost_FOUND)
    # include_directories(${Boost_INCLUDE_DIRS})
    add_executable(ttcp_blocking 1_ttcp/main.cc 1_ttcp/ttcp_blocking.cc 1_ttcp/common.cc)
    add_dependencies(ttcp_blocking lambda)
    target_link_libraries(ttcp_blocking ${LIBS})
    target_link_libraries(ttcp_blocking boost_program_options)
    set_target_properties(ttcp_blocking PROPERTIES COMPILE_FLAGS "-Wno-error=old-style-cast -Wno-error=conversion")
endif()

add_executable(echo_client 1_ttcp/echo/echo_client.cc)
add_dependencies(echo_client lambda)
target_link_libraries(echo_client ${LIBS})
add_executable(echo_server 1_ttcp/echo/echo_server.cc)
add_dependencies(echo_server lambda)
target_link_libraries(echo_server ${LIBS})


# 2 roundtrip
add_executable(roundtrip_udp 2_roundtrip/roundtrip_udp.cc)
add_dependencies(roundtrip_udp lambda)
target_link_libraries(roundtrip_udp ${LIBS})

# 3 netstat
add_executable(nc_sender 3_netcat/nc_sender.cc)
add_dependencies(roundtrip_udp lambda)
target_link_libraries(nc_sender ${LIBS})
add_executable(nc_receiver 3_netcat/nc_receiver.cc)
add_dependencies(nc_receiver lambda)
target_link_libraries(nc_receiver ${LIBS})

add_executable(nodelay_server 3_netcat/nodelay_server.cc)
add_dependencies(nodelay_server lambda)
target_link_libraries(nodelay_server ${LIBS})
add_executable(nodelay_client 3_netcat/nodelay_client.cc)
add_dependencies(nodelay_client lambda)
target_link_libraries(nodelay_client ${LIBS})


SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/tests)     # 可执行文件路径
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)          # 库输出路径
