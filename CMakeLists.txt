cmake_minimum_required(VERSION 3.20)

project(CalculagraphTimer VERSION 1.0.0 LANGUAGES CXX)

add_executable(CalculagraphTimer WIN32
    src/main.cpp
)

target_compile_features(CalculagraphTimer PRIVATE cxx_std_20)

target_compile_definitions(CalculagraphTimer PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MINGW)
    target_compile_options(CalculagraphTimer PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Werror
        -Wold-style-cast
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
    )

    target_link_options(CalculagraphTimer PRIVATE
        -municode
        -static
        -static-libgcc
        -static-libstdc++
    )
elseif(MSVC)
    target_compile_options(CalculagraphTimer PRIVATE
        /W4
        /WX
        /permissive-
    )
endif()
