cmake_minimum_required(VERSION 2.8)

project(SudaGureum)

include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMakeModule")

if(MSVC)
    set(FLAG "/EHsc")
else(MSVC)
    set(FLAG_DEBUG "-g3 -O0")
    set(FLAG_RELEASE "-O2")

    CHECK_CXX_COMPILER_FLAG(-std=c++11 HAVE_CXX11)
    if(HAVE_CXX11)
        set(FLAG "${FLAG} -std=c++11")
    else(HAVE_CXX11)
        CHECK_CXX_COMPILER_FLAG(-std=c++0x HAVE_CXX0X)
        if(HAVE_CXX0X)
            set(FLAG "${FLAG} -std=c++0x")
        else(HAVE_CXX0X)
            message(FATAL_ERROR "No C++11 support found")
        endif(HAVE_CXX0X)
    endif(HAVE_CXX11)

    set(FLAG "${FLAG} -pedantic -W -Wall -pipe")

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(FLAG "${FLAG} ${FLAG_DEBUG}")
    else(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(FLAG "${FLAG} ${FLAGS_RELEASE}")
    endif(CMAKE_BUILD_TYPE STREQUAL "Debug")
endif(MSVC)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/Bin)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/Bin)

set(CMAKE_C_FLAGS "${FLAG}")
set(CMAKE_CXX_FLAGS "${FLAG}")

file(GLOB_RECURSE SOURCES "SudaGureum/*.cpp")

find_package(Boost 1.51.0 COMPONENTS date_time filesystem locale program_options regex system thread REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
include_directories(${Boost_INCLUDE_DIR} ${OpenSSL_INCLUDE_DIR} ${SQLite3_INCLUDE_DIR}
    "${CMAKE_CURRENT_LIST_DIR}/External/SubmoduleLib" "${CMAKE_CURRENT_LIST_DIR}/External/SubmoduleLib/rapidjson/include" "${CMAKE_CURRENT_LIST_DIR}/External/SubmoduleLib/SQLiteCpp/include"
    "${CMAKE_CURRENT_LIST_DIR}")
link_directories(${Boost_LIBRARY_DIR} ${OpenSSL_LIBRARY_DIR} ${SQLite3_LIBRARY_DIR})

add_executable(SudaGureum ${SOURCES})
target_link_libraries(SudaGureum ${Boost_LIBRARIES} ${OpenSSL_LIBRARIES} ${SQLite3_LIBRARIES})
