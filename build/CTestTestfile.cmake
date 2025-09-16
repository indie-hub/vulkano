# CMake generated Testfile for 
# Source directory: /Users/bruno/Documents/Development/bruno/vulkano_codex
# Build directory: /Users/bruno/Documents/Development/bruno/vulkano_codex/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[vulkano_tests]=] "/Users/bruno/Documents/Development/bruno/vulkano_codex/build/bin/vulkano_tests")
set_tests_properties([=[vulkano_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "/Users/bruno/Documents/Development/bruno/vulkano_codex/CMakeLists.txt;123;add_test;/Users/bruno/Documents/Development/bruno/vulkano_codex/CMakeLists.txt;0;")
subdirs("_deps/glfw-build")
subdirs("_deps/glm-build")
subdirs("_deps/catch2-build")
