cmake_minimum_required(VERSION 3.10)
project(
  ModernCMakeExample
  VERSION 1.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(tcp SHARED
  src/tcp.cpp
  src/tcp_pcb.cpp
)
target_link_libraries(tcp ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(tcp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(exchange src/exchange.cpp src/application.cpp src/market.cpp)
target_link_libraries(exchange ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(exchange tcp)
target_include_directories(exchange PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(client src/client.cpp)
target_link_libraries(client ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(client tcp)
target_include_directories(client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(test1 tests/tcp_module_init.cpp src/market.cpp)
target_link_libraries(test1 ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(test1 tcp)
target_include_directories(test1 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(test2 tests/tcp_module_server.cpp src/market.cpp)
target_link_libraries(test2 ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(test2 tcp)
target_include_directories(test2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
