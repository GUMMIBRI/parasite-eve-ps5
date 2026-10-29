# Standalone only; intentionally not included from the game/root CMake.
option(PE_PS5_HD_PIPELINE "Build bounded HD index/cache pipeline" OFF)
if(PE_PS5_HD_PIPELINE)
    if(NOT TARGET pe-hd-index OR NOT TARGET pe-ps5-hd-cache)
        message(FATAL_ERROR "Enable/include HD index and cache before pipeline.cmake")
    endif()
    add_library(pe-hd-pipeline STATIC "${CMAKE_CURRENT_LIST_DIR}/pe_hd_pipeline.c")
    target_include_directories(pe-hd-pipeline PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
    target_link_libraries(pe-hd-pipeline PUBLIC pe-hd-index pe-ps5-hd-cache SDL3::SDL3)
    set_target_properties(pe-hd-pipeline PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    target_compile_options(pe-hd-pipeline PRIVATE -Wall -Wextra -Werror)
endif()

