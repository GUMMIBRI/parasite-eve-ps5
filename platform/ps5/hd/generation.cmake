# Standalone generation foundation; staging can opt in, no renderer hook.
option(PE_PS5_HD_GENERATION "Build bounded VRAM tile generation tracker" OFF)
if(PE_PS5_HD_GENERATION AND NOT TARGET pe-hd-generation)
    add_library(pe-hd-generation STATIC "${CMAKE_CURRENT_LIST_DIR}/pe_hd_generation.c")
    target_include_directories(pe-hd-generation PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
    set_target_properties(pe-hd-generation PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
endif()

