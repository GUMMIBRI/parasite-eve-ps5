# Standalone opt-in only. No game/rasterizer insertion or lookup identity claim.
option(PE_PS5_HD_CACHE "Build bounded asynchronous image cache" OFF)
if(PE_PS5_HD_CACHE)
    if(NOT PS5 OR NOT CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR "PE_PS5_HD_CACHE requires public PS5 SDK toolchain")
    endif()
    if(NOT TARGET pe-ps5-hd-image)
        message(FATAL_ERROR "Include hd.cmake with PE_PS5_HD_IMAGE=ON first")
    endif()
    find_package(SDL3 CONFIG REQUIRED)
    add_library(pe-ps5-hd-cache STATIC
        "${CMAKE_CURRENT_LIST_DIR}/../../platform/ps5/hd/pe_hd_cache.c")
    target_link_libraries(pe-ps5-hd-cache PUBLIC pe-ps5-hd-image SDL3::SDL3)
    set_target_properties(pe-ps5-hd-cache PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    target_compile_options(pe-ps5-hd-cache PRIVATE -Wall -Wextra -Werror)
endif()

