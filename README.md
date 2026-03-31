# ESPHome Vornado Transom Fan Controller

ESPHome package for controlling a Vornado Transom fan via IR, with state tracking and Home Assistant integration.

## Usage

Use this as a package from your own device config. See [`examples/d1_mini_vornado_fan.yaml`](examples/d1_mini_vornado_fan.yaml) for a complete example.

Required substitution: `vornado_ir_pin` — the GPIO pin connected to the IR LED.

## Home Assistant Entities

**Buttons:** Turn On, Turn Off, Speed 1–4, Direction, Reset State *(diagnostic)*

**Sensors:** Power State *(Unknown / Off / On)*, Speed *(NaN or 1–4)*, plus standard diagnostic sensors

## Features

- **Turn on/off and set speed 1–4** directly from Home Assistant — no need to step through speeds manually
- **Speed and power state** are tracked and exposed as sensors, so automations can read the current fan state
- **Commands always work**, even if the fan's display has gone to sleep — the wake-up is handled transparently
- **"Ensure On"** resets to a known state and guarantees the fan is on, useful in automations where you can't be sure of the starting state
- **Rapid commands are safe** — pressing buttons quickly won't confuse the fan; commands are queued and delivered in order

## Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for component details and IR codes.
