#include "arduino-dsmr-2/packet_accumulator.h"
#include <doctest.h>
#include <string>

using namespace arduino_dsmr_2;

TEST_CASE("Packet with correct CRC lower case") {
  const auto& msg = "/some !a3D4";

  auto accumulator = PacketAccumulator(1000, true);
  for (const auto& byte : msg) {
    auto res = accumulator.process_byte(byte);
    REQUIRE(res.error().has_value() == false);

    if (res.packet()) {
      REQUIRE(std::string(*res.packet()) == std::string(msg, std::size(msg) - 5));
      return;
    }
  }

  REQUIRE(false);
}

TEST_CASE("Packet with incorrect CRC") {
  const auto& msg = "/some data!0000";

  PacketAccumulator accumulator(1000, true);
  for (const auto& byte : msg) {
    auto packet = accumulator.process_byte(byte);
    if (packet.error()) {
      REQUIRE(*packet.error() == PacketAccumulator::Error::CrcMismatch);
      return;
    }
  }

  REQUIRE(false);
}

TEST_CASE("Packet with incorrect CRC symbol") {
  const auto& msg = "/some data!G000";

  PacketAccumulator accumulator(1000, true);
  for (const auto& byte : msg) {
    auto packet = accumulator.process_byte(byte);
    if (packet.error()) {
      REQUIRE(*packet.error() == PacketAccumulator::Error::IncorrectCrcCharacter);
      return;
    }
  }

  REQUIRE(false);
}

TEST_CASE("Packet without CRC") {
  const auto& msg = "/some data!";

  PacketAccumulator accumulator(1000, false);
  for (const auto& byte : msg) {
    auto res = accumulator.process_byte(byte);
    REQUIRE(res.error().has_value() == false);

    if (res.packet()) {
      REQUIRE(std::string(*res.packet()) == std::string(msg, std::size(msg) - 1));
      return;
    }
  }
}

TEST_CASE("Parse data with different packets. CRC check") {
  const auto& msg = "garbage /some !a3D4"      // correct package
                    "garbage /some !a3D3"      // CRC mismatch
                    "garbage /so/some !a3D4"   // Packet start symbol '/' in the middle of the packet
                    "garbage /some !a3G4"      // Incorrect CRC character
                    "/some !a3D4"              // correct package
                    "/garbage garbage garbage" // buffer overflow
                    "/some !a3D4";             // correct package

  std::vector<std::string> received_packets;
  std::vector<PacketAccumulator::Error> occurred_errors;

  PacketAccumulator accumulator(15, true);
  for (const auto& byte : msg) {
    auto res = accumulator.process_byte(byte);
    if (res.error()) {
      occurred_errors.push_back(*res.error());
    }

    if (res.packet()) {
      received_packets.push_back(std::string(*res.packet()));
    }
  }

  using enum PacketAccumulator::Error;
  REQUIRE(occurred_errors == std::vector{CrcMismatch, PacketStartSymbolInPacket, IncorrectCrcCharacter, BufferOverflow});
  REQUIRE(received_packets == std::vector<std::string>(4, "/some !"));
}

TEST_CASE("Parse data with different packets. No CRC check") {
  const auto& msg = "garbage /some !"          // correct package
                    "garbage /so/some !"       // Packet start symbol '/' in the middle of the packet
                    "/some !"                  // correct package
                    "/garbage garbage garbage" // buffer overflow
                    "/some !";                 // correct package

  std::vector<std::string> received_packets;
  std::vector<PacketAccumulator::Error> occurred_errors;

  PacketAccumulator accumulator(15, false);
  for (const auto& byte : msg) {
    auto res = accumulator.process_byte(byte);
    if (res.error()) {
      occurred_errors.push_back(*res.error());
    }

    if (res.packet()) {
      received_packets.push_back(std::string(*res.packet()));
    }
  }

  using enum PacketAccumulator::Error;
  REQUIRE(occurred_errors == std::vector{PacketStartSymbolInPacket, BufferOverflow});
  REQUIRE(received_packets == std::vector<std::string>(4, "/some !"));
}
