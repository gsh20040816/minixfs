cmake_minimum_required(VERSION 3.16)

project(minixfs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MINIXFS_SOURCES
    src/BlockDevice.cpp
    src/DirEntry.cpp
    src/FS.cpp
    src/Inode.cpp
    src/Utils.cpp
)

add_library(libminixfs SHARED ${MINIXFS_SOURCES})
target_include_directories(libminixfs
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
set_target_properties(libminixfs PROPERTIES OUTPUT_NAME minixfs)

add_executable(minixfs-cli tools/minixfs-cli.cpp)
target_link_libraries(minixfs-cli PRIVATE libminixfs)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

add_executable(minixfs-fuse fuse/main.cpp)
target_include_directories(minixfs-fuse
    PRIVATE
        ${FUSE3_INCLUDE_DIRS}
)
target_link_directories(minixfs-fuse
    PRIVATE
        ${FUSE3_LIBRARY_DIRS}
)
target_link_libraries(minixfs-fuse
    PRIVATE
        libminixfs
        ${FUSE3_LIBRARIES}
)
target_compile_options(minixfs-fuse
    PRIVATE
        ${FUSE3_CFLAGS_OTHER}
)

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME minixfs_consistency
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/minixfs_consistency.sh
                $<TARGET_FILE:minixfs-fuse>
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
                ${CMAKE_CURRENT_BINARY_DIR}/ctest/minixfs_consistency
    )
    set_tests_properties(minixfs_consistency PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 180
    )
endif()
