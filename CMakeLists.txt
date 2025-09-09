cmake_minimum_required (VERSION 3.20)
project(arduino-dsmr-test LANGUAGES CXX)

file(DOWNLOAD
  https://github.com/doctest/doctest/releases/download/v2.4.12/doctest.h
  ${CMAKE_BINARY_DIR}/doctest/doctest.h
  EXPECTED_MD5 0671bbca9fb00cb19a168cffa89ac184)

file(GLOB_RECURSE arduino_dsmr_test_src_files CONFIGURE_DEPENDS "src/*.h" "src/*.cpp")
add_executable(arduino_dsmr_test ${arduino_dsmr_test_src_files})
target_include_directories(arduino_dsmr_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/doctest)
target_compile_features(arduino_dsmr_test PUBLIC cxx_std_17)
