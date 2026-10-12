####################################################################################################
# This function converts any file into C/C++ source code.
# Example:
# - input file: data.dat
# - output file: data.h
# - variable name declared in output file: DATA
# - data length: sizeof(DATA)
# embed_resource("data.dat" "data.h" "DATA")
####################################################################################################

function(embed_resource resource_file_name source_file_name variable_name)

    set( output_file "${CMAKE_CURRENT_BINARY_DIR}/${source_file_name}")
    set(input_file "${CMAKE_CURRENT_SOURCE_DIR}/${resource_file_name}")

    # Only generate file if destination does not exists or is older than input file
    if( "${input_file}" IS_NEWER_THAN "${output_file}" )
        file(READ ${input_file} hex_content HEX)

        string(REPEAT "[0-9a-f]" 32 column_pattern)
        string(REGEX REPLACE "(${column_pattern})" "\\1\n" content "${hex_content}")

        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " content "${content}")

        string(REGEX REPLACE ", $" "" content "${content}")

        set(array_definition "static const unsigned char ${variable_name}[] =\n{\n${content}\n};")

        set(source "// Auto generated file.\n${array_definition}\n")

        file(WRITE "${output_file}" "${source}")
    endif()

endfunction()
