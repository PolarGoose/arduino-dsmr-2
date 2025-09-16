#include "dsmr/encrypted_packet_accumulator.h"
#include "dsmr/fields.h"
#include "dsmr/parser.h"
#include <doctest.h>
#include <iostream>

using namespace arduino_dsmr_2;
using namespace fields;

TEST_CASE("Complete example encrypted packet accumulator") {
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
