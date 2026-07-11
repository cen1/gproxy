project(gproxy_common VERSION 1.0.0)

add_library(gproxy_common STATIC)
add_library(gproxy::common ALIAS gproxy_common)

find_package(botan REQUIRED)
find_package(Boost "1.86" COMPONENTS date_time REQUIRED)

set(SOURCES
    config.cpp
    config.h
    log.cpp
    log.h
    systemid.h
    systemid.cpp
    timer.cpp
    timer.h
    util.cpp
    util.h
    w3version.cpp
    w3version.h
)

target_sources(gproxy_common PRIVATE ${SOURCES})

target_compile_definitions(gproxy_common PUBLIC -DBOOST_ALL_DYN_LINK -DBOOST_BIND_GLOBAL_PLACEHOLDERS)

set_target_properties(gproxy_common PROPERTIES OUTPUT_NAME gproxy_common)
target_compile_features(gproxy_common PRIVATE cxx_nullptr)

if (WIN32)
    if (MSVC)
        target_compile_options(gproxy_common PRIVATE -DWIN32 -DNDEBUG -D_CONSOLE -D_CRT_SECURE_NO_WARNINGS -D_HAS_STD_BYTE=0 /wd4251 /wd4244 /wd4267)
    else()
        # MinGW: Force include prefix header to disable std::byte before any other headers
        target_compile_options(gproxy_common PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/mingw_prefix.h -DWIN32 -DNDEBUG -D_CONSOLE -D_CRT_SECURE_NO_WARNINGS)
    endif()
    target_link_libraries(gproxy_common PUBLIC winmm version)
endif()

target_include_directories(gproxy_common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(gproxy_common PUBLIC botan::botan Boost::date_time)
