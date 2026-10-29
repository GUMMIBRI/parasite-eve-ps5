# Opt in by including this fragment in a standalone public SDK project.
# Deliberately not included by root CMake or any game/rasterizer target.
option(PE_PS5_HD_IMAGE "Build independent bounded PNG decoder" OFF)
if(PE_PS5_HD_IMAGE)
    if(NOT PS5 OR NOT CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR "PE_PS5_HD_IMAGE requires public PS5 SDK toolchain")
    endif()
    add_library(pe-ps5-hd-image STATIC
        "${CMAKE_CURRENT_LIST_DIR}/../../platform/ps5/hd/pe_hd_image.c")
    target_include_directories(pe-ps5-hd-image PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/../../platform/ps5/hd")
    set_target_properties(pe-ps5-hd-image PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    target_compile_options(pe-ps5-hd-image PRIVATE -Wall -Wextra -Wno-unused-function)
endif()
