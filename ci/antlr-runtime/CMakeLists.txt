cmake_minimum_required(VERSION 3.14)
project(pcc-antlr4-runtime-vendor NONE)

# Not pcc's own build (see ../../Makefile's own "pure make - no CMake"
# header comment) - a narrow, standalone CMake project whose only job
# is to fetch, build and install the ANTLR4 C++ runtime at one exact
# pinned version, so pcc's own Makefile can point
# ANTLR4_RUNTIME_PREFIX at the result afterward (see
# ../../.github/workflows/ci.yml's Linux job, the only one that still
# needs this - Windows's job gets an exact-version prebuilt package
# from MSYS2 instead, and macOS/local dev gets one from Homebrew).
# Generated code from one ANTLR version doesn't reliably compile
# against another version's runtime (e.g. 4.13.2's generated code
# needs antlr4::internal::OnceFlag, which doesn't exist before 4.11 -
# see ../../Makefile's own comment), so ANTLR_GIT_TAG below has to
# track the exact same version as the `antlr` tool jar used to
# generate pcc's own parsers - never "whatever's convenient" (e.g.
# Ubuntu's own libantlr4-runtime-dev package, which is 4.10).
#
# ExternalProject_Add, not FetchContent: FetchContent's usual
# add_subdirectory()-into-your-own-build pattern is for linking a
# dependency's CMake target directly into *your own* CMake project
# (e.g. target_link_libraries(my_target PRIVATE antlr4_shared)) -
# that doesn't apply here, since pcc itself isn't a CMake project at
# all. What pcc's Makefile needs instead is a real install tree on
# disk (headers + a static lib) - which is exactly what
# ExternalProject_Add's own configure+build+install steps produce,
# run as a genuinely separate sub-build with its own CMake cache,
# never mixed into this wrapper project's.

include(ExternalProject)

set(ANTLR_GIT_TAG "4.13.2" CACHE STRING
    "antlr/antlr4 git tag to build the C++ runtime from - must match the ANTLR tool jar version used to generate pcc's own parsers")
set(ANTLR_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH
    "Where to install the built runtime - point pcc's Makefile ANTLR4_RUNTIME_PREFIX at this")

ExternalProject_Add(antlr4_runtime
  GIT_REPOSITORY https://github.com/antlr/antlr4.git
  GIT_TAG        ${ANTLR_GIT_TAG}
  GIT_SHALLOW    TRUE
  SOURCE_SUBDIR  runtime/Cpp
  CMAKE_ARGS
    # Debug (-O0), not Release (-O3): this previously got CI's own
    # from-source build killed partway through (SIGTERM) - nothing
    # here needs the runtime's own performance, just a working
    # parser for pcc's own small generated grammars.
    -DCMAKE_BUILD_TYPE=Debug
    -DCMAKE_INSTALL_PREFIX=${ANTLR_INSTALL_PREFIX}
    # Skip the runtime's own test suite (pulls in an old googletest
    # snapshot via its own nested FetchContent, which doesn't
    # configure under a modern CMake - see its own CMakeLists.txt's
    # ANTLR_BUILD_CPP_TESTS option) and the shared lib (static
    # only - half the work, and pcc only ever links statically).
    -DANTLR_BUILD_CPP_TESTS=OFF
    -DANTLR_BUILD_SHARED=OFF
)
