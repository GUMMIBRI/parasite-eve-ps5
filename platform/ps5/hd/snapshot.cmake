# Standalone, intentionally outside the preserved staging publication snapshot.
include_guard(GLOBAL)
option(PE_PS5_HD_SNAPSHOT "Build synchronized full-page identity bridge" OFF)
if(PE_PS5_HD_SNAPSHOT)
    # Explicit dependencies without changing their independent default options.
    include("${CMAKE_CURRENT_LIST_DIR}/identity.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/generation.cmake")
    if(NOT TARGET pe-hd-identity OR NOT TARGET pe-hd-generation)
        message(FATAL_ERROR "Snapshot requires PE_PS5_HD_IDENTITY and PE_PS5_HD_GENERATION")
    endif()
    add_library(pe-hd-snapshot STATIC "${CMAKE_CURRENT_LIST_DIR}/pe_hd_snapshot.c")
    target_link_libraries(pe-hd-snapshot PUBLIC pe-hd-identity pe-hd-generation)
    target_include_directories(pe-hd-snapshot PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
    set_target_properties(pe-hd-snapshot PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
endif()

