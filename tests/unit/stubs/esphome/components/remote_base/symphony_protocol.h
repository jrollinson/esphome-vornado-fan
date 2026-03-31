#pragma once
#include <cstdint>
#include "esphome/components/remote_transmitter/remote_transmitter.h"

namespace esphome {
namespace remote_base {

struct SymphonyData {
  uint32_t data{0};
  uint8_t nbits{12};
};

class SymphonyProtocol {
 public:
  void encode(RemoteTransmitData * /*dst*/, const SymphonyData & /*src*/) {}
};

}  // namespace remote_base
}  // namespace esphome
