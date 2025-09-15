cmake_minimum_required (VERSION 3.20)
project(arduino-dsmr-test LANGUAGES CXX)
include(FetchContent)

file(DOWNLOAD
  https://github.com/doctest/doctest/releases/download/v2.4.12/doctest.h
  ${CMAKE_BINARY_DIR}/doctest/doctest.h
  EXPECTED_MD5 0671bbca9fb00cb19a168cffa89ac184)

FetchContent_Populate(
  cmake_template
  URL https://github.com/cpp-best-practices/cmake_template/archive/refs/heads/main.zip
  URL_HASH MD5=c391b4c21eeabac1d5142bcf404c740c)
include(${cmake_template_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
include(${cmake_template_SOURCE_DIR}/cmake/Sanitizers.cmake)

file(GLOB_RECURSE arduino_dsmr_test_src_files CONFIGURE_DEPENDS "src/*.h" "src/*.cpp")
add_executable(arduino_dsmr_test ${arduino_dsmr_test_src_files})
target_include_directories(arduino_dsmr_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/doctest)
target_compile_features(arduino_dsmr_test PUBLIC cxx_std_20)

# enable warnings
add_library(arduino_dsmr_test_warnings INTERFACE)
myproject_set_project_warnings(arduino_dsmr_test_warnings ON "" "" "" "")
if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
  target_compile_options(arduino_dsmr_test_warnings INTERFACE -Wno-gnu-zero-variadic-macro-arguments)
endif()
target_link_libraries(arduino_dsmr_test PRIVATE arduino_dsmr_test_warnings)

# enable sanitizers: address, leak, undefined behaviour
add_library(arduino_dsmr_test_sanitizers INTERFACE)
myproject_enable_sanitizers(arduino_dsmr_test_sanitizers ON ON ON OFF OFF)
target_link_libraries(arduino_dsmr_test PRIVATE arduino_dsmr_test_sanitizers)
