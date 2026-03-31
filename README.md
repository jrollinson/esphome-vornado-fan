# ESPHome Vornado Fan Controller

ESPHome configuration and custom components for controlling a Vornado fan via IR, with intelligent state tracking and Home Assistant integration.

## Hardware

- **Microcontroller:** ESP8266 D1 Mini
- **IR transmitter:** IR LED connected to GPIO4 (e.g. a simple LED + resistor, or a pre-built IR blaster module)
- **Target device:** Vornado fan with Symphony IR remote (tested on [your model here])

## Features

- **State tracking** — maintains power (Unknown/Off/On) and speed (1–4) state in software, since the fan has no feedback mechanism
- **Screen wake handling** — the Vornado's display sleeps after ~10 seconds of inactivity; the first IR command only wakes the display without executing. This config handles that automatically by sending a wake pulse before the real command when needed.
- **Ensure On** — a special command that waits for the screen to sleep, then sends power on, guaranteeing the fan ends up on regardless of prior state
- **Command queueing** — all IR commands are queued and spaced to avoid collisions
- **Home Assistant integration** — exposes turn on/off, speed 1–4, direction toggle, and state sensors

## Custom Components

This repo includes two custom ESPHome components:

### `vornado_controller`

The base IR layer. Manages:
- A queue of IR button commands
- Minimum spacing between commands (default 400ms)
- Screen sleep detection and automatic wake-before-command logic
- The `ENSURE_ON` command (waits for screen sleep timeout, then powers on)

### `vornado_stateful_controller`

Sits on top of `vornado_controller`. Manages:
- Optimistic state tracking for power and speed
- Smart speed changes (calculates the minimum increase/decrease presses needed)
- Auto power-on when a speed is requested while off
- State reset to a known position

## Setup

1. Copy this repo to the same directory as your ESPHome configs (or wherever your `external_components` path points).

2. Edit `vornado-fan.yaml` and replace the placeholder values:

   | Placeholder | Description |
   |---|---|
   | `YOUR_WIFI_SSID` | Your WiFi network name |
   | `YOUR_WIFI_PASSWORD` | Your WiFi password |
   | `YOUR_FALLBACK_PASSWORD` | Password for the fallback hotspot |
   | `YOUR_API_ENCRYPTION_KEY_BASE64` | 32-byte base64 key for HA API encryption |
   | `YOUR_OTA_PASSWORD` | Password for OTA firmware updates |

   Generate an API key:
   ```bash
   python3 -c "import base64, os; print(base64.b64encode(os.urandom(32)).decode())"
   ```

3. Flash to your D1 Mini:
   ```bash
   esphome run vornado-fan.yaml
   ```

## IR Codes

These are the raw Symphony protocol IR codes captured from the original Vornado remote:

| Command | Code | Bits |
|---|---|---|
| Power toggle | `0xD84` | 12 |
| Speed increase | `0xDC6` | 12 |
| Speed decrease | `0xD82` | 12 |
| Direction toggle | `0xD81` | 12 |

## Architecture

```
Home Assistant
     │
     ▼
Template Buttons (turn_on, turn_off, speed 1-4, direction)
     │
     ▼
VornadoStatefulController   ← tracks power/speed state
     │
     ▼
VornadoController           ← queues commands, handles wake timing
     │
     ▼
remote_transmitter (IR LED on GPIO4)
     │
     ▼
Vornado Fan
```

## Home Assistant Entities

After adding to HA, you'll get:

**Buttons:**
- Turn On / Turn Off
- Speed 1 / Speed 2 / Speed 3 / Speed 4
- Direction toggle
- Reset State *(diagnostic — resets to known state: on, speed 1)*

**Sensors:**
- Vornado Fan Power State *(text: Unknown / Off / On)*
- Vornado Fan Speed *(numeric: NaN or 1–4)*
- Uptime, WiFi Signal, Free Heap, IP Address, SSID, ESPHome Version *(diagnostic)*
