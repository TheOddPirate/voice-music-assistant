cmake_minimum_required(VERSION 3.16)
project(tr-buttond
        VERSION 1.0.0
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra)

add_executable(tr-buttond src/tr-buttond.cpp)

install(TARGETS tr-buttond RUNTIME DESTINATION bin)
