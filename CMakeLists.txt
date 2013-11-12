cmake_minimum_required(VERSION 2.8)

project(SudaGureum)

include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)

if(CMAKE_COMPILER_IS_GNUCXX)
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
else(MSVC)
	set(FLAG "/EHsc")
endif(CMAKE_COMPILER_IS_GNUCXX)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/Bin)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/Bin)

set(CMAKE_C_FLAGS "${FLAG}")
set(CMAKE_CXX_FLAGS "${FLAG}")

file(GLOB_RECURSE SOURCES "SudaGureum/*.cpp")

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.51.0 COMPONENTS thread filesystem date_time)
include_directories(${Boost_INCLUDE_DIR} ".")
link_directories(${Boost_LIBRARY_DIR})

add_executable(SudaGureum ${SOURCES})
target_link_libraries(SudaGureum ${Boost_LIBRARIES})
