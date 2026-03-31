#include <gtest/gtest.h>
#include "vornado_controller/vornado_controller.h"

using namespace esphome::vornado_controller;

// ============================================================================
// Test infrastructure definitions
// ============================================================================

uint32_t fake_millis = 0;
std::vector<esphome::PendingTimeout> esphome::pending_timeouts;

// ============================================================================
// Test doubles
// ============================================================================

// Overrides transmit_for_button() so tests never touch real IR hardware.
// Captures the value of last_command_time_ at the moment transmission is called
// — this is the key observable for the ordering fix.
class TestableController : public VornadoController {
 public:
  int transmit_count{0};
  int last_button_transmitted{-1};
  // Value of last_command_time_ at the moment transmit_for_button() was called
  uint32_t last_command_time_at_transmit{UINT32_MAX};

  // Expose protected members for direct inspection
  using VornadoController::last_command_time_;
  using VornadoController::is_locked_;
  using VornadoController::needs_wake;
  using VornadoController::press_and_track;
  using VornadoController::handle_with_wake;
  using VornadoController::handle_normal;

 protected:
  void transmit_for_button(int button_id) override {
    transmit_count++;
    last_button_transmitted = button_id;
    last_command_time_at_transmit = this->last_command_time_;
  }
};

// ============================================================================
// Test fixture
// ============================================================================

class VornadoControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fake_millis = 0;
    esphome::pending_timeouts.clear();
    ctrl.transmit_count = 0;
    ctrl.last_button_transmitted = -1;
    ctrl.last_command_time_at_transmit = UINT32_MAX;
    ctrl.set_min_spacing_ms(400);
    ctrl.set_screen_timeout_ms(10000);
    ctrl.set_ensure_delay_ms(15000);
  }

  TestableController ctrl;
};

// ============================================================================
// Tests: needs_wake() logic
// ============================================================================

TEST_F(VornadoControllerTest, NeedsWake_NoHistory_ReturnsTrue) {
  EXPECT_TRUE(ctrl.needs_wake(VornadoButton::SPEED_INCREASE));
}

TEST_F(VornadoControllerTest, NeedsWake_PowerOn_NeverNeeds) {
  // POWER_ON acts as its own wake — should never need a preceding wake
  EXPECT_FALSE(ctrl.needs_wake(VornadoButton::POWER_ON));
}

TEST_F(VornadoControllerTest, NeedsWake_WithinTimeout_ReturnsFalse) {
  ctrl.last_command_time_ = 1000;
  fake_millis = 5000;  // 4s since last command, timeout is 10s
  EXPECT_FALSE(ctrl.needs_wake(VornadoButton::SPEED_INCREASE));
}

TEST_F(VornadoControllerTest, NeedsWake_ExactlyAtTimeout_ReturnsFalse) {
  // needs_wake uses >, not >=, so exactly at boundary returns false
  ctrl.last_command_time_ = 1000;
  fake_millis = 11000;  // exactly 10000ms since last command
  EXPECT_FALSE(ctrl.needs_wake(VornadoButton::SPEED_INCREASE));
}

TEST_F(VornadoControllerTest, NeedsWake_AfterTimeout_ReturnsTrue) {
  ctrl.last_command_time_ = 1000;
  fake_millis = 12000;  // 11s since last command, > 10s timeout
  EXPECT_TRUE(ctrl.needs_wake(VornadoButton::SPEED_INCREASE));
}

// ============================================================================
// Tests: press_and_track() timing contract
//
// THE KEY ORDERING TEST: last_command_time_ must be set BEFORE transmit fires.
// This mirrors the button-based branch timing where last_command_time_ is set
// before the async IR automation fires.
// ============================================================================

TEST_F(VornadoControllerTest, PressAndTrack_LastCommandTimeSetBeforeTransmit) {
  fake_millis = 5000;
  ctrl.press_and_track(VornadoButton::POWER_ON);
  // last_command_time_ must already be set when transmit_for_button() is called
  EXPECT_EQ(ctrl.last_command_time_at_transmit, 5000u)
      << "last_command_time_ must be set BEFORE transmission, not after";
}

TEST_F(VornadoControllerTest, PressAndTrack_SetsLastCommandTime) {
  fake_millis = 5000;
  ctrl.press_and_track(VornadoButton::POWER_ON);
  EXPECT_EQ(ctrl.last_command_time_, 5000u);
}

TEST_F(VornadoControllerTest, PressAndTrack_UpdatesLastCommandTimeOnRepeat) {
  fake_millis = 1000;
  ctrl.press_and_track(VornadoButton::POWER_ON);
  fake_millis = 2000;
  ctrl.press_and_track(VornadoButton::SPEED_INCREASE);
  EXPECT_EQ(ctrl.last_command_time_, 2000u);
}

TEST_F(VornadoControllerTest, PressAndTrack_TransmitsPowerCode) {
  ctrl.press_and_track(VornadoButton::POWER_ON);
  EXPECT_EQ(ctrl.transmit_count, 1);
  EXPECT_EQ(ctrl.last_button_transmitted, VornadoButton::POWER_ON);
}

TEST_F(VornadoControllerTest, PressAndTrack_TransmitsIncreaseCode) {
  ctrl.press_and_track(VornadoButton::SPEED_INCREASE);
  EXPECT_EQ(ctrl.last_button_transmitted, VornadoButton::SPEED_INCREASE);
}

TEST_F(VornadoControllerTest, PressAndTrack_TransmitsDecreaseCode) {
  ctrl.press_and_track(VornadoButton::SPEED_DECREASE);
  EXPECT_EQ(ctrl.last_button_transmitted, VornadoButton::SPEED_DECREASE);
}

TEST_F(VornadoControllerTest, PressAndTrack_TransmitsDirectionCode) {
  ctrl.press_and_track(VornadoButton::DIRECTION);
  EXPECT_EQ(ctrl.last_button_transmitted, VornadoButton::DIRECTION);
}

// ============================================================================
// Tests: handle_with_wake() — wake-then-command sequence
// ============================================================================

TEST_F(VornadoControllerTest, HandleWithWake_TransmitsImmediatelyForWake) {
  fake_millis = 0;
  ctrl.handle_with_wake(VornadoButton::SPEED_INCREASE);
  EXPECT_EQ(ctrl.transmit_count, 1);
}

TEST_F(VornadoControllerTest, HandleWithWake_SetsLastCommandTimeAfterWake) {
  fake_millis = 1000;
  ctrl.handle_with_wake(VornadoButton::SPEED_INCREASE);
  EXPECT_EQ(ctrl.last_command_time_, 1000u);
}

TEST_F(VornadoControllerTest, HandleWithWake_DoesNotTransmitActualBeforeTimeout) {
  fake_millis = 0;
  ctrl.handle_with_wake(VornadoButton::SPEED_INCREASE);
  // Without flushing timeouts, only the wake transmission has fired
  EXPECT_EQ(ctrl.transmit_count, 1);
}

TEST_F(VornadoControllerTest, HandleWithWake_TransmitsActualCommandAfterTimeout) {
  fake_millis = 0;
  ctrl.handle_with_wake(VornadoButton::SPEED_INCREASE);
  EXPECT_EQ(ctrl.transmit_count, 1);

  fake_millis = 400;
  esphome::flush_timeouts();
  EXPECT_EQ(ctrl.transmit_count, 2);
  EXPECT_EQ(ctrl.last_button_transmitted, VornadoButton::SPEED_INCREASE);
}

TEST_F(VornadoControllerTest, HandleWithWake_ActualCommandAlsoSetsTimeBeforeTransmit) {
  fake_millis = 1000;
  ctrl.handle_with_wake(VornadoButton::SPEED_INCREASE);

  fake_millis = 1400;
  ctrl.last_command_time_at_transmit = UINT32_MAX;  // reset for second call
  esphome::flush_timeouts();
  // last_command_time_ must be set before the actual command transmits too
  EXPECT_EQ(ctrl.last_command_time_at_transmit, 1400u)
      << "last_command_time_ must be set BEFORE transmission in wake sequence too";
}

TEST_F(VornadoControllerTest, HandleWithWake_UpdatesLastCommandTimeForActualCommand) {
  fake_millis = 1000;
  ctrl.handle_with_wake(VornadoButton::SPEED_INCREASE);
  EXPECT_EQ(ctrl.last_command_time_, 1000u);

  fake_millis = 1400;
  esphome::flush_timeouts();
  EXPECT_EQ(ctrl.last_command_time_, 1400u);
}

// ============================================================================
// Tests: handle_normal() — no wake needed
// ============================================================================

TEST_F(VornadoControllerTest, HandleNormal_TransmitsOnce) {
  ctrl.handle_normal(VornadoButton::SPEED_INCREASE);
  EXPECT_EQ(ctrl.transmit_count, 1);
}

TEST_F(VornadoControllerTest, HandleNormal_SetsLastCommandTime) {
  fake_millis = 5000;
  ctrl.handle_normal(VornadoButton::SPEED_INCREASE);
  EXPECT_EQ(ctrl.last_command_time_, 5000u);
}
