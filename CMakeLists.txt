cmake_minimum_required(VERSION 3.30)
project(comet C)

set(CMAKE_C_STANDARD 99)

# dependencies
include(FetchContent)
FetchContent_Declare(raylib URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.zip)
FetchContent_MakeAvailable(raylib)
FetchContent_Declare(lua URL https://www.lua.org/ftp/lua-5.1.5.tar.gz)
FetchContent_MakeAvailable(lua)

# TODO: needs a way to load assets into desktop versions of the game
if (${PLATFORM} MATCHES "Web")
    set(CMAKE_EXECUTABLE_SUFFIX .html)

    # embedding a file with Emscripten requires a relative path
    file(RELATIVE_PATH rel ${CMAKE_BINARY_DIR} ${PROJECT_SOURCE_DIR}/user)
    add_link_options(--embed-file ${rel})

    if(${CMAKE_BUILD_TYPE} MATCHES Debug)
        add_compile_options(
                -fsanitize=address
                -fsanitize=undefined
        )
        add_link_options(
                -sALLOW_MEMORY_GROWTH
                -fsanitize=address
                -fsanitize=undefined
        )
    endif()
endif()

add_executable(${PROJECT_NAME} main.c)

target_include_directories(${PROJECT_NAME} PRIVATE ${raylib_SOURCE_DIR}/src)
target_link_directories(${PROJECT_NAME} PRIVATE ${raylib_BINARY_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

target_include_directories(${PROJECT_NAME} PRIVATE ${lua_SOURCE_DIR}/src)
target_sources(${PROJECT_NAME} PRIVATE
        ${lua_SOURCE_DIR}/src/lapi.c
        ${lua_SOURCE_DIR}/src/lauxlib.c
        ${lua_SOURCE_DIR}/src/lbaselib.c
        ${lua_SOURCE_DIR}/src/lcode.c
        ${lua_SOURCE_DIR}/src/ldblib.c
        ${lua_SOURCE_DIR}/src/ldebug.c
        ${lua_SOURCE_DIR}/src/ldo.c
        ${lua_SOURCE_DIR}/src/ldump.c
        ${lua_SOURCE_DIR}/src/lfunc.c
        ${lua_SOURCE_DIR}/src/lgc.c
        ${lua_SOURCE_DIR}/src/linit.c
        ${lua_SOURCE_DIR}/src/liolib.c
        ${lua_SOURCE_DIR}/src/llex.c
        ${lua_SOURCE_DIR}/src/lmathlib.c
        ${lua_SOURCE_DIR}/src/lmem.c
        ${lua_SOURCE_DIR}/src/loadlib.c
        ${lua_SOURCE_DIR}/src/lobject.c
        ${lua_SOURCE_DIR}/src/lopcodes.c
        ${lua_SOURCE_DIR}/src/loslib.c
        ${lua_SOURCE_DIR}/src/lparser.c
        ${lua_SOURCE_DIR}/src/lstate.c
        ${lua_SOURCE_DIR}/src/lstring.c
        ${lua_SOURCE_DIR}/src/lstrlib.c
        ${lua_SOURCE_DIR}/src/ltable.c
        ${lua_SOURCE_DIR}/src/ltablib.c
        ${lua_SOURCE_DIR}/src/ltm.c
        ${lua_SOURCE_DIR}/src/lundump.c
        ${lua_SOURCE_DIR}/src/lvm.c
        ${lua_SOURCE_DIR}/src/lzio.c
)
