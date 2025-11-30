cmake_minimum_required(VERSION 3.20)
project(curriculum C)

set(CMAKE_C_STANDARD 11)

find_package(civetweb CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(Jansson CONFIG REQUIRED)

add_executable(curriculum
    src/main.c
    src/server.c
    src/handlers.c
    src/db.c
    src/utils.c
)

target_link_libraries(curriculum
    PRIVATE
        civetweb::civetweb
        unofficial::sqlite3::sqlite3
        jansson::jansson
)

enable_testing()

add_executable(test_db
    test/test_db.c
    src/db.c
    src/utils.c
)

target_link_libraries(test_db
    PRIVATE
        civetweb::civetweb
        unofficial::sqlite3::sqlite3
        jansson::jansson
)

add_test(NAME db_test COMMAND test_db)

# Build the separate CLI application
add_subdirectory(cli)
