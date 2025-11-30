cmake_minimum_required(VERSION 3.20)
project(curriculum_cli C)

set(CMAKE_C_STANDARD 11)

find_package(CURL REQUIRED)
find_package(unofficial-pdcurses CONFIG REQUIRED)
find_package(Jansson CONFIG REQUIRED)

add_executable(curriculum-cli
    main.c
)

target_link_libraries(curriculum-cli PRIVATE CURL::libcurl unofficial::pdcurses::pdcurses)
target_link_libraries(curriculum-cli PRIVATE jansson::jansson)