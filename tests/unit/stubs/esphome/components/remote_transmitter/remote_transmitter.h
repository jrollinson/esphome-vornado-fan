#pragma once
#include <cstdint>
#include <vector>

namespace esphome {
namespace remote_base {
class RemoteTransmitData {
 public:
  void set_carrier_frequency(uint32_t) {}
  void mark(uint32_t) {}
  void space(uint32_t) {}
};
}  // namespace remote_base

namespace remote_transmitter {

class TransmitCall {
 public:
  remote_base::RemoteTransmitData *get_data() { return &data_; }
  void set_send_times(uint32_t) {}
  void set_send_wait(uint32_t) {}
  // perform() is the method we care about in tests — track calls via override
  virtual void perform() {}

 private:
  remote_base::RemoteTransmitData data_;
};

class RemoteTransmitterComponent {
 public:
  TransmitCall transmit() { return TransmitCall{}; }
};

}  // namespace remote_transmitter
}  // namespace esphome
