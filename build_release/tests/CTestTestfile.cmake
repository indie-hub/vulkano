# CMake generated Testfile for 
# Source directory: /Users/bruno/Documents/Development/bruno/vulkano_codex/tests
# Build directory: /Users/bruno/Documents/Development/bruno/vulkano_codex/build_release/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
include("/Users/bruno/Documents/Development/bruno/vulkano_codex/build_release/tests/vulkano_tests-b12d07c_include.cmake")
add_test([=[app_smoke]=] "/Users/bruno/Documents/Development/bruno/vulkano_codex/bin/vulkano_app")
set_tests_properties([=[app_smoke]=] PROPERTIES  ENVIRONMENT "VK_APP_AUTOCLOSE_MS=800;VK_SHADER_DIR=/Users/bruno/Documents/Development/bruno/vulkano_codex/bin/shaders" _BACKTRACE_TRIPLES "/Users/bruno/Documents/Development/bruno/vulkano_codex/tests/CMakeLists.txt;28;add_test;/Users/bruno/Documents/Development/bruno/vulkano_codex/tests/CMakeLists.txt;0;")
add_test([=[app_resize]=] "/Users/bruno/Documents/Development/bruno/vulkano_codex/bin/vulkano_app")
set_tests_properties([=[app_resize]=] PROPERTIES  ENVIRONMENT "VK_APP_AUTOCLOSE_MS=1500;VK_TEST_RESIZE=1;VK_TEST_RESIZE_INTERVAL_MS=250;VK_SHADER_DIR=/Users/bruno/Documents/Development/bruno/vulkano_codex/bin/shaders" _BACKTRACE_TRIPLES "/Users/bruno/Documents/Development/bruno/vulkano_codex/tests/CMakeLists.txt;35;add_test;/Users/bruno/Documents/Development/bruno/vulkano_codex/tests/CMakeLists.txt;0;")
subdirs("../_deps/catch2-build")
