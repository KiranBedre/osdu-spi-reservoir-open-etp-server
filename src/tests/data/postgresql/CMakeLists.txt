
set(
    files_to_copy
    "postgresql_test_config.json"
    "postgresql_test_config_windows.json"
    "postgresql_test_config_localhost.json"
)
copy_files_to_test_data_dir("postgresql" "${files_to_copy}")
unset(files_to_copy)
