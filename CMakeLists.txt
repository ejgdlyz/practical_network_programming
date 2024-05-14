cmake_minimum_required(VERSION 3.0)
project(practical_network_programming)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(cmake/utils.cmake)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")

include_directories(.)

find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
endif()

# io 库文件
set(LIB_SRC
    sylar/address.cc
    sylar/bytearray.cc
    sylar/config.cc
    sylar/daemon.cc
    sylar/env.cc
    sylar/fd_manager.cc
    sylar/fiber.cc
    sylar/http/http.cc
    sylar/http/http_parser.cc
    sylar/http/http_session.cc
    sylar/http/http_connection.cc
    sylar/http/http_server.cc
    sylar/http/servlet.cc
    sylar/http/ws_session.cc
    sylar/http/ws_connection.cc
    sylar/http/ws_server.cc
    sylar/http/ws_servlet.cc
    sylar/hook.cc
    sylar/iomanager.cc
    sylar/library.cc
    sylar/log.cc
    sylar/module.cc
    sylar/mutex.cc
    sylar/scheduler.cc
    sylar/socket.cc
    sylar/stream.cc
    sylar/socket_stream.cc
    sylar/tcp_server.cc
    sylar/thread.cc
    sylar/timer.cc
    sylar/util.cc
    sylar/util/json_util.cc
    sylar/util/hash_util.cc
    sylar/worker.cc
    sylar/application.cc

    sylar/file_utils.cc
    sylar/timestamp.cc
    sylar/process_info.cc

    util/util.cc
    )

ragelmaker(sylar/http/http11_parser.rl LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/sylar/http)
ragelmaker(sylar/http/httpclient_parser.rl LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/sylar/http)
ragelmaker(sylar/uri.rl LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/sylar)

add_library(sylar SHARED ${LIB_SRC})
force_redefine_file_macro_for_sources(sylar)

find_library(YAMLCPP yaml-cpp)
find_library(PTHREAD pthread)
find_library(JSONCPP libjsoncpp)

set(LIBS
        sylar
        dl
        pthread
        yaml-cpp
        jsoncpp
        -lgd
        ${OPENSSL_LIBRARIES}
)

# 1 ttcp
find_package(Boost REQUIRED)
if(Boost_FOUND)
    # include_directories(${Boost_INCLUDE_DIRS})
    add_executable(ttcp_blocking 1_ttcp/main.cc 1_ttcp/ttcp_blocking.cc 1_ttcp/common.cc)
    add_dependencies(ttcp_blocking sylar)
    target_link_libraries(ttcp_blocking ${LIBS})
    target_link_libraries(ttcp_blocking boost_program_options)
    set_target_properties(ttcp_blocking PROPERTIES COMPILE_FLAGS "-Wno-error=old-style-cast -Wno-error=conversion")
endif()

add_executable(echo_client 1_ttcp/echo/echo_client.cc)
add_dependencies(echo_client sylar)
target_link_libraries(echo_client ${LIBS})
add_executable(echo_server 1_ttcp/echo/echo_server.cc)
add_dependencies(echo_server sylar)
target_link_libraries(echo_server ${LIBS})


# 2 roundtrip
add_executable(roundtrip_udp 2_roundtrip/roundtrip_udp.cc)
add_dependencies(roundtrip_udp sylar)
target_link_libraries(roundtrip_udp ${LIBS})

# 3 netstat
add_executable(nc_sender 3_netcat/nc_sender.cc)
add_dependencies(roundtrip_udp sylar)
target_link_libraries(nc_sender ${LIBS})
add_executable(nc_receiver 3_netcat/nc_receiver.cc)
add_dependencies(nc_receiver sylar)
target_link_libraries(nc_receiver ${LIBS})

add_executable(nodelay_server 3_netcat/nodelay_server.cc)
add_dependencies(nodelay_server sylar)
target_link_libraries(nodelay_server ${LIBS})
add_executable(nodelay_client 3_netcat/nodelay_client.cc)
add_dependencies(nodelay_client sylar)
target_link_libraries(nodelay_client ${LIBS})

add_executable(netcat 3_netcat/netcat.cc)
add_dependencies(netcat sylar)
target_link_libraries(netcat ${LIBS})

add_executable(chargen 3_netcat/chargen.cc)
add_dependencies(chargen sylar)
target_link_libraries(chargen ${LIBS})

# 4_procmon
user_add_executable(my_http_server "examples/my_http_server.cc" sylar "${LIBS}")
user_add_executable(procmon "4_procmon/procmon.cc;4_procmon/plot.cc" sylar "${LIBS}")
user_add_executable(dummyload "4_procmon/dummyload.cc" sylar "${LIBS}")
user_add_executable(plot_test "4_procmon/plot_test.cc;4_procmon/plot.cc" sylar "${LIBS}")


SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/tests)     # 可执行文件路径
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)          # 库输出路径
