#include "dsmr/fields.h"
#include "dsmr/packet_accumulator.h"
#include "dsmr/parser.h"
#include <doctest.h>
#include <iostream>

using namespace arduino_dsmr_2;
using namespace fields;

TEST_CASE("Complete example packet accumulator") {
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
