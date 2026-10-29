# Standalone parser foundation. Not enabled in or linked to the game by default.
add_library(pe-hd-filename STATIC "${CMAKE_CURRENT_LIST_DIR}/pe_hd_filename.c")
target_include_directories(pe-hd-filename PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
set_target_properties(pe-hd-filename PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)

