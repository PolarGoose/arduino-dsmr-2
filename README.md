This is a fork of [matthijskooijman/arduino-dsmr](https://github.com/matthijskooijman/arduino-dsmr).
The main goal is to make the parser not depend on Arduino framework and be usable in ESPHome DSMR component with ESP-IDF framework.

# Current progress
* Code combines all fixes from [matthijskooijman/arduino-dsmr](https://github.com/matthijskooijman/arduino-dsmr) and [glmnet/arduino-dsmr](https://github.com/glmnet/arduino-dsmr)
* Added extensive unit tests suite
* Small refactoring and code optimizations
* Supported compilers: MSVC, GCC, Clang
* Header only library, no dependencies
* Code can be used on any platform, not only embedded.

# Details
For more details about the parser and DSMR please refer to original [README.md](https://github.com/matthijskooijman/arduino-dsmr/blob/master/README.md) from matthijskooijman

# How to use
## General usage
The library is header only. Add the `src/dsmr` folder to your project.

## Usage from PlatformIO
You can add the library to `platformio.ini`:
```
lib_deps =
  ArduinoDSMR=https://github.com/PolarGoose/arduino-dsmr-make-work-on-Windows-and-Linux.git#<commit-hash>
```

# Examples
## Examples how to use the parser
[minimal_parse.ino](https://github.com/matthijskooijman/arduino-dsmr/blob/master/examples/minimal_parse/minimal_parse.ino)
[parse.ino](https://github.com/matthijskooijman/arduino-dsmr/blob/master/examples/parse/parse.ino)

## Complete example
```
#include "dsmr/fields.h"
#include "dsmr/packet_accumulator.h"
#include "dsmr/parser.h"
#include <iostream>

using namespace dsmr;
using namespace fields;

void main() {
  const auto& data_from_p1_port = "garbage before"
                                  "/KFM5KAIFA-METER\r\n"
                                  "\r\n"
                                  "1-3:0.2.8(40)\r\n"
                                  "0-0:1.0.0(150117185916W)\r\n"
                                  "0-0:96.1.1(0000000000000000000000000000000000)\r\n"
                                  "1-0:1.8.1(000671.578*kWh)\r\n"
                                  "!60e5"
                                  "garbage after"
                                  "/KFM5KAIFA-METER\r\n"
                                  "\r\n"
                                  "1-3:0.2.8(40)\r\n"
                                  "0-0:1.0.0(150117185916W)\r\n"
                                  "0-0:96.1.1(0000000000000000000000000000000000)\r\n"
                                  "1-0:1.8.1(000671.578*kWh)\r\n"
                                  "!60e5";

  // Specify the fields you want to parse.
  // Full list of available fields is in fields.h
  ParsedData<
      /* String */ identification,
      /* String */ p1_version,
      /* String */ timestamp,
      /* String */ equipment_id,
      /* FixedValue */ energy_delivered_tariff1>
      data;

  // This class is used to receive the message from the P1 port.
  // It retrieves bytes from the UART and finds a DSMR message and optionally checks the CRC.
  // You only need to create this class once.
  PacketAccumulator accumulator(/* bufferSize */ 4000, /* check_crc */ true);

  // First step is to get the full message from the P1 port.
  // In this example, we go through the bytes from the message above.
  // In a real application, you need to read the bytes from the UART one byte at a time.
  for (const auto& byte : data_from_p1_port) {
    // feed the byte to the accumulator
    auto res = accumulator.process_byte(byte);

    // During receiving, errors may occur, such as CRC mismatches.
    // You can optionally log these errors, or ignore them.
    if (res.error()) {
      printf("Error during receiving a packet: %s", to_string(*res.error()));
    }

    // When a full packet is received, the packet() method will return it.
    // The packet starts with '/' and ends with the '!'.
    // The CRC is not included.
    if (res.packet()) {
      // Parse the received packet.
      const auto packet = *res.packet();
      // Specify `check_crc` as false, since the accumulator already checked the CRC and didn't include it in the packet
      P1Parser::parse(&data, packet.data(), packet.size(), /* unknown_error */ false, /* check_crc */ false);

      // Now you can use the parsed data.
      printf("Identification: %s\n", data.identification.c_str());
      printf("P1 version: %s\n", data.p1_version.c_str());
      printf("Timestamp: %s\n", data.timestamp.c_str());
      printf("Equipment ID: %s\n", data.equipment_id.c_str());
      printf("Energy delivered tariff 1: %.3f\n", static_cast<double>(data.energy_delivered_tariff1.val()));
    }
  }
}
```

# History behind this fork
[matthijskooijman](https://github.com/matthijskooijman) is the original creator of this DSMR parser.
Later, [glmnet/arduino-dsmr](https://github.com/glmnet/arduino-dsmr) fork was created to make the code work with more DSMR devices.
[glmnet](https://github.com/glmnet) also used his fork to create an [ESPHome DSMR](https://esphome.io/components/sensor/dsmr/) component.
After that, the work on the `arduino-dsmr` parser was abandoned. Currently, it was discovered that dependency on the Arduino framework causes issues for some ESP32 devices (the FW size is too big, and the Arduino framework makes you use an outdated version of ESP-IDF). Thus, I decided to create a new fork and make the parser work on any platform without Arduino.

# How to work with the code
* You can open the repository and work with the code using any IDE that supports Cmake.
* Note: if you want to run `build-windows.ps1` script you need `Visual Studio 2022` to be installed.
