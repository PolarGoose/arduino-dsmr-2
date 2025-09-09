#!/usr/bin/env bash

set -o xtrace -o errexit -o nounset -o pipefail

readonly currentScriptDir=`dirname "$(realpath -s "${BASH_SOURCE[0]}")"`
readonly buildDir="${currentScriptDir}/build"

echo "Build and test debug configuration"
cmake -S "${currentScriptDir}" \
      -B "${buildDir}/linux-debug" \
      -D CMAKE_BUILD_TYPE=Debug
cmake --build "${buildDir}/linux-debug"
"${buildDir}/linux-debug/arduino_dsmr_test"

echo "Build and test release configuration"
cmake -S "${currentScriptDir}" \
      -B "${buildDir}/linux-release" \
      -D CMAKE_BUILD_TYPE=Release
cmake --build "${buildDir}/linux-release"
"${buildDir}/linux-release/arduino_dsmr_test"

echo "Success"
