# Plan — Configurable VL53L0X sensor count (branch `sensors_number`)

## Goal

Let the system be configured to run with **1 or 2** VL53L0X ToF sensors via the
web dashboard, and have that choice applied **before** the sensors are
configured and read at boot.

## Approach

The sensor count is a boot-time setting persisted to NVS. It is read in
`setup()` *before* the VL53L0X init block, so only the configured number of
sensors is initialized. The dashboard exposes a dropdown (1 or 2) that writes
the value to NVS through a new API endpoint; the change takes effect on the next
reboot.

Why "reboot to apply" rather than hot reconfiguration: the two sensors share one
I²C bus and must be assigned unique addresses (0x29 / 0x30) in a specific
order. Re-assigning those addresses safely while a sensor is already running is
fragile, so the count is resolved once at boot.

## Changes made

### `config.h`
- Added `SENSOR_COUNT_DEFAULT` (2), `SENSOR_COUNT_MIN` (1), `SENSOR_COUNT_MAX` (2).

### `lights_control.ino`
1. Added NVS key `PREF_KEY_SENSOR_COUNT` and global `configuredSensorCount`.
2. `setup()` now loads the persisted count (clamped to 1–2) in the same block
   that loads duration / distance / poll settings — i.e. **before** sensor init.
3. Sensor init block:
   - Top sensor is only initialized when `configuredSensorCount >= 2`; with 1
     sensor its XSHUT pin stays LOW (shutdown) so it can never collide on the bus.
   - Bottom sensor is always initialized (count >= 1).
   - Reachability check now guarded by `tofXReady` (skips reads on missing sensors).
4. New endpoint `GET|POST /api/sensors?count=1|2` (clamped, persisted to NVS,
   returns `reboot_required:true`).
5. `/api` JSON now includes `sensor_count`.
6. Dashboard: added a "Sensors" dropdown (1 / 2) with a Set button that calls
   the new endpoint.

## Verification

- Compiles clean for `esp32:esp32:esp32s3` (86% flash, 14% RAM — unchanged budget).
- With `count=1` the top sensor stays in shutdown; bottom initializes at 0x29.
- With `count=2` the existing top-first / bottom-second address assignment is
  preserved (no address collision).

## Potential blockers / risks

1. **Runtime hot-swap is out of scope.** Changing the count requires a reboot.
   Re-assigning I²C addresses at runtime is possible but fragile; if live
   reconfiguration is ever required it should be its own change (see the
   `sensors_scan` branch for hot-plug recovery of a *missing* sensor, which is a
   different concern).
2. **Setting `count=2` with only one sensor wired** — the missing sensor's
   `init()` fails gracefully (logged as "not found") and the system runs with
   one sensor. No crash, but no automatic retry either (again, that is the
   `sensors_scan` branch's job).
3. **NVS key is new** — first boot after flashing uses the default (2 sensors),
   so existing single-sensor units must be configured once via the dashboard.
4. **Flash budget** — the sketch is already at 86% of program storage; large
   additions (e.g. a full config UI) could push it over the 1310720-byte limit.
5. **Wiring assumption** — "1 sensor" is interpreted as *bottom only* (the
   sensor on the default 0x29 address / bottom XSHUT GPIO 4). If a single
   physical sensor is wired to the *top* position instead, it would not be
   detected. The physical-to-logical mapping should be documented per install.
