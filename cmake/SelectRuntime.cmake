option(PJ_USE_STATIC_RUNTIME "Use static runtime" OFF)

if (PJ_USE_STATIC_RUNTIME)
  if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
endif()