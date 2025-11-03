cmake_minimum_required(VERSION 3.29.3)

# Set project name and language
project(dsts_formats LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(dsts_formats src/main.cpp)