// This code tests that the packet_accumulator header has all necessary dependencies included in its headers.
// We check that the code compiles.

#include "dsmr/packet_accumulator.h"

void PacketAccumulator_some_function() { arduino_dsmr_2::PacketAccumulator({}, true); }
