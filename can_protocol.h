/*
 * can_protocol.h — shared CAN message definitions
 * ================================================
 * Contract between the ESP32-S3 main controller and the smart-sensor nodes.
 * The main no longer reads the sensors over I²C; each sensor sits next to a
 * small node that reads it over a *short* local I²C link and publishes the
 * result on the CAN bus.
 *
 * Bus: CAN 2.0 (TWAI on ESP32), 500 kbps, standard 11-bit identifiers,
 *      120 Ω termination at each end of the bus.
 */
#pragma once
#include <stdint.h>

// ── Bus settings ────────────────────────────────
#define CAN_BITRATE_KHZ     500

// ── Participant / node IDs ──────────────────────
#define CAN_NODE_MAIN       0x01   // ESP32-S3 main controller
#define CAN_NODE_BOTTOM     0x10   // bottom VL53L0X node
#define CAN_NODE_TOP        0x11   // top VL53L0X node
#define CAN_NODE_BH1750     0x12   // BH1750 ambient-light node

// ── Message IDs (11-bit standard) ───────────────
#define CAN_ID_DIST_BOTTOM  0x110  // bottom ToF  -> main : distance + presence
#define CAN_ID_DIST_TOP     0x111  // top ToF     -> main : distance + presence
#define CAN_ID_LUX          0x112  // BH1750      -> main : lux (0.1-lux units)
#define CAN_ID_HEARTBEAT    0x120  // any node    -> main : node_id (liveness)
#define CAN_ID_SET_THRESHOLD 0x201 // main -> ToF node : presence threshold (mm)
#define CAN_ID_SET_POLL     0x202  // main -> BH1750   : poll interval (s)

// ── Payload layouts (classic CAN, up to 8 bytes) ──
// CAN_ID_DIST_BOTTOM / CAN_ID_DIST_TOP:
//   data[0]      = presence (1 = person in range, 0 = clear)
//   data[1..2]   = distance_mm (uint16, little-endian)
//   data[3]      = range_valid (1 = valid, 0 = out-of-range/error)
//
// CAN_ID_LUX:
//   data[0..1]   = lux * 10 (uint16, little-endian)  → 0.1 lux resolution
//
// CAN_ID_HEARTBEAT:
//   data[0]      = node_id (CAN_NODE_*)
//
// CAN_ID_SET_THRESHOLD:
//   data[0]      = target node_id (CAN_NODE_BOTTOM or CAN_NODE_TOP)
//   data[1..2]   = threshold_mm (uint16, little-endian)
//
// CAN_ID_SET_POLL:
//   data[0]      = interval_seconds (uint8)

// ── Helpers ─────────────────────────────────────
static inline void encode_distance(uint8_t* data, bool presence, uint16_t mm, bool valid) {
  data[0] = presence ? 1 : 0;
  data[1] = (uint8_t)(mm & 0xFF);
  data[2] = (uint8_t)((mm >> 8) & 0xFF);
  data[3] = valid ? 1 : 0;
}

static inline void decode_distance(const uint8_t* data,
                                   bool& presence, uint16_t& mm, bool& valid) {
  presence = (data[0] != 0);
  mm       = (uint16_t)(data[1] | ((uint16_t)data[2] << 8));
  valid    = (data[3] != 0);
}

static inline void encode_lux(uint8_t* data, float lux) {
  uint16_t v = (uint16_t)(lux * 10.0f);   // clamp handled by uint16 wrap
  data[0] = (uint8_t)(v & 0xFF);
  data[1] = (uint8_t)((v >> 8) & 0xFF);
  data[2] = 0;
  data[3] = 0;
}

static inline float decode_lux(const uint8_t* data) {
  uint16_t v = (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
  return (float)v / 10.0f;
}
