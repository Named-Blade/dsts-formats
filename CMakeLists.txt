cmake_minimum_required(VERSION 3.29.3)

# Set project name and language
project(dsts_formats LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(pybind11 REQUIRED)

pybind11_add_module(dsts_formats src/python/bindings.cpp)

# Define staging folder
set(PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/package")

add_custom_target(create_archive ALL
    # Step 1: create staging folder
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_DIR}"

    # Step 2: copy the entire addon folder contents into staging
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/addon"
        "${PACKAGE_DIR}/${CMAKE_PROJECT_NAME}"

    # Step 3: copy the built pybind11 module into the same staging folder
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:dsts_formats>"
        "${PACKAGE_DIR}/${CMAKE_PROJECT_NAME}/$<TARGET_FILE_NAME:dsts_formats>"

    # Step 4: create zip archive from staging folder
    COMMAND ${CMAKE_COMMAND} -E chdir "${PACKAGE_DIR}"
        ${CMAKE_COMMAND} -E tar "cf"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${CMAKE_PROJECT_NAME}.zip"
        "${PACKAGE_DIR}/${CMAKE_PROJECT_NAME}/"
        --format=zip

    DEPENDS dsts_formats
)