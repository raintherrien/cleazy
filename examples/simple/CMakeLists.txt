cmake_minimum_required(VERSION 3.9)
project(simple C)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/simple.c)
target_compile_definitions(${PROJECT_NAME} PRIVATE _XOPEN_SOURCE=700) # nanosleep
target_link_libraries(${PROJECT_NAME} cleazy)
