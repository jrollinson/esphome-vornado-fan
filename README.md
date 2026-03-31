# ESPHome Vornado Transom Fan Controller

ESPHome package for controlling a Vornado Transom fan via IR, with state tracking and Home Assistant integration.

## Usage

Use this as a package from your own device config. See [`examples/d1_mini_vornado_fan.yaml`](examples/d1_mini_vornado_fan.yaml) for a complete example.

Required substitution: `vornado_ir_pin` — the GPIO pin connected to the IR LED.

## Home Assistant Entities

**Buttons:** Turn On, Turn Off, Speed 1–4, Direction, Reset State *(diagnostic)*

**Sensors:** Power State *(Unknown / Off / On)*, Speed *(NaN or 1–4)*, plus standard diagnostic sensors

## Features

- **State tracking** — maintains power and speed state in software since the fan has no feedback
- **Screen wake handling** — the display sleeps after ~10s; the first IR command only wakes it. Handled automatically.
- **Ensure On** — waits for the screen to sleep, then powers on, guaranteeing the fan ends up on
- **Command queueing** — IR commands are queued and spaced to avoid collisions
- **Smart speed changes** — sends the minimum number of presses to reach the target speed

## Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for component details and IR codes.
