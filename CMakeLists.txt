cmake_minimum_required(VERSION 3.14)

project(tinyxpd
    VERSION 1.0.0
    DESCRIPTION "Header-only C++11 XGen XPD cache I/O library"
    LANGUAGES CXX
)

# ============================================
# Header-only library
# ============================================
add_library(tinyxpd INTERFACE)
target_include_directories(tinyxpd INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(tinyxpd INTERFACE cxx_std_11)

# ============================================
# Options
# ============================================
option(TINYXPD_BUILD_TOOLS "Build XPD reader tools" ON)
option(TINYXPD_BUILD_EXAMPLES "Build examples" ON)

# ============================================
# Alembic dependency (for tools)
# ============================================
if(TINYXPD_BUILD_TOOLS)
    set(ALEMBIC_ROOT "Z:/devkits/2026/devkitBase/devkit/Alembic" CACHE PATH "Alembic SDK root directory")
    set(ALEMBIC_INCLUDE_DIR "${ALEMBIC_ROOT}/include")
    set(ALEMBIC_LIBRARY_DIR "${ALEMBIC_ROOT}/lib")
endif()

# ============================================
# Subdirectories
# ============================================
if(TINYXPD_BUILD_TOOLS)
    add_subdirectory(src)
endif()

# ============================================
# Installation
# ============================================
install(FILES include/tiny_xpd.h DESTINATION include)
install(DIRECTORY docs/ DESTINATION share/doc/tinyxpd)
