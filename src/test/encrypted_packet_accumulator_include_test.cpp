// This code tests that the encrypted_packet_accumulator header has all necessary dependencies included in its headers.
// We check that the code compiles.

#include "dsmr/encrypted_packet_accumulator.h"

void EncryptedPacketAccumulator_some_function() { arduino_dsmr_2::EncryptedPacketAccumulator(1000); }
