cmake_minimum_required(VERSION 3.16)
project(tr-volume VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSE REQUIRED IMPORTED_TARGET libpulse)

add_executable(tr-volume src/tr-volume.c)
target_compile_options(tr-volume PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tr-volume PRIVATE PkgConfig::PULSE)

install(TARGETS tr-volume RUNTIME DESTINATION bin)
