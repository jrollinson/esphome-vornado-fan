#pragma once
#include "esphome/core/component.h"

namespace esphome {
namespace vornado_ir {

class VornadoIR : public Component {
 public:
  void send_power_toggle() {}
  void send_change_direction() {}
  void send_increase() {}
  void send_decrease() {}
};

}  // namespace vornado_ir
}  // namespace esphome
