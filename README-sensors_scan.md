# Plan — Missing-sensor scan & hot-plug recovery (branch `sensors_scan`)

## Goal

Make the system scan for missing VL53L0X sensors and recover gracefully:

- **Both sensors missing** → the controller blocks in a retry loop until at
  least one sensor is connected (it cannot detect presence with zero sensors).
- **One sensor missing** → the system stays fully operational with the sensor
  that *is* present, and periodically re-checks for the missing one, integrating
  it automatically once it is connected.

## Approach

Sensor initialization is refactored into two reusable, single-sensor helpers
(`initBottomSensor`, `initTopSensor`) plus a non-blocking
`scanForMissingSensors()` that runs on a cadence from `loop()`. A
`rearmInterruptsIfArmed()` helper re-attaches presence interrupts after any
hot-plug re-init.

Detection is **poll-based**: there is no hardware "sensor connected" pin, so a
missing sensor is detected by attempting an I²C init at its expected address on
a configurable interval (default 10 s).

### Address-collision note (why recovery is non-trivial)

The two sensors share one I²C bus. The **top** sensor must briefly claim the
default `0x29` address and then be moved to `0x30`; the **bottom** lives at
`0x29`. To recover a missing *top* sensor while the *bottom* is running, the
bottom must be momentarily held in shutdown (XSHUT LOW) so the top can claim
`0x29` without a bus collision — then the bottom is re-initialized. This is the
reason the top recovery briefly touches the bottom sensor.

## Changes made

### `config.h`
- Added `SENSOR_SCAN_INTERVAL_SEC` (default 10 s) — re-scan cadence.

### `lights_control.ino`
1. Extracted `initBottomSensor()` / `initTopSensor()` (each does: wake → init →
   set address → timing budget → GPIO1 interrupt config → start continuous).
2. `setup()` now calls these helpers, then:
   - runs the post-init reachability check (guarded by `tofXReady`), and
   - if **both** sensors are missing, blocks in a retry loop (`delay(2000)`)
     until at least one connects.
3. `loop()` runs `scanForMissingSensors()` every `SENSOR_SCAN_INTERVAL_SEC`
   whenever a sensor is missing (non-blocking — the system stays operational).
4. `rearmInterruptsIfArmed()` re-attaches interrupts after a re-init while
   interrupts were armed.
5. `/api` JSON now reports `sensor_bottom_ok` / `sensor_top_ok`.
6. Dashboard distance tiles show a red **MISSING** label when a sensor is not
   initialized.

## Verification

- Compiles clean for `esp32:esp32:esp32s3` (86% flash, 14% RAM).
- Logic paths exercised by inspection:
  - both sensors present → unchanged boot, no re-scan work.
  - one missing → boots with the present sensor, re-scan recovers the other.
  - zero present → blocking boot loop until one is connected.

## Potential blockers / risks

1. **No hardware "sensor present" signal.** Recovery is poll-based and therefore
   has up to `SENSOR_SCAN_INTERVAL_SEC` of latency. It cannot wake from sleep or
   fire an interrupt the instant a sensor is plugged in.
2. **Top recovery disturbs the bottom sensor.** Because the top must claim
   `0x29` first, each top-recovery attempt briefly shuts down and re-initializes
   the bottom — a short (~tens of ms, plus the failed-init time) presence blind
   spot, repeated every scan interval while the top stays missing. Increasing
   `SENSOR_SCAN_INTERVAL_SEC` reduces this churn at the cost of slower recovery.
3. **Mid-operation disconnects are not detected.** The scan targets sensors that
   failed to initialize *at boot*. A sensor that initializes fine and later
   disconnects while running is not actively re-checked (its interrupt simply
   stops firing). Detecting that case would require periodic reachability
   polling of *ready* sensors — a larger change.
4. **Blocking zero-sensor boot.** With no sensors connected the device sits in a
   loop and never starts the web server, so there is no dashboard during that
   wait — only serial output. If remote diagnosis with zero sensors is ever
   needed, the loop could be made non-blocking (start the server anyway).
5. **XSHUT pull-ups / wiring.** Hot-plugging assumes a clean XSHUT/GPIO1 wiring
   with the 10KΩ pull-ups documented in the main README; a floating XSHUT or
   GPIO1 can cause spurious inits or missed detections.
6. **Flash budget.** Sketch is at 86% of program storage; further additions
   (e.g. mid-operation disconnect polling) must watch the 1310720-byte ceiling.
