#pragma once

namespace esphome {

template<typename... Ts>
class Action {};

template<typename T>
class Parented {
 public:
  void set_parent(T *parent) { parent_ = parent; }
 protected:
  T *parent_{nullptr};
};

// Stub for TEMPLATABLE_VALUE — used in action classes
template<typename T, typename... Ts>
struct TemplatableValue {
  T value_{};
  T value(Ts...) const { return value_; }
  TemplatableValue() = default;
  TemplatableValue(T v) : value_(v) {}
};

#define TEMPLATABLE_VALUE(T, name) \
  esphome::TemplatableValue<T, Ts...> name##_{}; \
  void set_##name(esphome::TemplatableValue<T, Ts...> v) { this->name##_ = v; }

}  // namespace esphome
