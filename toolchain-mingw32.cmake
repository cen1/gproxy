# CMake toolchain file for cross-compiling from Linux to Windows (32-bit)
# using MinGW-w64

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

# Specify the cross compiler
set(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
set(CMAKE_RC_COMPILER i686-w64-mingw32-windres)

# Where to find libraries/includes in the target environment
set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)

# Adjust the default behavior of FIND_XXX() commands to:
# - Search headers and libraries in the target environment
# - Search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Set executable suffix
set(CMAKE_EXECUTABLE_SUFFIX ".exe")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".dll")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")

# Ensure proper flags for 32-bit compilation
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32" CACHE STRING "C flags for 32-bit")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32" CACHE STRING "CXX flags for 32-bit")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32" CACHE STRING "Shared linker flags for 32-bit")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32" CACHE STRING "Exe linker flags for 32-bit")
