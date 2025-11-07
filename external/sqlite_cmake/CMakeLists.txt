cmake_minimum_required(VERSION 3.24)
project(SQLite3 LANGUAGES C)

# Build static SQLite library from amalgamation
add_library(sqlite3 STATIC sqlite3.c)

target_include_directories(sqlite3 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Common SQLite compile options
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_ENABLE_COLUMN_METADATA
    SQLITE_ENABLE_FTS3
    SQLITE_ENABLE_FTS4
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_JSON1
    SQLITE_ENABLE_RTREE
    SQLITE_THREADSAFE=1
)

# Platform-specific options
if(UNIX AND NOT EMSCRIPTEN)
    target_compile_definitions(sqlite3 PRIVATE HAVE_USLEEP)
    target_link_libraries(sqlite3 PRIVATE ${CMAKE_DL_LIBS})
    find_package(Threads REQUIRED)
    target_link_libraries(sqlite3 PRIVATE Threads::Threads)
endif()

if(EMSCRIPTEN)
    target_compile_definitions(sqlite3 PRIVATE
        SQLITE_OS_OTHER=1
        SQLITE_TEMP_STORE=3
        SQLITE_OMIT_WAL
    )
endif()

# Optional: link with zlib if available
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    target_link_libraries(sqlite3 PRIVATE ZLIB::ZLIB)
endif()

# Installation
install(TARGETS sqlite3
    EXPORT SQLite3Targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES sqlite3.h sqlite3ext.h
    DESTINATION include
)

# Generate and install CMake config files
include(CMakePackageConfigHelpers)

install(EXPORT SQLite3Targets
    FILE SQLite3Targets.cmake
    NAMESPACE SQLite::
    DESTINATION lib/cmake/SQLite3
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/SQLite3Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SQLite3Config.cmake
    INSTALL_DESTINATION lib/cmake/SQLite3
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/SQLite3ConfigVersion.cmake
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/SQLite3Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/SQLite3ConfigVersion.cmake
    DESTINATION lib/cmake/SQLite3
)
