This is a fork of [matthijskooijman/arduino-dsmr](https://github.com/matthijskooijman/arduino-dsmr).
The main goal is to make the parser not depend on Arduino framework and be usable in ESPHome DSMR component with ESP-IDF framework.

# Current progress
* Code combines all fixes from [matthijskooijman/arduino-dsmr](https://github.com/matthijskooijman/arduino-dsmr) and [glmnet/arduino-dsmr](https://github.com/glmnet/arduino-dsmr)
* Added extensive unit tests suite
* Small refactoring and code optimizations
* Code is compatible with MSVC and GCC compilers
* Header only library, no dependencies
* Code can be used on any platform, not only embedded.

# Details
For more details about the parser and DSMR please refer to original [README.md](https://github.com/matthijskooijman/arduino-dsmr/blob/master/README.md) from matthijskooijman

# History behind this fork
[matthijskooijman](https://github.com/matthijskooijman) is the original creator of this DSMR parser.
Later, [glmnet/arduino-dsmr](https://github.com/glmnet/arduino-dsmr) fork was created to make the code work with more DSMR devices.
[glmnet](https://github.com/glmnet) also used his fork to create an [ESPHome DSMR](https://esphome.io/components/sensor/dsmr/) component.
After that, the work on the `arduino-dsmr` parser was abandoned. Currently, it was discovered that dependency on the Arduino framework causes issues for some ESP32 devices (the FW size is too big, and the Arduino framework makes you use an outdated version of ESP-IDF). Thus, I decided to create a new fork and make the parser work on any platform without Arduino.

# How to work with the code
* You can open the repository and work with the code using any IDE that supports Cmake.
* Note: if you want to run `build-windows.ps1` script you need `Visual Studio 2022` to be installed.
