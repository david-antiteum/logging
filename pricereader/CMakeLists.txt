cmake_minimum_required( VERSION 3.10 )

project( pricereader )
add_executable( ${PROJECT_NAME} main.cpp )
set_target_properties( ${PROJECT_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)

set_property( TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17 )

include_directories( ../include )

LINK_DIRECTORIES( /usr/local/lib )

target_link_libraries( ${PROJECT_NAME} 
	yaml-cpp
	cpprest
	pthread
	boost_system
	crypto
	jaegertracing
	opentracing
)
