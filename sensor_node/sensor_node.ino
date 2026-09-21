/*
 * sensor_node.ino — CAN smart-sensor node (I²C → CAN adapter)
 * ============================================================
 * A small ESP32 + SN65HVD230 transceiver that reads ONE sensor over a short
 * local I²C link and publishes the result on the CAN bus. This adapts the
 * existing VL53L0X / BH1750 sensors to CAN *without changing the sensors* —
 * the sensor stays exactly as-is; only its wiring moves from a 3 m I²C cable
 * to a few centimetres of I²C next to this node.
 *
 * Select the node type below:
 *   NODE_TOF  → VL53L0X presence/distance node (publishes range + presence)
 *   NODE_LUX  → BH1750 ambient-light node (publishes lux)
 *
 * Message IDs are defined in can_protocol.h — keep them in sync.
 */

#define NODE_TOF   1               // VL53L0X presence/distance node
#define NODE_LUX   2               // BH1750 ambient-light node
#define NODE_TYPE  NODE_TOF        // <- select the node type

#include <Arduino.h>
#include <Wire.h>
#include <driver/twai.h>

// ---- CAN wiring (ESP32 WROOM-32 conventions; change to your pins) ----
#define CAN_TX    5               // -> SN65HVD230 CTX / D
#define CAN_RX    4               // <- SN65HVD230 CRX / R

// ---- I²C (short local link to the sensor) ----
#define I2C_SDA   21
#define I2C_SCL   22

// ---- Protocol (must match can_protocol.h) ----
#define CAN_NODE_MAIN       0x01
#define CAN_NODE_BOTTOM     0x10
#define CAN_NODE_TOP        0x11
#define CAN_NODE_BH1750     0x12
#define CAN_ID_DIST_BOTTOM  0x110
#define CAN_ID_DIST_TOP     0x111
#define CAN_ID_LUX          0x112
#define CAN_ID_HEARTBEAT    0x120
#define CAN_ID_SET_THRESHOLD 0x201

// ---- Node identity / defaults ----
#if NODE_TYPE == NODE_TOF
  #define MY_NODE_ID       CAN_NODE_BOTTOM   // set to CAN_NODE_BOTTOM or CAN_NODE_TOP
  #define MY_MSG_ID        (MY_NODE_ID == CAN_NODE_BOTTOM ? CAN_ID_DIST_BOTTOM : CAN_ID_DIST_TOP)
  #define DEFAULT_THRESHOLD_MM 70            // presence distance (configurable over CAN)
  #define MIN_PRESENCE_MM  30               // ghost/crosstalk floor
  #define SEND_PERIOD_MS   100              // publish rate (10 Hz)
  #include <VL53L0X.h>
  VL53L0X tof;
  uint16_t thresholdMm = DEFAULT_THRESHOLD_MM;
#else
  #define MY_NODE_ID       CAN_NODE_BH1750
  #define MY_MSG_ID        CAN_ID_LUX
  #define SEND_PERIOD_MS   1000             // lux changes slowly → 1 Hz
  #include <BH1750.h>
  BH1750 lightMeter;
#endif

// ---- Payload encoding (must match can_protocol.h) ----
static inline void encode_distance(uint8_t* data, bool presence, uint16_t mm, bool valid) {
  data[0] = presence ? 1 : 0;
  data[1] = (uint8_t)(mm & 0xFF);
  data[2] = (uint8_t)((mm >> 8) & 0xFF);
  data[3] = valid ? 1 : 0;
}
static inline void encode_lux(uint8_t* data, float lux) {
  uint16_t v = (uint16_t)(lux * 10.0f);
  data[0] = (uint8_t)(v & 0xFF);
  data[1] = (uint8_t)((v >> 8) & 0xFF);
  data[2] = 0;
  data[3] = 0;
}

// ---- CAN helpers ----
static bool can_send(uint32_t id, const uint8_t* data, uint8_t len) {
  twai_message_t msg;
  msg.identifier = id;
  msg.extd = 0;                 // standard 11-bit
  msg.rtr = 0;
  msg.data_length_code = len;
  for (int i = 0; i < len; i++) msg.data[i] = data[i];
  return twai_transmit(&msg, pdMS_TO_TICKS(10)) == ESP_OK;
}

static void can_init() {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
      (gpio_num_t)CAN_TX, (gpio_num_t)CAN_RX, TWAI_MODE_NORMAL);
  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  if (twai_driver_install(&g, &t, &f) == ESP_OK) {
    twai_start();
    Serial.println("[CAN] bus started (500 kbps)");
  } else {
    Serial.println("[CAN] driver install failed");
  }
}

// ---- Sensor init ----
void init_sensor() {
#if NODE_TYPE == NODE_TOF
  Wire.begin(I2C_SDA, I2C_SCL);
  if (tof.init(true)) {                 // 2.8V mode (more stable)
    tof.setTimeout(500);
    tof.setMeasurementTimingBudget(33000);
    tof.startContinuous(50);            // 50 ms inter-measurement
    Serial.printf("[TOF] sensor ready at 0x%02X (threshold %umm)\n",
                  tof.getAddress(), thresholdMm);
  } else {
    Serial.println("[TOF] sensor not found — check wiring");
  }
#else
  Wire.begin(I2C_SDA, I2C_SCL);
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire)) {
    Serial.println("[LUX] BH1750 ready");
  } else {
    Serial.println("[LUX] BH1750 not found — check wiring");
  }
#endif
}

// ---- Process a config message from the main controller ----
void handle_config(const twai_message_t& msg) {
#if NODE_TYPE == NODE_TOF
  if (msg.identifier == CAN_ID_SET_THRESHOLD && msg.data[0] == MY_NODE_ID) {
    thresholdMm = (uint16_t)(msg.data[1] | ((uint16_t)msg.data[2] << 8));
    Serial.printf("[TOF] threshold -> %umm\n", thresholdMm);
  }
#endif
}

void setup() {
  Serial.begin(115200);
  can_init();
  init_sensor();
}

void loop() {
  // Receive (config) messages
  twai_message_t rx;
  while (twai_receive(&rx, pdMS_TO_TICKS(0)) == ESP_OK) {
    handle_config(rx);
  }

  // Publish sensor data on a fixed cadence
  static unsigned long lastSend = 0;
  static unsigned long lastBeat = 0;
  unsigned long now = millis();

  if (now - lastSend >= SEND_PERIOD_MS) {
    lastSend = now;
    uint8_t data[8] = {0};

#if NODE_TYPE == NODE_TOF
    uint16_t range = tof.readRangeContinuousMillimeters();
    bool valid = (range != 65535);                       // 65535 = timeout/out-of-range
    bool presence = valid && range >= MIN_PRESENCE_MM && range < thresholdMm;
    encode_distance(data, presence, valid ? range : 0, valid);
    can_send(MY_MSG_ID, data, 4);
#else
    float lux = lightMeter.measurementReady() ? lightMeter.readLightLevel() : 0.0f;
    if (lux < 0) lux = 0;
    encode_lux(data, lux);
    can_send(MY_MSG_ID, data, 4);
#endif
  }

  // Heartbeat every 2 s so the main can flag a dead node
  if (now - lastBeat >= 2000) {
    lastBeat = now;
    uint8_t beat[1] = { (uint8_t)MY_NODE_ID };
    can_send(CAN_ID_HEARTBEAT, beat, 1);
  }

  delay(10);
}
