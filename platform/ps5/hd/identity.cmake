# Independent opt-in content identity only; never activates renderer replacement.
include_guard(GLOBAL)
option(PE_PS5_HD_IDENTITY "Build bounded full-CLUT texpage content identity" OFF)
if(PE_PS5_HD_IDENTITY)
    add_library(pe-hd-identity STATIC "${CMAKE_CURRENT_LIST_DIR}/pe_hd_identity.c")
    target_include_directories(pe-hd-identity PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
    set_target_properties(pe-hd-identity PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
endif()

