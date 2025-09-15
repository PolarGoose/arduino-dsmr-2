#pragma once
#include "util.h"
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace dsmr {

class DsmrPacketBuffer : NonCopyable {
  friend class PacketAccumulator;

  std::vector<char> buffer;
  std::size_t packetSize = 0;

  explicit DsmrPacketBuffer(std::size_t bufferSize) : buffer(bufferSize) {}

  std::string_view packet() const { return std::string_view(buffer.data(), packetSize); }

  void add(char byte) {
    buffer[packetSize] = byte;
    packetSize++;
  }

  bool has_space() const { return packetSize < buffer.size(); }

  void reset() { packetSize = 0; }

  uint16_t calculate_crc16() const {
    uint16_t crc = 0;
    for (std::size_t i = 0; i < packetSize; ++i) {
      crc ^= static_cast<uint8_t>(buffer[i]);
      for (std::size_t bit = 0; bit < 8; bit++) {
        if (crc & 1)
          crc = (crc >> 1) ^ 0xa001;
        else
          crc = (crc >> 1);
      }
    }
    return crc;
  }
};

class CrcAccumulator {
  friend class PacketAccumulator;

  uint16_t crc = 0;
  size_t amount_of_crc_nibbles = 0;

  bool add_to_crc(char byte) {
    if (byte >= '0' && byte <= '9') {
      byte = byte - '0';
    } else if (byte >= 'A' && byte <= 'F') {
      byte = static_cast<char>(byte - 'A' + 10);
    } else if (byte >= 'a' && byte <= 'f') {
      byte = static_cast<char>(byte - 'a' + 10);
    } else {
      return false;
    }

    crc = static_cast<uint16_t>((crc << 4) | (byte & 0xF));
    amount_of_crc_nibbles++;
    return true;
  }

  bool has_full_crc() const { return amount_of_crc_nibbles == 4; }
};

enum class PacketAccumulatorError {
  BufferOverflow,
  PacketStartSymbolInPacket,
  IncorrectCrcCharacter,
  CrcMismatch,
};

inline const char* to_string(const PacketAccumulatorError error) {
  switch (error) {
  case PacketAccumulatorError::BufferOverflow:
    return "BufferOverflow";
  case PacketAccumulatorError::PacketStartSymbolInPacket:
    return "PacketStartSymbolInPacket";
  case PacketAccumulatorError::IncorrectCrcCharacter:
    return "IncorrectCrcCharacter";
  case PacketAccumulatorError::CrcMismatch:
    return "CrcMismatch";
  }
  
  return "Unknown error";
}

class PacketAccumulatorResult {
  friend class PacketAccumulator;

  std::optional<std::string_view> _packet;
  std::optional<PacketAccumulatorError> _error;

  PacketAccumulatorResult() = default;
  PacketAccumulatorResult(std::string_view packet) : _packet(packet) {}
  PacketAccumulatorResult(PacketAccumulatorError error) : _error(error) {}

public:
  auto packet() const { return _packet; }
  auto error() const { return _error; }
};

class PacketAccumulator : NonCopyable {
  enum class State { WaitingForPacketStartSymbol, WaitingForPacketEndSymbol, WaitingForCrc };
  State _state = State::WaitingForPacketStartSymbol;
  DsmrPacketBuffer _buf;
  CrcAccumulator _crc_accumulator;
  bool _check_crc;

public:
  PacketAccumulator(size_t bufferSize, bool check_crc) : _buf(bufferSize), _check_crc(check_crc) {}

  PacketAccumulatorResult process_byte(const char byte) {
    if (!_buf.has_space()) {
      _buf.reset();
      _state = State::WaitingForPacketStartSymbol;
      if (byte != '/') {
        return PacketAccumulatorError::BufferOverflow;
      }
    }

    if (byte == '/') {
      _buf.reset();
      _buf.add(byte);
      const auto prev_state = _state;
      _state = State::WaitingForPacketEndSymbol;

      if (prev_state == State::WaitingForPacketEndSymbol || prev_state == State::WaitingForCrc) {
        return PacketAccumulatorError::PacketStartSymbolInPacket;
      }
      return {};
    }

    switch (_state) {
    case State::WaitingForPacketStartSymbol:
      return {};

    case State::WaitingForPacketEndSymbol:
      _buf.add(byte);

      if (byte != '!') {
        return {};
      }

      if (!_check_crc) {
        _state = State::WaitingForPacketStartSymbol;
        return PacketAccumulatorResult(_buf.packet());
      }

      _state = State::WaitingForCrc;
      _crc_accumulator = CrcAccumulator();
      return {};

    case State::WaitingForCrc:
      if (!_crc_accumulator.add_to_crc(byte)) {
        _state = State::WaitingForPacketStartSymbol;
        return PacketAccumulatorError::IncorrectCrcCharacter;
      }

      if (!_crc_accumulator.has_full_crc()) {
        return {};
      }

      _state = State::WaitingForPacketStartSymbol;

      if (_crc_accumulator.crc == _buf.calculate_crc16()) {
        return _buf.packet();
      }

      return PacketAccumulatorError::CrcMismatch;
    }

    // unreachable
    return {};
  }
};

}
