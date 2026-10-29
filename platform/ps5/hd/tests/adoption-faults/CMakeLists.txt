cmake_minimum_required(VERSION 3.20)
project(pe_hd_adoption_faults C)
# Standalone host-only shim: production targets and SDL itself are not rewritten.
# cmake -S platform/ps5/hd/tests/adoption-faults -B <build> -DSDL3_DIR=<host-prefix>/lib/cmake/SDL3
# cmake --build <build>
# ctest --test-dir <build> --output-on-failure
if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Constructor fault injection requires a runnable host SDL")
endif()
find_package(SDL3 CONFIG REQUIRED)
set(hd "${CMAKE_CURRENT_LIST_DIR}/../..")
add_library(adoption-injected OBJECT "${hd}/pe_hd_pipeline.c" "${hd}/pe_hd_cache.c")
target_compile_definitions(adoption-injected PRIVATE
    calloc=fault_calloc free=fault_free
    SDL_CreateMutex=fault_CreateMutex SDL_DestroyMutex=fault_DestroyMutex
    SDL_CreateCondition=fault_CreateCondition SDL_DestroyCondition=fault_DestroyCondition
    SDL_CreateThreadRuntime=fault_CreateThreadRuntime SDL_WaitThread=fault_WaitThread)
target_link_libraries(adoption-injected PRIVATE SDL3::SDL3)
add_executable(check-adoption-faults check_adoption_faults.c
    "${hd}/pe_hd_index.c" "${hd}/pe_hd_filename.c" "${hd}/pe_hd_image.c"
    $<TARGET_OBJECTS:adoption-injected>)
target_include_directories(check-adoption-faults PRIVATE "${hd}" "${hd}/..")
target_link_libraries(check-adoption-faults PRIVATE SDL3::SDL3)
# stb_image deliberately defines unused static entry points; keep strict
# diagnostics on the fixture and all ownership code.
if(MSVC)
    set_source_files_properties("${hd}/pe_hd_image.c" PROPERTIES COMPILE_OPTIONS /wd4505)
else()
    set_source_files_properties("${hd}/pe_hd_image.c" PROPERTIES COMPILE_OPTIONS -Wno-unused-function)
endif()
foreach(target adoption-injected check-adoption-faults)
    set_target_properties(${target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    if(MSVC)
        target_compile_options(${target} PRIVATE /UNDEBUG /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -UNDEBUG -Wall -Wextra -Werror)
    endif()
endforeach()
enable_testing()
add_test(NAME hd-adoption-faults COMMAND check-adoption-faults)
set_tests_properties(hd-adoption-faults PROPERTIES TIMEOUT 15)

