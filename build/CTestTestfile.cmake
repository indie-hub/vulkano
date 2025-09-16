# CMake generated Testfile for 
# Source directory: /Users/bruno/Documents/Development/bruno/vulkano_codex
# Build directory: /Users/bruno/Documents/Development/bruno/vulkano_codex/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[build_sanity]=] "/Users/bruno/Documents/Development/bruno/vulkano_codex/bin/vulkano_codex_tests")
set_tests_properties([=[build_sanity]=] PROPERTIES  _BACKTRACE_TRIPLES "/Users/bruno/Documents/Development/bruno/vulkano_codex/CMakeLists.txt;126;add_test;/Users/bruno/Documents/Development/bruno/vulkano_codex/CMakeLists.txt;0;")
subdirs("_deps/glfw-build")
subdirs("_deps/glm-build")
