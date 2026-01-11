cmake_minimum_required(VERSION 3.14)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    cmake_policy(SET CMP0167 NEW)
endif()

project(handshake VERSION 1.0.0)

add_library(handshake SHARED)

set(SOURCES
    handshake.cpp
    handshake.h
)

target_sources(handshake PRIVATE ${SOURCES})

# Set include directories for consumers
target_include_directories(handshake PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/handshake>
)

set_target_properties(handshake PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(handshake PROPERTIES SOVERSION 1)
set_target_properties(handshake PROPERTIES OUTPUT_NAME gproxy_handshake)
target_compile_features(handshake PRIVATE cxx_nullptr)

# Export symbols on Windows
if(WIN32)
    target_compile_definitions(handshake PRIVATE HANDSHAKE_EXPORTS)
endif()

# Create an alias target for namespaced usage
add_library(gproxy::handshake ALIAS handshake)

# Install the library
install(TARGETS handshake
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install headers
install(FILES handshake.h DESTINATION include/handshake)
