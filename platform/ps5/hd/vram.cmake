# Standalone extraction foundation only; no renderer/game insertion.
option(PE_PS5_HD_VRAM "Build bounded PS1 VRAM texture extraction" OFF)
if(PE_PS5_HD_VRAM)
    add_library(pe-hd-vram STATIC "${CMAKE_CURRENT_LIST_DIR}/pe_hd_vram.c")
    target_include_directories(pe-hd-vram PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
    set_target_properties(pe-hd-vram PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
endif()

