cmake_minimum_required(VERSION 3.0)
project(practical_network_programming)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")

include_directories(.)

# 1 ttcp
find_package(Boost REQUIRED)
if(Boost_FOUND)
    # include_directories(${Boost_INCLUDE_DIRS})
    add_executable(ttcp_blocking 1_ttcp/main.cc 1_ttcp/ttcp_blocking.cc 1_ttcp/common.cc)
    target_link_libraries(ttcp_blocking boost_program_options)
    set_target_properties(ttcp_blocking PROPERTIES COMPILE_FLAGS "-Wno-error=old-style-cast -Wno-error=conversion")
endif()

add_executable(echo_client 1_ttcp/echo/echo_client.cc 1_ttcp/echo/tool.cc)
add_executable(echo_server 1_ttcp/echo/echo_server.cc 1_ttcp/echo/tool.cc)



# 2 roundtrip
add_executable(roundtrip_udp 2_roundtrip/roundtrip_udp.cc)
target_link_libraries(roundtrip_udp pthread)


SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/tests)   # 可执行文件路径