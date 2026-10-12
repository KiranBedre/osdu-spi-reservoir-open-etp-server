
macro(copy_files_to_test_data_dir subdir files)
    set(destination "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data/${subdir}")
    foreach(file ${files})
        configure_file( "${file}" "${destination}/${file}" COPYONLY)
    endforeach()
endmacro()


add_subdirectory(eml)
add_subdirectory(postgresql)

# create a drectories for some UT output
file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data/openkv)
file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data/epc)
