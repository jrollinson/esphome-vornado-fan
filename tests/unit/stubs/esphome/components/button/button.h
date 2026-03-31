#pragma once

namespace esphome {
namespace button {

class Button {
 public:
  virtual ~Button() = default;
  void press() { press_action(); }
 protected:
  virtual void press_action() {}
};

}  // namespace button
}  // namespace esphome
