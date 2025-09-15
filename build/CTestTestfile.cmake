# CMake generated Testfile for 
# Source directory: /Users/bruno/Documents/Development/bruno/vulkano_codex
# Build directory: /Users/bruno/Documents/Development/bruno/vulkano_codex/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[icosphere_tests]=] "/Users/bruno/Documents/Development/bruno/vulkano_codex/build/bin/icosphere_tests")
set_tests_properties([=[icosphere_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "/Users/bruno/Documents/Development/bruno/vulkano_codex/CMakeLists.txt;142;add_test;/Users/bruno/Documents/Development/bruno/vulkano_codex/CMakeLists.txt;0;")
subdirs("_deps/glm-build")
subdirs("_deps/glfw-build")
subdirs("_deps/catch2-build")
