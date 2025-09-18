#include <doctest.h>
#include <dsmr/encrypted_packet_accumulator.h>
#include <filesystem>
#include <fstream>
#include <source_location>

inline std::vector<std::uint8_t> read_binary_file(const std::filesystem::path path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

const auto& encrypted_packet =
    read_binary_file(std::filesystem::path(std::source_location::current().file_name()).parent_path() / "test_data" / "encrypted_packet.bin");

static std::vector<std::uint8_t> make_header(const std::uint16_t total_len) {
  std::vector<std::uint8_t> header(18);
  const std::array<std::uint8_t, 8> system_title = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
  const std::array<std::uint8_t, 4> invocation_counter = {0x00, 0x11, 0x22, 0x33};

  header[0] = 0xDB; // tag
  header[1] = 0x08; // system_title_length
  std::copy(system_title.begin(), system_title.end(), header.begin() + 2);
  header[10] = 0x82; // long form length indicator
  header[11] = static_cast<std::uint8_t>((total_len >> 8) & 0xFF);
  header[12] = static_cast<std::uint8_t>(total_len & 0xFF);
  header[13] = 0x30; // security_control_field
  std::copy(invocation_counter.begin(), invocation_counter.end(), header.begin() + 14);

  return header;
}

TEST_CASE("Can receive correct packet") {
  auto accumulator = arduino_dsmr_2::EncryptedPacketAccumulator(1000);
  REQUIRE(!accumulator.set_encryption_key("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));

  for (const auto& byte : encrypted_packet) {
    const auto& res = accumulator.process_byte(byte);
    REQUIRE(res.error().has_value() == false);

    if (res.packet()) {
      REQUIRE(std::string(*res.packet()).ends_with("1-0:4.7.0(000000166*var)\r\n!7EF9\r\n"));
      REQUIRE(std::string(*res.packet()).starts_with("/EST5\\253710000_A\r\n"));
      return;
    }
  }

  REQUIRE(false);
}

TEST_CASE("Error on corrupted packet") {
  auto corrupted_packet = encrypted_packet;
  corrupted_packet[50] ^= 0xFF;

  auto accumulator = arduino_dsmr_2::EncryptedPacketAccumulator(1000);
  REQUIRE(!accumulator.set_encryption_key("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").has_value());

  for (const auto& byte : corrupted_packet) {
    const auto& res = accumulator.process_byte(byte);
    if (res.error()) {
      REQUIRE(*res.error() == arduino_dsmr_2::EncryptedPacketAccumulator::Error::DecryptionFailed);
      return;
    }
  }

  REQUIRE(false);
}

TEST_CASE("Receive many packets") {
  auto garbage1 = std::vector<std::uint8_t>(100, 0x55);
  auto packet1 = encrypted_packet;
  auto garbage2 = std::vector<std::uint8_t>(100, 0x55);
  auto packet2_corrupted = encrypted_packet;
  packet2_corrupted[50] ^= 0xFF;
  auto packet3 = encrypted_packet;
  auto packet4 = encrypted_packet;

  std::vector<std::uint8_t> total_packet;
  total_packet.insert(total_packet.end(), garbage1.begin(), garbage1.end());
  total_packet.insert(total_packet.end(), packet1.begin(), packet1.end());
  total_packet.insert(total_packet.end(), garbage2.begin(), garbage2.end());
  total_packet.insert(total_packet.end(), packet2_corrupted.begin(), packet2_corrupted.end());
  total_packet.insert(total_packet.end(), packet3.begin(), packet3.end());
  total_packet.insert(total_packet.end(), packet4.begin(), packet4.end());

  size_t received_packets = 0;
  auto accumulator = arduino_dsmr_2::EncryptedPacketAccumulator(1000);
  REQUIRE(!accumulator.set_encryption_key("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").has_value());

  for (const auto byte : total_packet) {
    auto res = accumulator.process_byte(byte);

    if (res.packet()) {
      received_packets++;
    }
  }

  REQUIRE(received_packets == 3);
}

TEST_CASE("Encryption key validation") {
  auto accumulator = arduino_dsmr_2::EncryptedPacketAccumulator(1000);
  REQUIRE(!accumulator.set_encryption_key("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").has_value());
  REQUIRE(!accumulator.set_encryption_key("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa").has_value());
  REQUIRE(*accumulator.set_encryption_key("AAAAAAAAAAA") == arduino_dsmr_2::EncryptedPacketAccumulator::SetEncryptionKeyError::EncryptionKeyLengthIsNot32Bytes);
  REQUIRE(*accumulator.set_encryption_key("GAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA") ==
          arduino_dsmr_2::EncryptedPacketAccumulator::SetEncryptionKeyError::EncryptionKeyContainsNonHexSymbols);
}

TEST_CASE("BufferOverflow when telegram length exceeds capacity") {
  arduino_dsmr_2::EncryptedPacketAccumulator acc(10);
  const auto& header = make_header(40);
  for (const auto byte : header) {
    const auto& res = acc.process_byte(byte);
    if (res.error()) {
      REQUIRE(*res.error() == arduino_dsmr_2::EncryptedPacketAccumulator::Error::BufferOverflow);
      return;
    }
  }
  REQUIRE(false);
}

TEST_CASE("Telegram is too small") {
  arduino_dsmr_2::EncryptedPacketAccumulator acc(1000);
  const auto& header = make_header(16);
  for (const auto byte : header) {
    const auto& res = acc.process_byte(byte);
    if (res.error()) {
      REQUIRE(*res.error() == arduino_dsmr_2::EncryptedPacketAccumulator::Error::HeaderCorrupted);
      return;
    }
  }
  REQUIRE(false);
}

TEST_CASE("Can receive packet after buffer overflow") {
  arduino_dsmr_2::EncryptedPacketAccumulator accumulator(1000);
  REQUIRE_FALSE(accumulator.set_encryption_key("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").has_value());

  auto too_long_header = make_header(2000);
  bool overflow_error_occurred = false;
  for (const auto byte : too_long_header) {
    const auto& res = accumulator.process_byte(byte);
    if (res.error() == arduino_dsmr_2::EncryptedPacketAccumulator::Error::BufferOverflow) {
      overflow_error_occurred = true;
    }
  }
  REQUIRE(overflow_error_occurred);

  for (const auto& byte : encrypted_packet) {
    auto res = accumulator.process_byte(byte);
    REQUIRE(res.error().has_value() == false);

    if (res.packet()) {
      REQUIRE(std::string(*res.packet()).ends_with("1-0:4.7.0(000000166*var)\r\n!7EF9\r\n"));
      REQUIRE(std::string(*res.packet()).starts_with("/EST5\\253710000_A\r\n"));
      return;
    }
  }

  REQUIRE(false);
}
