cmake_minimum_required(VERSION 3.9)
project(simple C)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/simple.c)
add_definitions(-D_XOPEN_SOURCE=500) # usleep
target_link_libraries(${PROJECT_NAME} cleazy)
