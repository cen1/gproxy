cmake_minimum_required(VERSION 3.14)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    cmake_policy(SET CMP0167 NEW)
endif()
project(gproxy-root LANGUAGES CXX)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Modules")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS ON)

# Check for XPAM_EDITION environment variable
if(DEFINED ENV{XPAM_EDITION} AND "$ENV{XPAM_EDITION}" STREQUAL "1")
    set(XPAM_EDITION TRUE)
else()
    set(XPAM_EDITION FALSE)
endif()

# Add subdirectories
add_subdirectory(common)

# Custom GPS handshake dll
if (XPAM_EDITION AND EXISTS "${CMAKE_SOURCE_DIR}/handshake-xpam")
    add_subdirectory(handshake-xpam)
else()
    add_subdirectory(handshake)
endif()

# Only include addons if XPAM_EDITION is enabled
if(XPAM_EDITION AND EXISTS "${CMAKE_SOURCE_DIR}/addons")
    add_subdirectory(addons)
    message(STATUS "XPAM_EDITION enabled - including addons")
else()
    message(STATUS "XPAM_EDITION disabled - building without addons")
endif()

add_subdirectory(gproxy)