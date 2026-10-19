
set(
    files_to_copy
    "client_Epic_20191113.trace"
    "interp_well_survey.epc"
    "interp_well_survey.h5"
    "PillarGrid.epc"
    "PillarGrid.h5"
    "WELL_RESQML.epc"
    "WELL_RESQML.h5"
    "server_20191113.trace"
)
copy_files_to_test_data_dir("eml" "${files_to_copy}")
unset(files_to_copy)
