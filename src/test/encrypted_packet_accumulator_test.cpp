#include "arduino-dsmr-2/encrypted_packet_accumulator.h"
#include <doctest.h>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <source_location>

using namespace arduino_dsmr_2;

template <class T, class... Vecs>
std::vector<T> concat(const std::vector<T>& first, const Vecs&... rest) {
  std::vector<T> out;
  out.reserve(first.size() + (rest.size() + ... + 0));
  out.insert(out.end(), first.begin(), first.end());
  (out.insert(out.end(), rest.begin(), rest.end()), ...);
  return out;
}

inline std::vector<std::uint8_t> read_binary_file(const std::filesystem::path path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

const auto& encrypted_packet =
    read_binary_file(std::filesystem::path(std::source_location::current().file_name()).parent_path() / "test_data" / "encrypted_packet.bin");

static void change_length(std::vector<std::uint8_t>& packet, const std::uint16_t total_len) {
  packet[11] = static_cast<std::uint8_t>((total_len >> 8) & 0xFF);
  packet[12] = static_cast<std::uint8_t>(total_len & 0xFF);
}

TEST_CASE("Can receive correct packet") {
  auto accumulator = EncryptedPacketAccumulator(1000);
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

  auto accumulator = EncryptedPacketAccumulator(1000);
  REQUIRE(!accumulator.set_encryption_key("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").has_value());

  for (const auto& byte : corrupted_packet) {
    const auto& res = accumulator.process_byte(byte);
    if (res.error()) {
      REQUIRE(*res.error() == EncryptedPacketAccumulator::Error::DecryptionFailed);
      return;
    }
  }

  REQUIRE(false);
}

TEST_CASE("Encryption key validation") {
  auto accumulator = EncryptedPacketAccumulator(1000);
  REQUIRE(!accumulator.set_encryption_key("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").has_value());
  REQUIRE(!accumulator.set_encryption_key("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa").has_value());
  REQUIRE(*accumulator.set_encryption_key("AAAAAAAAAAA") == EncryptedPacketAccumulator::SetEncryptionKeyError::EncryptionKeyLengthIsNot32Bytes);
  REQUIRE(*accumulator.set_encryption_key("GAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA") ==
          EncryptedPacketAccumulator::SetEncryptionKeyError::EncryptionKeyContainsNonHexSymbols);
}

TEST_CASE("BufferOverflow when telegram length exceeds capacity") {
  EncryptedPacketAccumulator acc(10);
  for (const auto byte : encrypted_packet) {
    const auto& res = acc.process_byte(byte);
    if (res.error()) {
      REQUIRE(*res.error() == EncryptedPacketAccumulator::Error::BufferOverflow);
      return;
    }
  }
  REQUIRE(false);
}

TEST_CASE("Telegram is too small") {
  EncryptedPacketAccumulator acc(1000);
  auto too_small_packet = encrypted_packet;
  change_length(too_small_packet, 16);

  for (const auto byte : too_small_packet) {
    const auto& res = acc.process_byte(byte);
    if (res.error()) {
      REQUIRE(*res.error() == EncryptedPacketAccumulator::Error::HeaderCorrupted);
      return;
    }
  }
  REQUIRE(false);
}

TEST_CASE("Reset works") {
  EncryptedPacketAccumulator accumulator(1000);
  REQUIRE_FALSE(accumulator.set_encryption_key("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").has_value());

  for (auto byte : encrypted_packet | std::views::take(50)) {
    const auto& res = accumulator.process_byte(byte);
    REQUIRE(res.error().has_value() == false);
    REQUIRE(res.packet().has_value() == false);
  }

  accumulator.reset();

  bool got_packet = false;
  for (const auto& byte : encrypted_packet) {
    const auto& res = accumulator.process_byte(byte);
    REQUIRE(res.error().has_value() == false);

    if (res.packet()) {
      REQUIRE(std::string(*res.packet()).starts_with("/EST5\\253710000_A\r\n"));
      REQUIRE(std::string(*res.packet()).ends_with("1-0:4.7.0(000000166*var)\r\n!7EF9\r\n"));
      got_packet = true;
      break;
    }
  }

  REQUIRE(got_packet);
}

TEST_CASE("Receive many packets") {
  auto accumulator = EncryptedPacketAccumulator(500);
  REQUIRE(!accumulator.set_encryption_key("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").has_value());

  const auto& garbage = std::vector<std::uint8_t>(100, 0x55);

  auto packet_corrupted = encrypted_packet;
  packet_corrupted[50] ^= 0xFF;

  auto packet_too_short_length = encrypted_packet;
  change_length(packet_too_short_length, 16);

  auto packet_too_long_length = encrypted_packet;
  change_length(packet_too_long_length, 2000);

  size_t received_packets = 0;
  std::vector<EncryptedPacketAccumulator::Error> occurred_errors;

  for (const auto byte : concat(garbage, encrypted_packet, garbage, packet_too_short_length, packet_corrupted, encrypted_packet, packet_corrupted,
                                encrypted_packet, packet_too_long_length, encrypted_packet)) {
    auto res = accumulator.process_byte(byte);

    if (res.packet()) {
      received_packets++;
    }

    if (res.error()) {
      occurred_errors.push_back(*res.error());
    }
  }

  REQUIRE(received_packets == 4);

  using enum EncryptedPacketAccumulator::Error;
  REQUIRE(occurred_errors == std::vector{HeaderCorrupted, HeaderCorrupted, HeaderCorrupted, HeaderCorrupted, DecryptionFailed, DecryptionFailed, BufferOverflow,
                                         HeaderCorrupted, HeaderCorrupted, HeaderCorrupted});
}
