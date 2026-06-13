cmake_minimum_required(VERSION 3.15)

project(Skript VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Fix runtime library for MSVC
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")
endif()

# Include header files
file(GLOB GTPROXY_INCLUDE_FILES
    client/*.hpp
    core/*.hpp
    packet/*.hpp
    extension/*.hpp
    extension/**/*.hpp
    packet/game/*.hpp
    packet/message/*.hpp
    player/*.hpp
    server/*.hpp
    utils/*.hpp
    utils/extra_tile_data.h
    utils/world_tile.h
    utils/world_info.h
    utils/lua_bridge.hpp  # Make sure this is included
)

# Include source files
file(GLOB GTPROXY_SOURCE_FILES
    *.cpp
    client/*.cpp
    core/*.cpp
    extension/*.cpp
    extension/**/*.hpp
    extension/**/*.cpp
    packet/*.cpp
    player/*.cpp
    server/*.cpp
    utils/*.cpp
    utils/world_info.cpp
)

# ImGui source files
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
list(APPEND GTPROXY_SOURCE_FILES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx9.cpp
)

set(RC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/version.rc")
if(EXISTS "${RC_FILE}")
    list(APPEND GTPROXY_SOURCE_FILES ${RC_FILE})
endif()

add_executable(${PROJECT_NAME}
    ${GTPROXY_INCLUDE_FILES}
    ${GTPROXY_SOURCE_FILES})

find_package(fmt REQUIRED)
find_package(glm REQUIRED)
find_package(magic_enum REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(pcg-cpp REQUIRED)
find_package(spdlog REQUIRED)

# FIX: Find and setup LuaJIT properly
find_package(LuaJIT QUIET)

if(NOT LuaJIT_FOUND)
    message(STATUS "LuaJIT not found via find_package, searching manually...")
    
    # Common LuaJIT installation paths
    set(LUAJIT_PATHS
        "C:/Users/11User/vcpkg/installed/x64-windows"
        "C:/vcpkg/installed/x64-windows"
        "$ENV{VCPKG_ROOT}/installed/x64-windows"
        "$ENV{USERPROFILE}/vcpkg/installed/x64-windows"
        "C:/Program Files/LuaJIT"
        "C:/LuaJIT"
    )
    
    # Find LuaJIT include directory
    find_path(LUAJIT_INCLUDE_DIR
        NAMES lua.hpp luajit.h
        PATHS ${LUAJIT_PATHS}
        PATH_SUFFIXES include include/luajit include/lua include/luajit-2.1
    )
    
    # Find LuaJIT library
    find_library(LUAJIT_LIBRARY
        NAMES lua51 lua5.1 lua liblua51 liblua5.1 luajit libluajit
        PATHS ${LUAJIT_PATHS}
        PATH_SUFFIXES lib lib/x64
    )
    
    if(LUAJIT_INCLUDE_DIR AND LUAJIT_LIBRARY)
        message(STATUS "Found LuaJIT include: ${LUAJIT_INCLUDE_DIR}")
        message(STATUS "Found LuaJIT library: ${LUAJIT_LIBRARY}")
        
        # Create imported target for LuaJIT
        add_library(lua_jit SHARED IMPORTED)
        set_target_properties(lua_jit PROPERTIES
            IMPORTED_LOCATION "${LUAJIT_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LUAJIT_INCLUDE_DIR}"
        )
        
        # Create alias
        add_library(luajit::luajit ALIAS lua_jit)
        set(LuaJIT_FOUND TRUE)
    else()
        message(WARNING "LuaJIT not found. Creating dummy target...")
        # Create empty interface library as fallback
        add_library(lua_dummy INTERFACE)
        add_library(luajit::luajit ALIAS lua_dummy)
        set(LuaJIT_FOUND FALSE)
    endif()
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME}
    enet
    eventpp
    crypto
    tls
    ssl  # Added explicit ssl library
    fmt::fmt
    glm::glm
    httplib::httplib
    magic_enum::magic_enum
    nlohmann_json::nlohmann_json
    pcg-cpp::pcg-cpp
    spdlog::spdlog
)

# Add LuaJIT if found
if(TARGET luajit::luajit)
    target_link_libraries(${PROJECT_NAME} luajit::luajit)
    message(STATUS "Linking with LuaJIT")
endif()

# Windows system libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME}
        ws2_32
        winmm
        crypt32
        secur32
    )
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lib/libressl/include
    ${CMAKE_SOURCE_DIR}/third_party/imgui
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends
)

# Also add include directories for LuaJIT if we found them
if(LUAJIT_INCLUDE_DIR)
    target_include_directories(${PROJECT_NAME} PRIVATE ${LUAJIT_INCLUDE_DIR})
endif()

# Compiler options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE 
        /EHsc
        /W4
        /permissive-
        /Zc:__cplusplus
        /std:c++latest
        /D_ENABLE_EXTENDED_ALIGNED_STORAGE
        /D_HAS_CXX20=1
        /MD$<$<CONFIG:Debug>:d>  # Dynamic runtime
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -fexceptions)
endif()

# Preprocessor definitions
target_compile_definitions(${PROJECT_NAME} PUBLIC
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    SPDLOG_FMT_EXTERNAL
    CPPHTTPLIB_OPENSSL_SUPPORT
    _HAS_CXX20=1
    # FIX: Use Windows certificate store for SSL verification
    CPPHTTPLIB_USE_CERTS_FROM_WIN32_CERT_STORE=1
    # LuaJIT specific definitions
    LUAJIT_TARGET=LUAJIT_OS_WINDOWS
    LUAJIT_DISABLE_JIT=0
)

# If LuaJIT not found, define a macro to disable it
if(NOT LuaJIT_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE 
        DISABLE_LUA_SUPPORT=1
    )
    message(WARNING "LuaJIT not found. Lua support will be disabled.")
endif()

# Version definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    GTPROXY_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    GTPROXY_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    GTPROXY_VERSION_PATCH=${PROJECT_VERSION_PATCH})

# Debug mode
if(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "" OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE GTPROXY_DEBUG)
endif()

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# Copy resources if they exist
if(EXISTS "${CMAKE_SOURCE_DIR}/resources")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
        COMMENT "Copying resources"
    )
else()
    message(STATUS "Resources directory not found, skipping...")
endif()

# Copy certificate files to resources folder
if(EXISTS "${CMAKE_SOURCE_DIR}/cert.pem")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/cert.pem"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources/cert.pem"
        COMMENT "Copying cert.pem to resources"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/key.pem")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/key.pem"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources/key.pem"
        COMMENT "Copying key.pem to resources"
    )
endif()

# NEW: Copy CA certificate if it exists
if(EXISTS "${CMAKE_SOURCE_DIR}/cacert.pem")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/cacert.pem"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources/cacert.pem"
        COMMENT "Copying CA certificate to resources"
    )
endif()

# NEW: Auto-download CA certificate if missing
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/cacert.pem")
    message(STATUS "CA certificate not found. Adding download instructions...")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
        COMMAND ${CMAKE_COMMAND} -E echo "NOTE: Download CA certificate for SSL:"
        COMMAND ${CMAKE_COMMAND} -E echo "      https://curl.se/ca/cacert.pem"
        COMMAND ${CMAKE_COMMAND} -E echo "      Save as cacert.pem in resources folder"
        COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    )
endif()

# Post-build message
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "Build completed for Skript v${PROJECT_VERSION}"
    COMMAND ${CMAKE_COMMAND} -E echo "LuaJIT: $<IF:$<BOOL:${LuaJIT_FOUND}>,Found,NOT FOUND>"
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
if(EXISTS "${CMAKE_SOURCE_DIR}/resources")
    install(DIRECTORY resources/ DESTINATION resources)
endif()