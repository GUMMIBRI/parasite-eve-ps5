cmake_minimum_required(VERSION 3.20)
project(pe_card_import VERSION 1.0 LANGUAGES C)
add_executable(pe-card-import main.c memcard.c)
target_compile_features(pe-card-import PRIVATE c_std_11)
if(NOT MSVC)
  target_compile_options(pe-card-import PRIVATE -Wall -Wextra)
  target_compile_definitions(pe-card-import PRIVATE _POSIX_C_SOURCE=200809L)
endif()

