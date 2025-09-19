#pragma once
#include "util.h"
#include <array>
#include <mbedtls/gcm.h>
#include <optional>
#include <span>
#include <vector>

namespace arduino_dsmr_2 {

// Some smart meters sent DSMR packets encrypted with AES-128-GCM.
// The encryption is described in the "specs/Luxembourg Smarty P1 specification v1.1.3.pdf" chapter "3.2.5 P1 software – Channel security".
// The packet has the following structure:
//   Header (18 bytes) | Telegram | GCM Tag (12 bytes)
class EncryptedPacketAccumulator : NonCopyableAndNonMovable {
  class HeaderAccumulator {
#pragma pack(push, 1)
    union Header {
      struct Fields {
        uint8_t tag;                 // always = 0xDB
        uint8_t system_title_length; // always = 0x08
        uint8_t system_title[8];
        uint8_t long_form_length_indicator;       // always = 0x82
        uint8_t total_length_big_endian[2];       // SecurityControlFieldLength + InvocationCounterLength + CiphertextLength + GcmTagLength
        uint8_t security_control_field;           // always = 0x30
        uint8_t invocation_counter_big_endian[4]; // also called "frame counter"
      } fields;
      std::array<std::uint8_t, sizeof(Fields)> bytes;
    };
#pragma pack(pop)
    static_assert(sizeof(Header) == 18, "PacketHeader struct must be 18 bytes");

    Header header;
    std::size_t number_of_accumulated_bytes = 0;

  public:
    bool received_whole_header() const { return number_of_accumulated_bytes == sizeof(Header); }

    void add(const std::uint8_t byte) {
      header.bytes[number_of_accumulated_bytes] = byte;
      number_of_accumulated_bytes++;
    }

    int telegram_with_gcm_tag_length() const {
      // Convert from big-endian to the host's little-endian
      const auto total_length = (header.fields.total_length_big_endian[0] << 8) | (header.fields.total_length_big_endian[1]);
      return total_length - 5; // 5 = SecurityControlFieldLength + InvocationCounterLength
    }

    // Also called "IV"
    auto nonce() const {
      // SystemTitle (8 bytes) + InvocationCounter (4 bytes)
      std::array<uint8_t, 12> nonce;
      const auto& system_title = header.fields.system_title;
      const auto& invocation_counter = header.fields.invocation_counter_big_endian;
      std::copy(system_title, system_title + 8, nonce.begin());
      std::copy(invocation_counter, invocation_counter + 4, nonce.begin() + 8);
      return nonce;
    }

    // There is no way to check if the received header is valid.
    // Best we can do is to check the values of the constant fields and that the length is realistic.
    bool check_consistency() const {
      return header.fields.tag == 0xDB && header.fields.system_title_length == 0x08 && header.fields.long_form_length_indicator == 0x82 &&
             header.fields.security_control_field == 0x30 && telegram_with_gcm_tag_length() > 25;
    }
  };

  class TelegramAccumulator : NonCopyableAndNonMovable {
    std::vector<uint8_t> buffer;
    std::size_t packetSize = 0;

  public:
    explicit TelegramAccumulator(std::size_t bufferSize) : buffer(bufferSize) {}

    void add(const uint8_t byte) {
      buffer[packetSize] = byte;
      packetSize++;
    }

    size_t number_of_accumulated_bytes() const { return packetSize; }
    size_t capacity() const { return buffer.size(); }

    // The tag is always last 12 bytes
    std::span<const uint8_t> telegram() const { return {buffer.data(), packetSize - 12}; }
    std::span<const uint8_t> tag() const { return {buffer.data() + packetSize - 12, 12}; }

    void reset() { packetSize = 0; }
  };

  class MbedTlsAes128GcmDecryptor : NonCopyableAndNonMovable {
    mbedtls_gcm_context gcm;

  public:
    MbedTlsAes128GcmDecryptor() { mbedtls_gcm_init(&gcm); }

    bool set_encryption_key(const std::span<const uint8_t> key) { return mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key.data(), 128) == 0; }

    bool decrypt(std::span<const uint8_t> iv, std::span<const uint8_t> ciphertext, std::span<const uint8_t> tag, std::vector<char>& decrypted_output) {
      // AdditionalAuthenticatedData = SecurityControlField + AuthenticationKey.
      //   SecurityControlField is always 0x30.
      //   AuthenticationKey = "00112233445566778899AABBCCDDEEFF". It is hardcoded and is the same for all DSMR devices.
      constexpr uint8_t aad[] = {0x30, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

      const auto& res = mbedtls_gcm_auth_decrypt(&gcm, ciphertext.size(), iv.data(), iv.size(), aad, std::size(aad), tag.data(), tag.size(), ciphertext.data(),
                                                 reinterpret_cast<unsigned char*>(decrypted_output.data()));
      return res == 0;
    }

    ~MbedTlsAes128GcmDecryptor() { mbedtls_gcm_free(&gcm); }
  };

  enum class State { WaitingForPacketStartSymbol, AccumulatingPacketHeader, AccumulatingTelegramWithGcmTag };
  State _state = State::WaitingForPacketStartSymbol;
  HeaderAccumulator _header_accumulator;
  TelegramAccumulator _encrypted_telegram_accumulator;
  std::vector<char> _decrypted_telegram_buf;
  std::array<uint8_t, 16> _encryption_key{};

public:
  enum class Error { BufferOverflow, HeaderCorrupted, FailedToSetEncryptionKey, DecryptionFailed };
  enum class SetEncryptionKeyError { EncryptionKeyLengthIsNot32Bytes, EncryptionKeyContainsNonHexSymbols };

  class Result {
    friend EncryptedPacketAccumulator;

    std::optional<std::string_view> _packet;
    std::optional<Error> _error;

    Result() = default;
    Result(std::string_view packet) : _packet(packet) {}
    Result(Error error) : _error(error) {}

  public:
    auto packet() const { return _packet; }
    auto error() const { return _error; }
  };

  explicit EncryptedPacketAccumulator(size_t bufferSize) : _encrypted_telegram_accumulator(bufferSize), _decrypted_telegram_buf(bufferSize) {}

  // key_hex is a string like "00112233445566778899AABBCCDDEEFF"
  std::optional<SetEncryptionKeyError> set_encryption_key(std::string_view key_hex) {
    if (key_hex.size() != 32) {
      return SetEncryptionKeyError::EncryptionKeyLengthIsNot32Bytes;
    }

    for (size_t i = 0; i < 16; ++i) {
      const auto hi = to_hex_value(key_hex[2 * i]);
      const auto lo = to_hex_value(key_hex[2 * i + 1]);
      if (!hi || !lo) {
        return SetEncryptionKeyError::EncryptionKeyContainsNonHexSymbols;
      }
      _encryption_key[i] = static_cast<uint8_t>((*hi << 4) | *lo);
    }

    return {};
  }

  Result process_byte(const uint8_t byte) {
    switch (_state) {
    case State::WaitingForPacketStartSymbol:
      if (byte == 0xDB) {
        _header_accumulator = HeaderAccumulator();
        _header_accumulator.add(byte);
        _encrypted_telegram_accumulator.reset();
        _state = State::AccumulatingPacketHeader;
      }
      return {};
    case State::AccumulatingPacketHeader:
      _header_accumulator.add(byte);
      if (!_header_accumulator.received_whole_header()) {
        return {};
      }

      if (!_header_accumulator.check_consistency()) {
        _state = State::WaitingForPacketStartSymbol;
        return Error::HeaderCorrupted;
      }

      if (_header_accumulator.telegram_with_gcm_tag_length() > static_cast<int>(_encrypted_telegram_accumulator.capacity())) {
        _state = State::WaitingForPacketStartSymbol;
        return Error::BufferOverflow;
      }

      _state = State::AccumulatingTelegramWithGcmTag;
      return {};
    case State::AccumulatingTelegramWithGcmTag:
      _encrypted_telegram_accumulator.add(byte);

      if (static_cast<int>(_encrypted_telegram_accumulator.number_of_accumulated_bytes()) != _header_accumulator.telegram_with_gcm_tag_length()) {
        return {};
      }

      _state = State::WaitingForPacketStartSymbol;

      MbedTlsAes128GcmDecryptor decryptor;

      if (!decryptor.set_encryption_key(_encryption_key)) {
        return Error::FailedToSetEncryptionKey;
      }

      if (!decryptor.decrypt(_header_accumulator.nonce(), _encrypted_telegram_accumulator.telegram(), _encrypted_telegram_accumulator.tag(),
                             _decrypted_telegram_buf)) {
        return Error::DecryptionFailed;
      }

      return std::string_view(_decrypted_telegram_buf.data(), _encrypted_telegram_accumulator.telegram().size());
    }

    // Unreachable
    return {};
  }

private:
  static std::optional<uint8_t> to_hex_value(const char c) {
    if (c >= '0' && c <= '9')
      return static_cast<uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f')
      return static_cast<uint8_t>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F')
      return static_cast<uint8_t>(c - 'A' + 10);
    return {};
  }
};

inline const char* to_string(const EncryptedPacketAccumulator::Error error) {
  switch (error) {
  case EncryptedPacketAccumulator::Error::BufferOverflow:
    return "BufferOverflow";
  case EncryptedPacketAccumulator::Error::HeaderCorrupted:
    return "HeaderCorrupted";
  case EncryptedPacketAccumulator::Error::FailedToSetEncryptionKey:
    return "FailedToSetEncryptionKey";
  case EncryptedPacketAccumulator::Error::DecryptionFailed:
    return "DecryptionFailed";
  }
  return "Unknown error";
}

inline const char* to_string(const EncryptedPacketAccumulator::SetEncryptionKeyError error) {
  switch (error) {
  case EncryptedPacketAccumulator::SetEncryptionKeyError::EncryptionKeyLengthIsNot32Bytes:
    return "EncryptionKeyLengthIsNot32Bytes";
  case EncryptedPacketAccumulator::SetEncryptionKeyError::EncryptionKeyContainsNonHexSymbols:
    return "EncryptionKeyContainsNonHexSymbols";
  }
  return "Unknown error";
}

}
