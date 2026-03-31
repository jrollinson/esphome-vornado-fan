#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include <cstdio>

// Controllable fake clock — set this in tests to control millis()
extern uint32_t fake_millis;
inline uint32_t millis() { return fake_millis; }

// Logging macros — print to stdout so test failures are visible
#define ESP_LOGI(tag, fmt, ...) printf("[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) printf("[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) // suppress debug noise

namespace esphome {

namespace setup_priority {
  static const float DATA = 600.0f;
}

// Global timeout queue — flush in tests to fire deferred callbacks
struct PendingTimeout {
  uint32_t delay_ms;
  std::function<void()> callback;
};
extern std::vector<PendingTimeout> pending_timeouts;

inline void flush_timeouts() {
  while (!pending_timeouts.empty()) {
    auto cb = std::move(pending_timeouts.front().callback);
    pending_timeouts.erase(pending_timeouts.begin());
    cb();
  }
}

class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void loop() {}
  virtual float get_setup_priority() const { return setup_priority::DATA; }

  template<typename F>
  void set_timeout(uint32_t delay_ms, F &&callback) {
    pending_timeouts.push_back({delay_ms, std::forward<F>(callback)});
  }

  template<typename F>
  void set_timeout(const char* /*name*/, uint32_t delay_ms, F &&callback) {
    pending_timeouts.push_back({delay_ms, std::forward<F>(callback)});
  }
};

}  // namespace esphome
