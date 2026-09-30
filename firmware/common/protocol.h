/**
 * @file protocol.h
 * @brief Authoritative ESP-NOW Multi-Node Protocol Specification.
 * 
 * Implements two distinct operational tiers:
 * Tier 1: Continuous lightweight telemetry (Node ID, 100ms RMS amplitude, Battery mV).
 * Tier 2: On-demand high-speed fragmented 64 KB audio transfer from candidate cot.
 * 
 * Complies with ESP-NOW 250-byte maximum frame constraints.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// System Limits & Parameters
#define MAX_NODES                3
#define AUDIO_SAMPLE_RATE        16000
#define AUDIO_DURATION_SEC       2
#define TOTAL_AUDIO_BYTES        (AUDIO_SAMPLE_RATE * sizeof(int16_t) * AUDIO_DURATION_SEC) // 64,000 bytes
#define AUDIO_PAYLOAD_PER_PACKET 200 // Bytes per fragment (fits within 250-byte ESP-NOW limit)
#define TOTAL_AUDIO_FRAGMENTS    (TOTAL_AUDIO_BYTES / AUDIO_PAYLOAD_PER_PACKET)             // exactly 320 packets

// Message Type Identifiers
typedef enum {
    MSG_TYPE_TELEMETRY    = 0x01, // Periodic RMS & battery status (Node -> S3 Hub)
    MSG_TYPE_AUDIO_REQ    = 0x02, // Request 64KB buffer from candidate cot (S3 Hub -> Node)
    MSG_TYPE_AUDIO_FRAG   = 0x03, // Audio PCM fragment packet (Node -> S3 Hub)
    MSG_TYPE_AUDIO_ACK    = 0x04  // Audio transfer completion / verification (S3 Hub -> Node)
} packet_type_t;

/**
 * @brief TIER 1: Periodic Node Telemetry Packet (~100ms interval)
 * Total Size: 12 bytes
 */
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;      // MSG_TYPE_TELEMETRY
    uint8_t  node_id;       // 1, 2, or 3
    uint16_t seq_num;       // Monotonically increasing sequence number
    uint32_t timestamp_ms;  // Uptime in ms
    float    rms_amplitude; // Calculated 100ms RMS energy
    uint16_t battery_mv;    // Battery millivolts from MAX17048
} telemetry_packet_t;

/**
 * @brief TIER 2: Hub Audio Request Packet
 * Total Size: 7 bytes
 */
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;      // MSG_TYPE_AUDIO_REQ
    uint8_t  target_node;   // Target candidate cot (1, 2, or 3)
    uint16_t session_id;    // Unique audio session ID
    uint16_t total_frags;   // Expected fragments (320)
    uint8_t  reserved;
} audio_req_packet_t;

/**
 * @brief TIER 2: Audio Fragment Packet
 * Total Size: 208 bytes (well under the 250-byte ESP-NOW limit)
 */
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;      // MSG_TYPE_AUDIO_FRAG
    uint8_t  node_id;       // 1, 2, or 3
    uint16_t session_id;    // Session matching request
    uint16_t frag_index;    // 0 to 319
    uint16_t payload_len;   // Typically 200
    uint8_t  audio_data[AUDIO_PAYLOAD_PER_PACKET]; // Raw 16-bit PCM slice
} audio_frag_packet_t;

/**
 * @brief Caregiver Wearable BLE GATT Packet (Transmitted by S3 to Wearable C3)
 * Total Size: 6 bytes
 */
typedef struct __attribute__((packed)) {
    uint8_t  cot_id;        // 1, 2, or 3
    uint8_t  alert_type;    // 0x01 = Genuine Infant Cry, 0x02 = Cancel
    uint16_t confidence_pct;// e.g., 95 for 95.0%
    uint16_t event_id;      // Unique event ID for logging
} wearable_alert_packet_t;

#ifdef __cplusplus
}
#endif

#endif // PROTOCOL_H
