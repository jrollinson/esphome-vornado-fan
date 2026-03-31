#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/remote_base/symphony_protocol.h"
#include <vector>
#include <queue>
#include <string>

namespace esphome {
namespace vornado_controller {

static const char *const TAG = "vornado_controller";
static const size_t MAX_QUEUE_SIZE = 50;

// IR codes for Vornado fan (Symphony protocol, 12 bits)
static const uint32_t IR_CODE_POWER     = 0xD84;
static const uint32_t IR_CODE_DIRECTION = 0xD81;
static const uint32_t IR_CODE_INCREASE  = 0xDC6;
static const uint32_t IR_CODE_DECREASE  = 0xD82;
static const uint8_t  IR_NBITS          = 12;

// Button command enumeration
enum VornadoButton : int {
  POWER_ON = 0,         // Turns fan on (acts as wake command)
  POWER_OFF = 1,        // Turns fan off (requires wake if screen asleep)
  DIRECTION = 2,        // Toggles fan direction (requires wake if screen asleep)
  SPEED_INCREASE = 3,   // Increases fan speed (requires wake if screen asleep)
  SPEED_DECREASE = 4,   // Decreases fan speed (requires wake if screen asleep)
  ENSURE_ON = 5         // Special: Waits for screen sleep then powers on
};

// Command structure for queue
struct Command {
  int button_id;

  explicit Command(int id) : button_id(id) {}
};

class VornadoController : public Component {
 public:
  void setup() override {
    ESP_LOGI(TAG, "Vornado Controller initialized");
    if (!transmitter_) ESP_LOGW(TAG, "Transmitter not configured!");
  }

  float get_setup_priority() const override { return setup_priority::DATA; }

  void loop() override {
    uint32_t now = millis();

    // Safety: timeout if lock held too long
    if (is_locked_ && (now - lock_acquired_time_) > 30000) {
      ESP_LOGW(TAG, "Lock timeout! Forcing release after 30s - this indicates a bug");
      is_locked_ = false;
    }

    // Process next command if unlocked and queue has items
    if (!is_locked_ && !command_queue_.empty()) {
      is_locked_ = true;
      lock_acquired_time_ = now;

      Command cmd = command_queue_.front();
      command_queue_.pop();

      ESP_LOGI(TAG, "--- Processing: %s (button_id: %d) ---",
               get_button_name(cmd.button_id), cmd.button_id);

      dispatch_command(cmd.button_id);
    }
  }

  // Configuration setter
  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }

  void set_min_spacing_ms(uint32_t ms) { this->min_spacing_ms_ = ms; }
  void set_screen_timeout_ms(uint32_t ms) { this->screen_timeout_ms_ = ms; }
  void set_ensure_delay_ms(uint32_t ms) { this->ensure_delay_ms_ = ms; }

  // Main command interface - queued!
  void send_command(int button_id) {
    if (command_queue_.size() >= MAX_QUEUE_SIZE) {
      ESP_LOGW(TAG, "Command queue full (%d commands), dropping: %s",
               MAX_QUEUE_SIZE, get_button_name(button_id));
      return;
    }
    ESP_LOGI(TAG, "Queueing command: %s (button_id: %d)",
             get_button_name(button_id), button_id);
    command_queue_.push(Command(button_id));
    // loop() will process it automatically
  }

  // Command sequence interface - queued!
  void send_sequence(const std::vector<int> &commands) {
    if (commands.empty()) {
      ESP_LOGW(TAG, "Empty command sequence received");
      return;
    }

    if (commands.size() > MAX_QUEUE_SIZE) {
      ESP_LOGW(TAG, "Sequence too large (%d commands), max is %d",
               commands.size(), MAX_QUEUE_SIZE);
      return;
    }

    ESP_LOGI(TAG, "Queueing command sequence with %d commands", commands.size());

    // Add all commands to queue
    for (size_t i = 0; i < commands.size(); i++) {
      int button_id = commands[i];
      if (is_valid_button_id(button_id)) {
        ESP_LOGD(TAG, "  Queuing step %d: %s (button_id: %d)",
                 i + 1, get_button_name(button_id), button_id);
        command_queue_.push(Command(button_id));
      } else {
        ESP_LOGW(TAG, "  Skipping invalid button_id %d in sequence", button_id);
      }
    }
    // loop() will process them automatically
  }

  // Queue management utilities
  size_t queue_size() const { return command_queue_.size(); }
  bool is_processing() const { return is_locked_; }
  void clear_queue() {
    std::queue<Command> empty;
    std::swap(command_queue_, empty);
    ESP_LOGI(TAG, "Command queue cleared (%d commands dropped)", empty.size());
  }

 protected:
  // Transmitter reference
  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};

  // Timing configuration
  uint32_t min_spacing_ms_{400};
  uint32_t screen_timeout_ms_{10000};
  uint32_t ensure_delay_ms_{15000};

  // State tracking
  uint32_t last_command_time_{0};   // For screen sleep detection
  std::queue<Command> command_queue_;
  bool is_locked_{false};           // Processing lock
  uint32_t lock_acquired_time_{0};  // For timeout safety

  // ========================================
  // Helper Functions
  // ========================================

  // Validate button ID
  bool is_valid_button_id(int button_id) const {
    switch (button_id) {
      case VornadoButton::POWER_ON:
      case VornadoButton::POWER_OFF:
      case VornadoButton::DIRECTION:
      case VornadoButton::SPEED_INCREASE:
      case VornadoButton::SPEED_DECREASE:
      case VornadoButton::ENSURE_ON:
        return true;
      default:
        return false;
    }
  }

  // Transmit an IR code via Symphony protocol
  void transmit_ir(uint32_t data, uint8_t nbits) {
    if (this->transmitter_ == nullptr) {
      ESP_LOGW(TAG, "Cannot transmit: transmitter not configured");
      return;
    }
    auto call = this->transmitter_->transmit();
    remote_base::SymphonyData symphony{};
    symphony.data = data;
    symphony.nbits = nbits;
    remote_base::SymphonyProtocol().encode(call.get_data(), symphony);
    call.perform();
  }

  // Transmit the IR code for a given button ID (virtual for testing)
  virtual void transmit_for_button(int button_id) {
    switch (button_id) {
      case VornadoButton::POWER_ON:
      case VornadoButton::POWER_OFF:
        transmit_ir(IR_CODE_POWER, IR_NBITS);
        break;
      case VornadoButton::DIRECTION:
        transmit_ir(IR_CODE_DIRECTION, IR_NBITS);
        break;
      case VornadoButton::SPEED_INCREASE:
        transmit_ir(IR_CODE_INCREASE, IR_NBITS);
        break;
      case VornadoButton::SPEED_DECREASE:
        transmit_ir(IR_CODE_DECREASE, IR_NBITS);
        break;
      default:
        ESP_LOGW(TAG, "No IR code for button_id: %d", button_id);
        break;
    }
  }

  // Transmit and update last command time.
  // last_command_time_ is set BEFORE transmission so inter-command spacing is
  // measured from the start of transmission, not the end. This matches the
  // timing of the button-based approach where last_command_time_ is set before
  // the async IR automation fires.
  void press_and_track(int button_id) {
    last_command_time_ = millis();
    transmit_for_button(button_id);
  }

  // Release lock after spacing delay with optional message
  void release_lock_after_spacing(std::string message = "Complete") {
    this->set_timeout(min_spacing_ms_, [this, message]() {
      ESP_LOGI(TAG, "%s, releasing lock", message.c_str());
      is_locked_ = false;
    });
  }

  // Transmit, update time, and schedule lock release (common pattern)
  void press_and_finish(int button_id, std::string message = "Command complete") {
    press_and_track(button_id);
    release_lock_after_spacing(message);
  }

  // Calculate wait time for ensure_on command
  uint32_t calculate_ensure_on_wait() {
    uint32_t now = millis();

    if (last_command_time_ == 0) {
      if (now < ensure_delay_ms_) {
        uint32_t wait = ensure_delay_ms_ - now;
        ESP_LOGI(TAG, "First command since boot (at %u ms). Waiting %u ms more for screen to sleep",
                 now, wait);
        return wait;
      } else {
        ESP_LOGI(TAG, "First command since boot (at %u ms). Screen already asleep (threshold is %u ms)",
                 now, ensure_delay_ms_);
        return 0;
      }
    }

    uint32_t time_since = now - last_command_time_;
    if (time_since < ensure_delay_ms_) {
      uint32_t wait = ensure_delay_ms_ - time_since;
      ESP_LOGI(TAG, "Waiting additional %u ms for screen to sleep (already waited %u ms)",
               wait, time_since);
      return wait;
    }

    ESP_LOGI(TAG, "Screen already asleep (last command was %u ms ago, threshold is %u ms)",
             time_since, ensure_delay_ms_);
    return 0;
  }

  // Helper: Get button name for logging
  const char* get_button_name(int button_id) const {
    static constexpr const char* names[] = {
      "power on", "power off", "direction",
      "speed increase", "speed decrease", "ensure on"
    };
    if (is_valid_button_id(button_id)) {
      return names[button_id];
    }
    return "INVALID";
  }

  // Helper: Check if screen wake is needed
  bool needs_wake(int button_id) const {
    // Power on acts as its own wake command
    if (button_id == VornadoButton::POWER_ON) {
      return false;
    }

    // First command or timeout exceeded - screen is asleep
    if (this->last_command_time_ == 0) {
      return true;
    }

    uint32_t time_since = millis() - this->last_command_time_;
    return time_since > this->screen_timeout_ms_;
  }

  // Dispatch command to appropriate handler
  void dispatch_command(int button_id) {
    if (!is_valid_button_id(button_id)) {
      ESP_LOGE(TAG, "ERROR: Invalid button_id %d", button_id);
      is_locked_ = false;  // Release lock on error
      return;
    }

    uint32_t now = millis();
    if (this->last_command_time_ == 0) {
      ESP_LOGI(TAG, "This is the first command since boot");
    } else {
      uint32_t time_since = now - this->last_command_time_;
      ESP_LOGI(TAG, "Time since last command: %u ms (%.1f seconds)",
               time_since, time_since / 1000.0f);
    }

    // Route to appropriate handler
    if (button_id == VornadoButton::ENSURE_ON) {
      handle_ensure_on();
    } else if (needs_wake(button_id)) {
      handle_with_wake(button_id);
    } else {
      handle_normal(button_id);
    }
  }

  // Handler: Normal command (no wake needed)
  void handle_normal(int button_id) {
    ESP_LOGI(TAG, "Handling normal command: %s", get_button_name(button_id));
    press_and_finish(button_id, "Command complete");
  }

  // Handler: Command requiring wake
  void handle_with_wake(int button_id) {
    ESP_LOGI(TAG, "Handling with wake: %s", get_button_name(button_id));
    press_and_track(button_id);  // Wake transmission

    // Schedule actual command after spacing
    this->set_timeout(min_spacing_ms_, [this, button_id]() {
      ESP_LOGD(TAG, "Sending actual command after wake");
      press_and_finish(button_id, "Wake sequence complete");
    });
  }

  // Handler: Ensure On (wait for screen sleep, then power on)
  void handle_ensure_on() {
    ESP_LOGI(TAG, "Handling 'Ensure On' command");

    uint32_t wait_time = calculate_ensure_on_wait();

    auto execute_power_on = [this]() {
      ESP_LOGI(TAG, "Sending power command to turn fan ON");
      press_and_finish(VornadoButton::POWER_ON, "'Ensure On' complete");
    };

    if (wait_time > 0) {
      this->set_timeout(wait_time, execute_power_on);
    } else {
      execute_power_on();
    }
  }
};

// ============================================================================
// Action Classes
// ============================================================================

template<typename... Ts> class SendCommandAction : public Action<Ts...>, public Parented<VornadoController> {
 public:
  TEMPLATABLE_VALUE(int, button_id)

  void play(Ts... x) override {
    int button_id = this->button_id_.value(x...);
    this->parent_->send_command(button_id);
  }
};

template<typename... Ts> class SendSequenceAction : public Action<Ts...>, public Parented<VornadoController> {
 public:
  void set_commands(const std::vector<int> &commands) { this->commands_ = commands; }

  void play(Ts... x) override {
    this->parent_->send_sequence(this->commands_);
  }

 protected:
  std::vector<int> commands_;
};

}  // namespace vornado_controller
}  // namespace esphome
