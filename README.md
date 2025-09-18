This is a fork of [matthijskooijman/arduino-dsmr](https://github.com/matthijskooijman/arduino-dsmr).
The primary goal is to make the parser independent of the Arduino framework and usable on ESP32 with the ESP-IDF framework or any other platform.

# Features
* Combines all fixes from [matthijskooijman/arduino-dsmr](https://github.com/matthijskooijman/arduino-dsmr) and [glmnet/arduino-dsmr](https://github.com/glmnet/arduino-dsmr) including unmerged pull requests.
* Added an extensive unit test suite
* Small refactoring and code optimizations
* Supported compilers: MSVC, GCC, Clang
* Header-only library, no dependencies
* Code can be used on any platform, not only embedded.

# Differences from the original arduino-dsmr
* Requires a C++20 compatible compiler.
* [P1Reader](https://github.com/matthijskooijman/arduino-dsmr/blob/master/src/dsmr/reader.h) class is replaced with the [PacketAccumulator](https://github.com/PolarGoose/arduino-dsmr-2/blob/master/src/dsmr/packet_accumulator.h) class with a different interface to allow usage on any platform.
* Added `EncryptedPacketAccumulator` class to receive encrypted DSMR messages (like from "Luxembourg Smarty").

# How to use
## General usage
The library is header-only. Add the `src/dsmr` folder to your project.<br>
Note: `encrypted_packet_accumulator.h` header depends on [Mbed TLS](https://www.trustedfirmware.org/projects/mbed-tls/) library. It is already included in the `ESP-IDF` framework and can be easily added to any other platforms.

## Usage from PlatformIO
The library is available on the PlatformIO registry:<br>
[PlatformIO arduino-dsmr-2](https://registry.platformio.org/libraries/polargoose/arduino-dsmr-2/installation)

# Examples
## Examples of how to use the parser
* [minimal_parse.ino](https://github.com/matthijskooijman/arduino-dsmr/blob/master/examples/minimal_parse/minimal_parse.ino)
* [parse.ino](https://github.com/matthijskooijman/arduino-dsmr/blob/master/examples/parse/parse.ino)

## Complete example
```
#include "dsmr/fields.h"
#include "dsmr/packet_accumulator.h"
#include "dsmr/parser.h"
#include <iostream>

using namespace arduino_dsmr_2;
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
                                  "!60e5
                                  "another packet";

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

## EncryptedPacketAccumulator example
```
#include "dsmr/fields.h"
#include "dsmr/encrypted_packet_accumulator.h"
#include "dsmr/parser.h"
#include <doctest.h>
#include <iostream>

using namespace arduino_dsmr_2;
using namespace fields;

void main() {
  std::vector<uint8_t> encrypted_data_from_p1_port;

  // This class is similar to the PacketAccumulator, but it handles encrypted packets.
  // Use this class only if you have a smart meter that uses encryption.
  EncryptedPacketAccumulator accumulator(4000);

  // Set the encryption key. This key is unique for each smart meter and should be provided by your energy supplier.
  const auto error = accumulator.set_encryption_key("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
  if (error) {
    printf("Failed to set encryption key: %s", to_string(*error));
    return;
  }

  for (const auto& byte : encrypted_data_from_p1_port) {
    // feed the byte to the accumulator
    auto res = accumulator.process_byte(byte);
    if (res.error()) {
      printf("Error during receiving a packet: %s", to_string(*res.error()));
    }

    // When a full packet is received, the packet() method returns unencrypted packet.
    if (res.packet()) {
      // Parse the received packet the same way as with PacketAccumulator example
    }
  }
}
```

# History behind arduino-dsmr
[matthijskooijman](https://github.com/matthijskooijman) is the original creator of this DSMR parser.
[glmnet](https://github.com/glmnet) and [zuidwijk](https://github.com/zuidwijk) continued work on this parser in the fork [glmnet/arduino-dsmr](https://github.com/glmnet/arduino-dsmr). They used the parser to create [ESPHome DSMR](https://esphome.io/components/sensor/dsmr/) component.
After that, the work on the `arduino-dsmr` parser stopped.
Since then, some issues and unmerged pull requests have accumulated. Additionally, the dependency on the Arduino framework causes various issues for some ESP32 boards.
This fork addresses the existing issues and makes the parser usable on any platform.

## The reasons `arduino-dsmr-2` fork was created
* Dependency on the Arduino framework limits the applicability of this parser. For example, it is not possible to use it on Linux.
* The Arduino framework on ESP32 inflates the FW size and doesn't allow usage of the latest version of ESP-IDF.
* Many pull requests and bug fixes needed to be integrated into the parser.
* Lack of support for encrypted DSMR messages.

# How to work with the code
* You can open the code using any IDE that supports CMake.
  * `build-windows.ps1` script needs `Visual Studio 2022` to be installed.
  * `build-linux.sh` script needs `clang` to be installed.

# References
* [DSMR parser in Python](https://github.com/ndokter/dsmr_parser/tree/master)
