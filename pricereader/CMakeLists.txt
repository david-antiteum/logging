cmake_minimum_required( VERSION 3.12 )
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
	set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project( pricereader )

find_package( yaml-cpp CONFIG REQUIRED )
find_package( cpprestsdk CONFIG REQUIRED )
find_package( nlohmann_json CONFIG REQUIRED )
find_package( OpenTracing CONFIG REQUIRED )
find_package( uriparser CONFIG REQUIRED )
find_package( fmt CONFIG REQUIRED )
find_package( spdlog CONFIG REQUIRED )
find_package( consulcpp CONFIG REQUIRED )
find_package( Boost COMPONENTS thread REQUIRED )

link_libraries( nlohmann_json cpprestsdk::cpprest OpenTracing::opentracing yaml-cpp consulcpp uriparser::uriparser fmt::fmt )

find_package( Thrift CONFIG )
find_package( jaegertracing CONFIG )
if( jaegertracing_FOUND )
	add_compile_definitions( JAEGER_ENABLED )
    link_libraries( jaegertracing::jaegertracing-static )
endif()

find_program( CLANG_TIDY NAMES "clang-tidy" )
if( CLANG_TIDY )
	set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,readability-*)
endif()

add_executable( ${PROJECT_NAME} main.cpp )
set_target_properties( ${PROJECT_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)

target_link_libraries(
    ${PROJECT_NAME}
    Boost::thread
)

set_property( TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17 )

include_directories( ../include )
