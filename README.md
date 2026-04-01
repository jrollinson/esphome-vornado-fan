# ESPHome Vornado Transom Fan Controller

ESPHome component for controlling a Vornado Transom fan via IR, with state tracking and Home Assistant integration.

## Usage

Copy [`examples/d1_mini_vornado_fan.yaml`](examples/d1_mini_vornado_fan.yaml) and adapt it for your device. The core config looks like this:

```yaml
external_components:
  - source: github://jrollinson/esphome-vornado-fan@main
    components: [ vornado_controller, vornado_stateful_controller ]

packages:
  vornado_buttons: github://jrollinson/esphome-vornado-fan/buttons.yaml@main

remote_transmitter:
  id: ir_tx
  pin: GPIO4  # GPIO pin connected to your IR LED
  carrier_duty_percent: 50%

vornado_controller:
  id: vornado_ctrl
  transmitter_id: ir_tx

vornado_stateful_controller:
  id: vornado_stateful
  name: "Vornado Fan"
  controller: vornado_ctrl
```

`buttons.yaml` adds Turn On, Turn Off, Speed 1–4, Direction, and Reset State buttons. To customize, omit the package and define your own using the `vornado_stateful.*` actions:

```yaml
button:
  - platform: template
    name: "Fan Turn On"
    on_press:
      - vornado_stateful.turn_on:

  - platform: template
    name: "Fan Speed 2"
    on_press:
      - vornado_stateful.set_speed:
          speed: 2
```

## Components

### `vornado_controller` — IR delivery

Handles the low-level work of getting IR commands to the physical fan reliably.

- **Command queue** — commands are queued (up to 50) and sent one at a time with a minimum 400ms gap, so rapid button presses never confuse the fan
- **Screen wake** — the fan's display sleeps after ~10 seconds of inactivity; when that happens, the first IR command only wakes the screen without acting. `vornado_controller` detects this and automatically sends the command twice: once to wake, once to act
- **`ENSURE_ON`** — waits for the display to go to sleep, then sends `POWER_ON`. Because `POWER_ON` always turns the fan on from a sleeping display, this guarantees the fan ends up on regardless of prior state

**Actions:**
- `vornado_controller.send_command` — send a single command: `POWER_ON`, `POWER_OFF`, `DIRECTION`, `SPEED_INCREASE`, `SPEED_DECREASE`, `ENSURE_ON`
- `vornado_controller.send_sequence` — send a list of commands

### `vornado_stateful_controller` — State tracking and smart commands

Sits on top of `vornado_controller` and tracks the fan's logical state in software (the fan has no feedback mechanism). Exposes sensors to Home Assistant and actions for use in buttons and automations.

- **State tracking** — tracks `PowerState` (Unknown/Off/On) and `SpeedState` (Unknown/1–4), updated optimistically each time a command is sent
- **Smart `set_speed`** — calculates the minimum number of increase/decrease presses to reach the target from the current speed; skips no-ops
- **Auto-reset** — if state is unknown when an action is called, automatically runs `reset_to_known_state()` first: sends `ENSURE_ON` then 3× `SPEED_DECREASE`, which always lands at speed 1 regardless of starting position

**Sensors (auto-created):**
- Power State — text sensor: `Unknown` / `Off` / `On`
- Speed — numeric sensor: `NaN` or `1`–`4`

**Actions:**
- `vornado_stateful.turn_on` — powers on (defaults to speed 2 if speed unknown)
- `vornado_stateful.turn_off`
- `vornado_stateful.set_speed` — set speed 1–4 (powers on first if off)
- `vornado_stateful.toggle_direction`
- `vornado_stateful.reset_state` — reset to known state (on, speed 1)

## Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for component internals and IR codes.
