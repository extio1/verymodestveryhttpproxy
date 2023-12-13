cmake_minimum_required(VERSION 3.12)

project(os_proxy)

file(GLOB SOURCES CONFIGURE_DEPENDS "./src/*")
file(GLOB HEADERS CONFIGURE_DEPENDS "./include/*")

add_executable(proxy ${SOURCES} ${HEADERS})
target_link_libraries(proxy http_parser) # LD_LIBRARY_PATH=/usr/local/lib
target_compile_options(proxy PRIVATE -Wall -g)
