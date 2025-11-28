set(DATA_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/cmclient-6.grf
)

foreach(DATA_SOURCE_FILE IN LISTS DATA_SOURCE_FILES)
    string(REPLACE "${CMAKE_SOURCE_DIR}/bin/" "" DATA_SOURCE_FILE_NAME "${DATA_SOURCE_FILE}")
    string(CONCAT DATA_BINARY_FILE "${CMAKE_BINARY_DIR}/" "${DATA_SOURCE_FILE_NAME}")

    add_custom_command(OUTPUT ${DATA_BINARY_FILE}
            COMMAND ${CMAKE_COMMAND} -E copy
                    ${DATA_SOURCE_FILE}
                    ${DATA_BINARY_FILE}
            MAIN_DEPENDENCY ${DATA_SOURCE_FILE}
            COMMENT "Copying ${DATA_SOURCE_FILE_NAME}"
    )

    list(APPEND DATA_BINARY_FILES ${DATA_BINARY_FILE})
endforeach()

# Create a new target which copies all compat files
add_custom_target(data_files
        DEPENDS ${DATA_BINARY_FILES}
)

add_dependencies(openttd
    data_files
)
