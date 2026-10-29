# Public PS5 platform fragment for a generated copy of pinned SDL3.
# This is not Sony's private SDL backend. Video is CPU/software presentation.

if(SDL_AUDIO)
  set(SDL_AUDIO_DRIVER_PS5 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/audio/ps5/*.c"
                   "${SDL3_SOURCE_DIR}/src/audio/ps5/*.h")
  sdl_include_directories(PRIVATE "${SDL3_SOURCE_DIR}/src/audio")
  sdl_link_dependency(ps5-audio LIBS SceAudioOut)
  set(HAVE_SDL_AUDIO TRUE)
endif()

if(SDL_VIDEO)
  set(SDL_VIDEO_DRIVER_PS5 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/video/ps5/*.c"
                   "${SDL3_SOURCE_DIR}/src/video/ps5/*.h")
  sdl_include_directories(PRIVATE "${SDL3_SOURCE_DIR}/src/video")
  sdl_link_dependency(ps5-video LIBS SceVideoOut SceSystemService)
  set(HAVE_SDL_VIDEO TRUE)
endif()

if(SDL_JOYSTICK)
  set(SDL_JOYSTICK_PS5 1)
  sdl_glob_sources("${SDL3_SOURCE_DIR}/src/joystick/ps5/*.c"
                   "${SDL3_SOURCE_DIR}/src/joystick/ps5/*.h")
  sdl_include_directories(PRIVATE "${SDL3_SOURCE_DIR}/src/joystick")
  sdl_link_dependency(ps5-pad LIBS ScePad SceUserService)
  set(HAVE_SDL_JOYSTICK TRUE)
endif()

# Public SDK libc provides these POSIX facilities. CMake checks pthread
# signatures through the actual cross compiler, without -lpthread host linkage.
set(SDL_PTHREADS_PRIVATE ON)
CheckPTHREAD()
if(NOT HAVE_PTHREADS)
  message(FATAL_ERROR "Public PS5 SDL3 requires working SDK pthread headers/linkage")
endif()

set(SDL_FILESYSTEM_UNIX 1)
sdl_glob_sources("${SDL3_SOURCE_DIR}/src/filesystem/unix/*.c")
set(HAVE_SDL_FILESYSTEM TRUE)
set(SDL_FSOPS_POSIX 1)
sdl_sources("${SDL3_SOURCE_DIR}/src/filesystem/posix/SDL_sysfsops.c")
set(HAVE_SDL_FSOPS TRUE)
set(SDL_STORAGE_GENERIC 1)
sdl_glob_sources("${SDL3_SOURCE_DIR}/src/storage/generic/*.c")
set(HAVE_SDL_STORAGE TRUE)
set(SDL_TIME_UNIX 1)
sdl_glob_sources("${SDL3_SOURCE_DIR}/src/time/unix/*.c")
set(HAVE_SDL_TIME TRUE)
set(SDL_TIMER_UNIX 1)
sdl_glob_sources("${SDL3_SOURCE_DIR}/src/timer/unix/*.c")
set(HAVE_SDL_TIMERS TRUE)
set(SDL_LOADSO_DLOPEN 1)
sdl_sources("${SDL3_SOURCE_DIR}/src/loadso/dlopen/SDL_sysloadso.c")
set(HAVE_SDL_LOADSO TRUE)
