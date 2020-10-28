cmake_minimum_required(VERSION 3.9)
project(cleazy C)

# Options
option(CLEAZY_PROFILE_SELF "Profile block allocation and insertion; \
Note this will throw off profiling the application itself" OFF)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)

# Try to compile release
set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Compiler flags
set(CLEAZY_COMMON_C_FLAGS "-Wall -Werror -Wextra -pedantic")
if(CLEAZY_PROFILE_SELF)
    set(CLEAZY_COMMON_C_FLAGS "${CLEAZY_COMMON_C_FLAGS} -DCLEAZY_PROFILE_SELF")
endif()
set(CMAKE_C_FLAGS_DEBUG          "${CMAKE_C_FLAGS_DEBUG}          ${CLEAZY_COMMON_C_FLAGS} -g3")
set(CMAKE_C_FLAGS_RELEASE        "${CMAKE_C_FLAGS_RELEASE}        ${CLEAZY_COMMON_C_FLAGS}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${CLEAZY_COMMON_C_FLAGS} -g3")
set(CMAKE_C_FLAGS_MINSIZEREL     "${CMAKE_C_FLAGS_MINSIZEREL}     ${CLEAZY_COMMON_C_FLAGS}")
# Required POSIX version for clock_gettime
add_definitions(-D_POSIX_C_SOURCE=199309L)

# Try to enable LTO
include(CheckIPOSupported)
check_ipo_supported(RESULT IS_IPO_SUPPORTED OUTPUT output)
if(IS_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Packages
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Executable, source dir
set(CLEAZY_SOURCES ${PROJECT_SOURCE_DIR}/src/cleazy.c)
add_library(${PROJECT_NAME} STATIC ${CLEAZY_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads)
