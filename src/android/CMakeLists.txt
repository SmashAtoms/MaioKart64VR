find_package(OpenXR REQUIRED CONFIG)
# NDK's absolute source path becomes part of Ninja's object name and exceeds
# MAX_PATH in a normal Windows checkout. Copy it into the generated build tree
# so the object name remains short; the NDK header is still used unchanged.
set(AETHER_NATIVE_GLUE "${CMAKE_CURRENT_BINARY_DIR}/native_app_glue.c")
configure_file("${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c" "${AETHER_NATIVE_GLUE}" COPYONLY)
add_library(native_app_glue STATIC "${AETHER_NATIVE_GLUE}")
target_include_directories(native_app_glue PUBLIC "${ANDROID_NDK}/sources/android/native_app_glue")
add_library(aether64 SHARED android_main.cpp xr_app.cpp gles_renderer.cpp)
target_include_directories(aether64 PRIVATE .)
target_compile_definitions(aether64 PRIVATE XR_USE_PLATFORM_ANDROID XR_USE_GRAPHICS_API_OPENGL_ES)
target_compile_options(aether64 PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(aether64 PRIVATE -uANativeActivity_onCreate -Wl,-z,max-page-size=16384)
target_link_libraries(aether64 PRIVATE aether_core native_app_glue OpenXR::openxr_loader EGL GLESv3 android log jnigraphics)
