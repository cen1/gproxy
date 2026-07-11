cmake_minimum_required(VERSION 3.14)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    cmake_policy(SET CMP0167 NEW)
endif()

project(gproxy VERSION 1.14.0)

add_executable(gproxy)

find_package(Boost "1.86" COMPONENTS filesystem system thread REQUIRED)

set(SOURCES
    argagg.h
    bnetchat.h
    bytearray.h
    cmdbuffer.cpp
    cmdbuffer.h
    commandpacket.cpp
    commandpacket.h
    gameprotocol.cpp
    gameprotocol.h
    gproxy.cpp
    gproxy.h
    gpsprotocol.cpp
    gpsprotocol.h
    incomingfriendlist.h
    incomingfriendlist.cpp
    incominggamehost.cpp
    incominggamehost.h
    socket.cpp
    socket.h
    wc3.cpp
    wc3.h
)

target_sources(gproxy PRIVATE ${SOURCES})

target_compile_definitions(gproxy PRIVATE -DBOOST_ALL_DYN_LINK -DBOOST_BIND_GLOBAL_PLACEHOLDERS)

set_target_properties(gproxy PROPERTIES OUTPUT_NAME gproxy)

set_target_properties(gproxy PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(gproxy PROPERTIES SOVERSION 1)
target_compile_features(gproxy PRIVATE cxx_nullptr)

if (WIN32)
    if (MSVC)
        target_compile_options(gproxy PRIVATE -DWIN32 -DNDEBUG -D_CONSOLE -D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS -DPREVENT_DEBUGGERS -D_PORTABLE -DCURL_STATICLIB -D_HAS_STD_BYTE=0 /wd4251 /wd4244 /wd4267)
    else()
        # MinGW: Force include prefix header to disable std::byte before any other headers
        target_compile_options(gproxy PRIVATE -include ${CMAKE_SOURCE_DIR}/common/mingw_prefix.h -DWIN32 -DNDEBUG -D_CONSOLE -D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS -DPREVENT_DEBUGGERS -D_PORTABLE -DCURL_STATICLIB)
    endif()
	target_link_libraries(gproxy PRIVATE ws2_32)
endif()

# Base dependencies
target_link_libraries(gproxy PRIVATE gproxy::common gproxy::handshake Boost::thread Boost::system Boost::filesystem)

# Add addons dependency if XPAM_EDITION is enabled
if(XPAM_EDITION)
    target_link_libraries(gproxy PRIVATE gproxy::addons)
    target_compile_definitions(gproxy PRIVATE -DXPAM_EDITION)
endif()

install(TARGETS gproxy RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

if(WIN32 AND BUILD_SHARED_LIBS)
    # Copy conan dlls
    add_custom_command(
        TARGET gproxy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_BINARY_DIR}/bin" "$<TARGET_FILE_DIR:gproxy>"
    )

    # This does not take care of transitive dependencies but we leave it anyway..
    add_custom_command(
        TARGET gproxy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:gproxy> $<TARGET_FILE_DIR:gproxy>
        COMMAND_EXPAND_LISTS
    )
endif()