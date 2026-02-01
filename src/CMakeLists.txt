# Basic reader (no dependencies)
add_executable(xpd_reader_basic xpd_reader.cc)
target_link_libraries(xpd_reader_basic PRIVATE tinyxpd)

# XPD to Alembic converter (requires Alembic)
if(EXISTS ${ALEMBIC_INCLUDE_DIR})
    add_executable(xpd_to_abc xpd_to_abc.cc)
    target_link_libraries(xpd_to_abc PRIVATE tinyxpd)
    target_include_directories(xpd_to_abc PRIVATE
        ${ALEMBIC_INCLUDE_DIR}
        ${ALEMBIC_INCLUDE_DIR}/Imath
    )
    target_link_directories(xpd_to_abc PRIVATE ${ALEMBIC_LIBRARY_DIR})
    target_link_libraries(xpd_to_abc PRIVATE Alembic Imath-3_1-maya)

    message(STATUS "Building xpd_to_abc with Alembic support")

    # Install the tools
    install(TARGETS xpd_to_abc xpd_reader_basic
        RUNTIME DESTINATION bin
    )
else()
    message(WARNING "Alembic not found at ${ALEMBIC_ROOT}, skipping xpd_to_abc")
    message(STATUS "Only building xpd_reader_basic")

    install(TARGETS xpd_reader_basic
        RUNTIME DESTINATION bin
    )
endif()
