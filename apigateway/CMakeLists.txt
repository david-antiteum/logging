cmake_minimum_required( VERSION 3.12 )
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
	set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project( apigateway )

find_package( yaml-cpp CONFIG REQUIRED )
find_package( cpprestsdk CONFIG REQUIRED )
find_package( opentracing )
find_package( jaegertracing )

link_libraries( cpprestsdk::cpprest)

add_executable( ${PROJECT_NAME} main.cpp )
set_target_properties( ${PROJECT_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)

set_property( TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17 )

include_directories( ../include )
