cmake_minimum_required( VERSION 3.12 )
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
	set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

add_compile_definitions( _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS )

project( Logging-Meeting )

find_program( CLANG_TIDY NAMES "clang-tidy" )
if( CLANG_TIDY )
	set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY}" -checks=readability-*,cppcoreguidelines-*)
endif()

add_subdirectory( apigateway )
add_subdirectory( forecaster )
add_subdirectory( pricereader )
