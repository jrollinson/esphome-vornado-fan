#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/button/button.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../vornado_controller/vornado_controller.h"
#include <string>

namespace esphome {
namespace vornado_controller {

static const char *const STATEFUL_TAG = "vornado_stateful";

// Power state enumeration
enum PowerState : uint8_t {
  STATE_POWER_UNKNOWN = 0,
  STATE_POWER_OFF = 1,
  STATE_POWER_ON = 2
};

// Speed state enumeration (1-4 are the actual speeds)
enum SpeedState : uint8_t {
  STATE_SPEED_UNKNOWN = 0,
  STATE_SPEED_1 = 1,
  STATE_SPEED_2 = 2,
  STATE_SPEED_3 = 3,
  STATE_SPEED_4 = 4
};

// Constants for fan capabilities
static const uint8_t MIN_FAN_SPEED = 1;
static const uint8_t MAX_FAN_SPEED = 4;

class VornadoStatefulController : public Component {
 public:
  void setup() override {
    ESP_LOGI(STATEFUL_TAG, "Vornado Stateful Controller initialized");
    ESP_LOGI(STATEFUL_TAG, "Initial state: Power=%s, Speed=%s", 
             power_state_to_string(power_state_), 
             speed_state_to_string(speed_state_));
    publish_state();
  }

  float get_setup_priority() const override { return setup_priority::DATA; }

  void loop() override {
    // No loop logic needed - we delegate to underlying controller
  }

  // Configuration
  void set_controller(VornadoController *controller) { this->controller_ = controller; }
  void set_power_state_sensor(text_sensor::TextSensor *sensor) { this->power_state_sensor_ = sensor; }
  void set_speed_state_sensor(sensor::Sensor *sensor) { this->speed_state_sensor_ = sensor; }

  // ========================================
  // Public API - Button Actions
  // ========================================

  // Turn on (defaults to speed 2 if speed unknown)
  void turn_on() {
    ESP_LOGI(STATEFUL_TAG, "Turn ON requested");
    ensure_known_state();
    
    if (power_state_ != STATE_POWER_ON) {
      send_command_and_update_state(VornadoButton::POWER_ON);
    }
    
    // If speed is unknown, default to speed 2
    if (speed_state_ == STATE_SPEED_UNKNOWN) {
      set_speed_internal(2);
    }
  }

  // Turn off
  void turn_off() {
    ESP_LOGI(STATEFUL_TAG, "Turn OFF requested");
    ensure_known_state();
    
    if (power_state_ != STATE_POWER_OFF) {
      send_command_and_update_state(VornadoButton::POWER_OFF);
    }
  }

  // Set specific speed (1-4)
  void set_speed(uint8_t target_speed) {
    if (target_speed < MIN_FAN_SPEED || target_speed > MAX_FAN_SPEED) {
      ESP_LOGW(STATEFUL_TAG, "Invalid speed %d, must be %d-%d", 
               target_speed, MIN_FAN_SPEED, MAX_FAN_SPEED);
      return;
    }

    ESP_LOGI(STATEFUL_TAG, "Set speed to %d requested", target_speed);
    ensure_known_state();
    
    // Ensure device is powered on before changing speed
    if (power_state_ == STATE_POWER_OFF) {
      ESP_LOGI(STATEFUL_TAG, "Powering on before speed change");
      send_command_and_update_state(VornadoButton::POWER_ON);
    }
    
    set_speed_internal(target_speed);
  }

  // Direction control
  void toggle_direction() {
    ESP_LOGI(STATEFUL_TAG, "Direction TOGGLE requested");
    ensure_known_state();
    send_command_and_update_state(VornadoButton::DIRECTION);
  }

  // State management
  void reset_to_known_state() {
    ESP_LOGI(STATEFUL_TAG, "Resetting to known state...");
    
    // Strategy: Ensure ON, then decrease to speed 1
    std::vector<int> reset_sequence = {
      VornadoButton::ENSURE_ON,
      VornadoButton::SPEED_DECREASE,
      VornadoButton::SPEED_DECREASE,
      VornadoButton::SPEED_DECREASE
    };
    
    controller_->send_sequence(reset_sequence);
    
    // Set known state (optimistic)
    power_state_ = STATE_POWER_ON;
    speed_state_ = STATE_SPEED_1;
    
    ESP_LOGI(STATEFUL_TAG, "Reset commands queued. Target state: Power=ON, Speed=1");
    publish_state();
  }

  // State queries
  PowerState get_power_state() const { return power_state_; }
  SpeedState get_speed_state() const { return speed_state_; }
  uint8_t get_speed() const { return static_cast<uint8_t>(speed_state_); }
  bool is_state_known() const { 
    return power_state_ != STATE_POWER_UNKNOWN && speed_state_ != STATE_SPEED_UNKNOWN; 
  }

 protected:
  // Component references
  VornadoController *controller_{nullptr};
  text_sensor::TextSensor *power_state_sensor_{nullptr};
  sensor::Sensor *speed_state_sensor_{nullptr};

  // State tracking
  PowerState power_state_{STATE_POWER_UNKNOWN};
  SpeedState speed_state_{STATE_SPEED_UNKNOWN};

  // ========================================
  // Helper Functions
  // ========================================

  // Ensure state is known (resets if needed)
  void ensure_known_state() {
    if (!is_state_known()) {
      ESP_LOGW(STATEFUL_TAG, "State unknown, resetting to known state first");
      reset_to_known_state();
    }
  }

  // Internal speed setting logic
  void set_speed_internal(uint8_t target_speed) {
    uint8_t current = static_cast<uint8_t>(speed_state_);
    
    if (current == target_speed) {
      ESP_LOGI(STATEFUL_TAG, "Already at speed %d, skipping", target_speed);
      return;
    }

    ESP_LOGI(STATEFUL_TAG, "Changing speed from %d to %d", current, target_speed);
    
    // Calculate button sequence
    std::vector<int> sequence;
    
    if (target_speed > current) {
      int increases_needed = target_speed - current;
      ESP_LOGD(STATEFUL_TAG, "Increasing speed %d times", increases_needed);
      for (int i = 0; i < increases_needed; i++) {
        sequence.push_back(VornadoButton::SPEED_INCREASE);
      }
    } else {
      int decreases_needed = current - target_speed;
      ESP_LOGD(STATEFUL_TAG, "Decreasing speed %d times", decreases_needed);
      for (int i = 0; i < decreases_needed; i++) {
        sequence.push_back(VornadoButton::SPEED_DECREASE);
      }
    }

    // Send sequence and update state
    controller_->send_sequence(sequence);
    speed_state_ = static_cast<SpeedState>(target_speed);
    publish_state();
  }

  // Send command and update internal state
  void send_command_and_update_state(int button_id) {
    controller_->send_command(button_id);
    update_state_for_command(button_id);
    publish_state();
  }

  // Update state based on command sent
  void update_state_for_command(int button_id) {
    switch (button_id) {
      case VornadoButton::POWER_ON:
        power_state_ = STATE_POWER_ON;
        ESP_LOGD(STATEFUL_TAG, "State updated: Power=ON");
        break;

      case VornadoButton::POWER_OFF:
        power_state_ = STATE_POWER_OFF;
        ESP_LOGD(STATEFUL_TAG, "State updated: Power=OFF");
        break;

      case VornadoButton::SPEED_INCREASE:
        if (speed_state_ != STATE_SPEED_UNKNOWN && speed_state_ < MAX_FAN_SPEED) {
          speed_state_ = static_cast<SpeedState>(static_cast<uint8_t>(speed_state_) + 1);
          ESP_LOGD(STATEFUL_TAG, "State updated: Speed=%d", static_cast<uint8_t>(speed_state_));
        }
        break;

      case VornadoButton::SPEED_DECREASE:
        if (speed_state_ != STATE_SPEED_UNKNOWN && speed_state_ > MIN_FAN_SPEED) {
          speed_state_ = static_cast<SpeedState>(static_cast<uint8_t>(speed_state_) - 1);
          ESP_LOGD(STATEFUL_TAG, "State updated: Speed=%d", static_cast<uint8_t>(speed_state_));
        }
        break;

      case VornadoButton::DIRECTION:
        ESP_LOGD(STATEFUL_TAG, "Direction toggled");
        break;

      case VornadoButton::ENSURE_ON:
        power_state_ = STATE_POWER_ON;
        ESP_LOGD(STATEFUL_TAG, "State updated: Power=ON (via ENSURE_ON)");
        break;

      default:
        ESP_LOGW(STATEFUL_TAG, "Unknown button_id: %d", button_id);
        break;
    }
  }

  // Publish state to sensors
  void publish_state() {
    if (power_state_sensor_ != nullptr) {
      power_state_sensor_->publish_state(power_state_to_string(power_state_));
    }
    if (speed_state_sensor_ != nullptr) {
      if (speed_state_ == STATE_SPEED_UNKNOWN) {
        speed_state_sensor_->publish_state(NAN);
      } else {
        speed_state_sensor_->publish_state(static_cast<float>(speed_state_));
      }
    }
  }

  // Convert state enums to strings
  const char* power_state_to_string(PowerState state) const {
    switch (state) {
      case STATE_POWER_UNKNOWN: return "Unknown";
      case STATE_POWER_OFF: return "Off";
      case STATE_POWER_ON: return "On";
      default: return "Invalid";
    }
  }

  const char* speed_state_to_string(SpeedState state) const {
    static char buffer[16];
    if (state == STATE_SPEED_UNKNOWN) {
      return "Unknown";
    }
    snprintf(buffer, sizeof(buffer), "%d", static_cast<uint8_t>(state));
    return buffer;
  }
};

// ============================================================================
// Action Classes
// ============================================================================

template<typename... Ts> class TurnOnAction : public Action<Ts...>, public Parented<VornadoStatefulController> {
 public:
  void play(Ts... x) override { this->parent_->turn_on(); }
};

template<typename... Ts> class TurnOffAction : public Action<Ts...>, public Parented<VornadoStatefulController> {
 public:
  void play(Ts... x) override { this->parent_->turn_off(); }
};

template<typename... Ts> class SetSpeedAction : public Action<Ts...>, public Parented<VornadoStatefulController> {
 public:
  TEMPLATABLE_VALUE(uint8_t, speed)

  void play(Ts... x) override {
    auto speed_val = this->speed_.value(x...);
    this->parent_->set_speed(speed_val);
  }
};

template<typename... Ts> class ToggleDirectionAction : public Action<Ts...>, public Parented<VornadoStatefulController> {
 public:
  void play(Ts... x) override { this->parent_->toggle_direction(); }
};

template<typename... Ts> class ResetStateAction : public Action<Ts...>, public Parented<VornadoStatefulController> {
 public:
  void play(Ts... x) override { this->parent_->reset_to_known_state(); }
};

// ============================================================================
// Auto-generated Button Support
// ============================================================================

enum VornadoButtonAction : uint8_t {
  BTN_TURN_ON   = 0,
  BTN_TURN_OFF  = 1,
  BTN_SPEED_1   = 2,
  BTN_SPEED_2   = 3,
  BTN_SPEED_3   = 4,
  BTN_SPEED_4   = 5,
  BTN_DIRECTION = 6,
  BTN_RESET     = 7,
};

class VornadoActionButton : public button::Button, public Parented<VornadoStatefulController> {
 public:
  void set_action(int action) { action_ = static_cast<VornadoButtonAction>(action); }

 protected:
  void press_action() override {
    switch (action_) {
      case BTN_TURN_ON:   this->parent_->turn_on();        break;
      case BTN_TURN_OFF:  this->parent_->turn_off();       break;
      case BTN_SPEED_1:   this->parent_->set_speed(1);     break;
      case BTN_SPEED_2:   this->parent_->set_speed(2);     break;
      case BTN_SPEED_3:   this->parent_->set_speed(3);     break;
      case BTN_SPEED_4:   this->parent_->set_speed(4);     break;
      case BTN_DIRECTION: this->parent_->toggle_direction(); break;
      case BTN_RESET:     this->parent_->reset_to_known_state(); break;
    }
  }

 private:
  VornadoButtonAction action_{BTN_TURN_ON};
};

}  // namespace vornado_controller
}  // namespace esphome
