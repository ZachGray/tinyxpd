cmake_minimum_required(VERSION 3.14)

project(tinyxpd
    VERSION 1.0.0
    DESCRIPTION "Header-only C++11 XGen XPD cache I/O library"
    LANGUAGES CXX
)

# Header-only library
add_library(tinyxpd INTERFACE)
target_include_directories(tinyxpd INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(tinyxpd INTERFACE cxx_std_11)

# Alembic paths
set(ALEMBIC_ROOT "Z:/devkits/2026/devkitBase/devkit/Alembic")
set(ALEMBIC_INCLUDE_DIR "${ALEMBIC_ROOT}/include")
set(ALEMBIC_LIBRARY_DIR "${ALEMBIC_ROOT}/lib")

# Examples
option(TINYXPD_BUILD_EXAMPLES "Build examples" ON)

if(TINYXPD_BUILD_EXAMPLES)
    # XPD Reader Example
    add_executable(xpd_reader_example xpd_reader_example.cc)
    target_link_libraries(xpd_reader_example PRIVATE tinyxpd)

    # XPD Detailed Reader
    add_executable(xpd_reader_detailed xpd_reader_detailed.cc)
    target_link_libraries(xpd_reader_detailed PRIVATE tinyxpd)

    # XPD to Alembic Converter
    if(EXISTS ${ALEMBIC_INCLUDE_DIR})
        add_executable(xpd_reader_abc xpd_reader_abc.cc)
        target_link_libraries(xpd_reader_abc PRIVATE tinyxpd)
        target_include_directories(xpd_reader_abc PRIVATE
            ${ALEMBIC_INCLUDE_DIR}
            ${ALEMBIC_INCLUDE_DIR}/Imath
        )
        target_link_directories(xpd_reader_abc PRIVATE ${ALEMBIC_LIBRARY_DIR})
        target_link_libraries(xpd_reader_abc PRIVATE Alembic Imath-3_1-maya)
        message(STATUS "Building xpd_reader_abc with Alembic support")
    else()
        message(WARNING "Alembic not found at ${ALEMBIC_ROOT}, skipping xpd_reader_abc")
    endif()

    # Simple Spline Writer (if source exists)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/simple_sprine_writer)
        file(GLOB SPLINE_SOURCES "examples/simple_sprine_writer/*.cc" "examples/simple_sprine_writer/*.cpp")
        if(SPLINE_SOURCES)
            add_executable(simple_spline_writer ${SPLINE_SOURCES})
            target_link_libraries(simple_spline_writer PRIVATE tinyxpd)
        endif()
    endif()
endif()
