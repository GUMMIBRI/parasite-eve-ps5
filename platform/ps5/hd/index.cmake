# Opt-in standalone manifest/provider; no root/game/rasterizer integration.
option(PE_PS5_HD_INDEX "Build bounded read-only HD index/provider foundation" OFF)
if(PE_PS5_HD_INDEX)
    if(NOT TARGET pe-hd-filename)
        include("${CMAKE_CURRENT_LIST_DIR}/filename.cmake")
    endif()
    add_library(pe-hd-index STATIC "${CMAKE_CURRENT_LIST_DIR}/pe_hd_index.c")
    target_include_directories(pe-hd-index PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
    target_link_libraries(pe-hd-index PUBLIC pe-hd-filename)
    set_target_properties(pe-hd-index PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
endif()

