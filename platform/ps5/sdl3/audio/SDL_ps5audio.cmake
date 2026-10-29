# Include inside SDL3's dedicated public-PS5 platform branch, before final
# build-config generation. Requires register-audio.patch to have been applied
# to a separate build copy of SDL3; does not mutate the upstream submodule.
if(SDL_AUDIO)
  set(SDL_AUDIO_DRIVER_PS5 1)
  set(HAVE_SDL_AUDIO TRUE)
  sdl_sources("${CMAKE_CURRENT_LIST_DIR}/SDL_ps5audio.c"
              "${CMAKE_CURRENT_LIST_DIR}/SDL_ps5audio.h")
  sdl_include_directories(PRIVATE "${SDL3_SOURCE_DIR}/src/audio")
  sdl_link_dependency(ps5-audio LIBS SceAudioOut)
endif()
