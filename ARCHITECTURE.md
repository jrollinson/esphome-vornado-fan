# Architecture

## Overview

```
Home Assistant
     │  (button press)
     ▼
Template Buttons              vornado-fan.yaml
(turn_on, turn_off,           User-facing HA entities. Each button press
 speed 1-4, direction,        invokes a vornado_stateful.* action.
 reset_state)
     │
     ▼
VornadoStatefulController     vornado_stateful_controller/vornado_stateful_controller.h
                              Tracks logical state (power, speed).
                              Decides what commands to send:
                              - Skips no-op commands
                              - Calculates minimal speed adjustments
                              - Resets to known state if needed
                              - Updates state optimistically after sending
     │  (send_command / send_sequence)
     ▼
VornadoController             vornado_controller/vornado_controller.h
                              Queues IR commands (max 50).
                              Processes one at a time with min spacing.
                              Detects screen sleep and auto-inserts a
                              wake pulse when needed.
                              Handles ENSURE_ON (wait-for-sleep then on).
     │  (button.press())
     ▼
Internal Template Buttons     vornado-fan.yaml
(power, direction,            Each maps to one raw IR code.
 increase, decrease)          Not exposed to Home Assistant.
     │  (remote_transmitter.transmit_symphony)
     ▼
IR Transmitter (GPIO)

     │  (38 kHz IR signal)
     ▼
Vornado Transom Fan
```

## Layer 1 — Raw IR: `vornado_controller`

Defined in `vornado_controller/vornado_controller.h`.

This layer's only job is to **reliably deliver IR commands to the physical fan**. It knows nothing about logical state (power/speed); it just presses buttons.

**Command queue** — all commands go into a `std::queue<Command>` (max 50). The `loop()` callback processes one command at a time using a processing lock (`is_locked_`). This ensures commands are never sent simultaneously and are always spaced by at least `min_spacing_ms` (default 400ms). A 30-second lock timeout protects against bugs that would stall the queue forever.

**Screen wake logic** — The Vornado's display sleeps after ~10 seconds of inactivity. When asleep, the first IR command only wakes the display without executing. `VornadoController` tracks `last_command_time_` and, when a command arrives after the sleep timeout, automatically sends it *twice*: once as a wake pulse, then again (after the spacing delay) as the actual command. `POWER_ON` is exempt — it acts as its own wake command.

**`ENSURE_ON` command** — A special command that *waits* for the screen to definitely be asleep (default 15 seconds), then sends `POWER_ON`. Because `POWER_ON` always turns the fan on when the screen is asleep (regardless of prior state), this guarantees the fan ends up on. The wait time is calculated from `last_command_time_`; if enough time has already passed, it fires immediately.

**Actions exposed to YAML:**
- `vornado_controller.send_command` — queue a single named command (`POWER_ON`, `POWER_OFF`, `DIRECTION`, `SPEED_INCREASE`, `SPEED_DECREASE`, `ENSURE_ON`)
- `vornado_controller.send_sequence` — queue a list of commands

## Layer 2 — State Logic: `vornado_stateful_controller`

Defined in `vornado_stateful_controller/vornado_stateful_controller.h`.

This layer sits on top of `VornadoController` and tracks **what the fan is doing** in software (since the fan has no feedback mechanism). It never touches IR directly — it only calls `controller_->send_command()` and `controller_->send_sequence()`.

**State model** — Two enums track current state optimistically:
- `PowerState`: `Unknown` → `Off` or `On`
- `SpeedState`: `Unknown` → `1`, `2`, `3`, or `4`

State starts as `Unknown` at boot and is updated immediately each time a command is sent (optimistic updates — the firmware assumes the fan responded correctly).

**`ensure_known_state()`** — Called at the start of every public action. If either power or speed state is unknown, it triggers `reset_to_known_state()` before proceeding, so all subsequent logic can reason about state reliably.

**`reset_to_known_state()`** — Sends `ENSURE_ON` followed by 3× `SPEED_DECREASE`. Since the fan's minimum speed is 1, three decreases always land at speed 1 regardless of starting position. After queuing this sequence, state is set optimistically to `Power=On, Speed=1`.

**`set_speed(target)`** — Calculates the minimum number of increase or decrease presses to move from the current speed to the target, then queues only those presses.

**`turn_on()`** — If power is off, sends `POWER_ON`. If speed is unknown after reset, defaults to speed 2.

**`turn_off()`** — Only sends `POWER_OFF` if not already off.

**State sensors** — Auto-created at build time (in `vornado_stateful_controller/__init__.py`):
- A `TextSensor` for power state, published as `"Unknown"`, `"Off"`, or `"On"`
- A `Sensor` for speed, published as `NaN` (unknown) or `1.0`–`4.0`

**Actions exposed to YAML:**
- `vornado_stateful.turn_on`
- `vornado_stateful.turn_off`
- `vornado_stateful.set_speed` (1–4)
- `vornado_stateful.toggle_direction`
- `vornado_stateful.reset_state`

## IR Codes

These are raw Symphony protocol IR codes captured from the original Vornado Transom remote:

| Command | Code | Bits |
|---------|------|------|
| Power toggle | `0xD84` | 12 |
| Speed increase | `0xDC6` | 12 |
| Speed decrease | `0xD82` | 12 |
| Direction toggle | `0xD81` | 12 |

The `remote_transmitter` component uses the `transmit_symphony` action to send these. The IR LED should be connected to the GPIO pin specified by `vornado_ir_pin`, with carrier duty cycle set to 50%.
