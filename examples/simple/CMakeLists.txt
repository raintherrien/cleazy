cmake_minimum_required(VERSION 3.9)
project(simple C)

option(CLEAZY_PROFILE "Build with cleazy profiling enabled" OFF)

if(CLEAZY_PROFILE)
    set(CMAKE_C_FLAGS_DEBUG          "${CMAKE_C_FLAGS_DEBUG}          -DCLEAZY_PROFILE")
    set(CMAKE_C_FLAGS_RELEASE        "${CMAKE_C_FLAGS_RELEASE}        -DCLEAZY_PROFILE")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -DCLEAZY_PROFILE")
    set(CMAKE_C_FLAGS_MINSIZEREL     "${CMAKE_C_FLAGS_MINSIZEREL}     -DCLEAZY_PROFILE")
endif()

add_subdirectory(${PROJECT_SOURCE_DIR}/../../ ${PROJECT_BINARY_DIR}/lib/cleazy)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/simple.c)
target_compile_definitions(${PROJECT_NAME} PRIVATE _XOPEN_SOURCE=700) # nanosleep
target_link_libraries(${PROJECT_NAME} cleazy)
