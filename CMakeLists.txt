cmake_minimum_required(VERSION 3.24)

# Clang par défaut, mais -DCMAKE_C_COMPILER=... / -DCMAKE_CXX_COMPILER=... reste possible.
if (NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED ENV{CC})
    set(CMAKE_C_COMPILER clang)
endif()
if (NOT DEFINED CMAKE_CXX_COMPILER AND NOT DEFINED ENV{CXX})
    set(CMAKE_CXX_COMPILER clang++)
endif()

project(tp2 LANGUAGES C CXX)

if (NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Ce projet doit être compilé avec Clang (compilateur détecté : "
        "${CMAKE_C_COMPILER_ID}). Essayez -DCMAKE_C_COMPILER=clang.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
include(FetchContent)

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------

set(SINOSCOPE_IMPL_DIR "${PROJECT_SOURCE_DIR}/source" CACHE PATH
    "Dossier contenant sinoscope-openmp.c, sinoscope-opencl.c et kernel/sinoscope.cl ; relatif à la racine du projet si le chemin n'est pas absolu (ex. : -DSINOSCOPE_IMPL_DIR=charge/solution)")
file(REAL_PATH "${SINOSCOPE_IMPL_DIR}" SINOSCOPE_IMPL_DIR BASE_DIRECTORY "${PROJECT_SOURCE_DIR}")

set(BENCH_ITERATIONS 100 CACHE STRING "Nombre d'itérations des cibles run-*")
set(BENCH_WIDTH 1024 CACHE STRING "Largeur de l'image des cibles run-*")
set(BENCH_HEIGHT 1024 CACHE STRING "Hauteur de l'image des cibles run-*")

option(ENABLE_ASAN "Build project executables with AddressSanitizer" OFF)
option(ENABLE_VIEWER "Build the graphical interface (requires OpenGL and GLFW)" ON)

if (ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
endif()

if (WIN32)
    add_compile_definitions(_USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

# -----------------------------------------------------------------------------
# Dependencies (downloaded and built by CMake, nothing to install on the machine)
# -----------------------------------------------------------------------------

find_package(Threads REQUIRED)

find_package(OpenMP COMPONENTS C)
if (NOT OpenMP_C_FOUND)
    message(FATAL_ERROR
        "OpenMP est introuvable pour ${CMAKE_C_COMPILER}. Sous macOS, utilisez le clang de Homebrew "
        "(`brew install llvm`, -DCMAKE_C_COMPILER=$(brew --prefix llvm)/bin/clang) ou le Dev Container.")
endif()

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(stb)
if (NOT TARGET stb::stb)
    add_library(stb::stb INTERFACE IMPORTED)
    target_include_directories(stb::stb SYSTEM INTERFACE "${stb_SOURCE_DIR}")
endif()

FetchContent_Declare(
    OpenCL-Headers
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-Headers.git
    GIT_TAG v2024.10.24
    GIT_SHALLOW TRUE
)
set(OPENCL_HEADERS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPENCL_HEADERS_BUILD_CXX_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(OpenCL-Headers)

if (APPLE)
    # macOS fournit directement la librairie OpenCL (framework) ; seuls les en-têtes Khronos
    # (<CL/cl.h>) sont téléchargés.
    find_library(OPENCL_FRAMEWORK OpenCL REQUIRED)
    add_library(sinoscope-opencl-lib INTERFACE)
    target_link_libraries(sinoscope-opencl-lib INTERFACE OpenCL::Headers "${OPENCL_FRAMEWORK}")
    set(SINOSCOPE_OPENCL_VERSION 120)
else()
    FetchContent_Declare(
        OpenCL-ICD-Loader
        GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-ICD-Loader.git
        GIT_TAG v2024.10.24
        GIT_SHALLOW TRUE
    )
    set(OPENCL_ICD_LOADER_HEADERS_DIR "${opencl-headers_SOURCE_DIR}" CACHE PATH "" FORCE)
    set(OPENCL_ICD_LOADER_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(OpenCL-ICD-Loader)
    if (TARGET OpenCL)
        target_compile_options(OpenCL PRIVATE -w)
    endif()
    add_library(sinoscope-opencl-lib INTERFACE)
    target_link_libraries(sinoscope-opencl-lib INTERFACE OpenCL::Headers OpenCL::OpenCL)
    set(SINOSCOPE_OPENCL_VERSION 220)
endif()

set(SINOSCOPE_HAS_VIEWER OFF)
if (ENABLE_VIEWER)
    find_package(OpenGL)

    set(_viewer_ok ${OpenGL_FOUND})
    if (UNIX AND NOT APPLE)
        find_package(X11)
        if (NOT (X11_FOUND AND X11_Xrandr_FOUND AND X11_Xinerama_FOUND AND X11_Xcursor_FOUND AND X11_Xi_FOUND))
            set(_viewer_ok OFF)
        endif()
    endif()

    if (_viewer_ok)
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4
            GIT_SHALLOW TRUE
        )
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(glfw)
        # Les avertissements du code de GLFW ne concernent pas le laboratoire
        target_compile_options(glfw PRIVATE -w)
        set(SINOSCOPE_HAS_VIEWER ON)
    else()
        message(WARNING "OpenGL/X11 introuvables : l'interface graphique n'est pas compilée "
            "(utilisez --headless). Sous Debian/Ubuntu : libgl-dev libx11-dev libxrandr-dev "
            "libxinerama-dev libxcursor-dev libxi-dev.")
    endif()
endif()

# -----------------------------------------------------------------------------
# Executables
# -----------------------------------------------------------------------------

set(SINOSCOPE_COMMON_SOURCES
    source/color.c
    source/headless.c
    source/image.c
    source/main.c
    source/platform.c
    source/sinoscope.c
    source/sinoscope-serial.c
)

# add_sinoscope_executable(<name> [OPENCL] [OPENMP] [VIEWER])
#   OPENCL / OPENMP / VIEWER: compile the real implementation instead of the stubs of the
#   same name (source/stubs-*.c), so that the other parts can be used without them.
function(add_sinoscope_executable name)
    cmake_parse_arguments(ARG "OPENCL;OPENMP;VIEWER" "" "" ${ARGN})

    add_executable(${name} ${SINOSCOPE_COMMON_SOURCES})
    target_include_directories(${name} PRIVATE include)
    target_compile_options(${name} PRIVATE -Wall)
    target_link_libraries(${name} PRIVATE Threads::Threads stb::stb sinoscope-opencl-lib)
    if (NOT WIN32)
        target_link_libraries(${name} PRIVATE m)
    endif()
    target_compile_definitions(${name} PRIVATE CL_TARGET_OPENCL_VERSION=${SINOSCOPE_OPENCL_VERSION})

    if (ARG_OPENMP)
        target_sources(${name} PRIVATE "${SINOSCOPE_IMPL_DIR}/sinoscope-openmp.c")
        target_link_libraries(${name} PRIVATE OpenMP::OpenMP_C)
    else()
        target_sources(${name} PRIVATE source/stubs-openmp.c)
    endif()

    if (ARG_OPENCL)
        target_sources(${name} PRIVATE source/opencl.c "${SINOSCOPE_IMPL_DIR}/sinoscope-opencl.c")
        target_compile_definitions(${name} PRIVATE
            __KERNEL_FILE__="${SINOSCOPE_IMPL_DIR}/kernel/sinoscope.cl"
            __OPENCL_INCLUDE__="${PROJECT_SOURCE_DIR}/source/kernel")
    else()
        target_sources(${name} PRIVATE source/stubs-opencl.c)
    endif()

    if (ARG_VIEWER AND SINOSCOPE_HAS_VIEWER)
        target_sources(${name} PRIVATE source/viewer.c)
        target_link_libraries(${name} PRIVATE glfw OpenGL::GL)
        if (APPLE)
            target_compile_definitions(${name} PRIVATE GL_SILENCE_DEPRECATION)
        endif()
    else()
        target_sources(${name} PRIVATE source/stubs-viewer.c)
    endif()
endfunction()

add_sinoscope_executable(sinoscope OPENCL OPENMP VIEWER)
add_sinoscope_executable(sinoscope-nocl OPENMP)
add_sinoscope_executable(sinoscope-nomp OPENCL)

# -----------------------------------------------------------------------------
# Commandes
# -----------------------------------------------------------------------------

# Compare les temps (et accélérations) du sériel, d'OpenMP et d'OpenCL, puis valide les résultats.
add_custom_target(run-all
    COMMAND $<TARGET_FILE:sinoscope> --width ${BENCH_WIDTH} --height ${BENCH_HEIGHT}
            --benchmarks ${BENCH_ITERATIONS}
    COMMAND ${CMAKE_COMMAND} -E echo "-- vérification --"
    COMMAND $<TARGET_FILE:sinoscope> --check mp
    COMMAND $<TARGET_FILE:sinoscope> --check cl
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(run-all sinoscope)

add_custom_target(run-serial
    COMMAND $<TARGET_FILE:sinoscope-nocl> --width ${BENCH_WIDTH} --height ${BENCH_HEIGHT}
            --benchmark serial ${BENCH_ITERATIONS}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(run-serial sinoscope-nocl)

add_custom_target(run-openmp
    COMMAND $<TARGET_FILE:sinoscope-nocl> --width ${BENCH_WIDTH} --height ${BENCH_HEIGHT}
            --benchmark mp ${BENCH_ITERATIONS}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(run-openmp sinoscope-nocl)

add_custom_target(run-opencl
    COMMAND $<TARGET_FILE:sinoscope-nomp> --width ${BENCH_WIDTH} --height ${BENCH_HEIGHT}
            --benchmark cl ${BENCH_ITERATIONS}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(run-opencl sinoscope-nomp)

add_test(NAME check-openmp COMMAND sinoscope-nocl --check mp)
add_test(NAME check-opencl COMMAND sinoscope-nomp --check cl)

file(GLOB_RECURSE SINOSCOPE_FORMAT_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/source/*.c"
    "${PROJECT_SOURCE_DIR}/source/*.cl"
    "${PROJECT_SOURCE_DIR}/include/*.h")
add_custom_target(format
    COMMAND clang-format -i --verbose ${SINOSCOPE_FORMAT_FILES}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMAND_EXPAND_LISTS
)

# -----------------------------------------------------------------------------
# Vérificateur de consignes (optionnel : nécessite les librairies de développement de Clang)
# -----------------------------------------------------------------------------

find_package(Clang CONFIG QUIET)
if (Clang_FOUND)
    add_executable(source-checker
        source/matcher/main.cpp
        source/matcher/file_search.cpp
        source/matcher/matchers.cpp
    )
    target_include_directories(source-checker SYSTEM PRIVATE ${CLANG_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS})
    target_compile_definitions(source-checker PRIVATE ${LLVM_DEFINITIONS})
    # LLVM est compilé sans RTTI
    target_compile_options(source-checker PRIVATE -fno-rtti)
    if (APPLE)
        # Les cibles importées de zlib/libxml2 ajoutent <SDK>/usr/include avant les en-têtes de
        # libc++ lorsqu'on utilise le clang de Homebrew, ce qui casse la compilation.
        foreach(_target ZLIB::ZLIB LibXml2::LibXml2)
            if (TARGET ${_target})
                set_property(TARGET ${_target} PROPERTY INTERFACE_INCLUDE_DIRECTORIES "")
            endif()
        endforeach()
    endif()
    target_link_libraries(source-checker PRIVATE clangTooling)

    add_custom_target(check-source
        COMMAND $<TARGET_FILE:source-checker> -d ${SINOSCOPE_IMPL_DIR}
                -I ${PROJECT_SOURCE_DIR}/source/kernel
        USES_TERMINAL
    )
    add_dependencies(check-source source-checker)

    add_test(NAME check-source
        COMMAND source-checker -d ${SINOSCOPE_IMPL_DIR} -I ${PROJECT_SOURCE_DIR}/source/kernel)
else()
    message(STATUS "Clang (librairies de développement) introuvable : cible check-source désactivée")
endif()

# -----------------------------------------------------------------------------
# Installation
# -----------------------------------------------------------------------------

install(TARGETS sinoscope sinoscope-nocl sinoscope-nomp)
install(DIRECTORY "${SINOSCOPE_IMPL_DIR}/kernel/" DESTINATION share/cl/sinoscope
        FILES_MATCHING PATTERN "*.cl")
