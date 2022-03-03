#pragma once

#define NODE_FLAG_HAS_BUTTON BIT0
#define NODE_FLAG_HAS_RINGER BIT1

#define NODE_ID_LEN 8

typedef int32_t ring_event_number_t;

typedef enum {
    PACKET_TYPE_HEARTBEAT,
    PACKET_TYPE_RING_EVENT,
    PACKET_TYPE_RING_ACKNOWLEDGE,
} packet_type_id_t;

typedef struct {
    uint8_t node_id[NODE_ID_LEN];
    uint32_t minimum_free_heap;
    uint32_t current_free_heap;
    uint64_t uptime;
    uint8_t node_flags;
} packet_type_heartbeat_t;

typedef struct {
    uint8_t node_id[NODE_ID_LEN];
    ring_event_number_t event_number;
} packet_type_ring_event_t;

typedef struct {
    uint8_t ring_node_id[NODE_ID_LEN];
    uint8_t ack_node_id[NODE_ID_LEN];
    ring_event_number_t event_number;
} packet_type_ring_acknowledge_t;

typedef union {
    packet_type_heartbeat_t heartbeat;
    packet_type_ring_event_t ring;
    packet_type_ring_acknowledge_t ring_ack;
} packet_info_t;

typedef struct {
    char magic[4];
    uint16_t crc;
    packet_type_id_t id;
    packet_info_t info;
} packet_t;

typedef struct {
    int peers;
    int max_peers;
    int peers_with_button;
    int peers_with_ringer;
    uint64_t most_recent_seen_time;
    int ringers_acknowledged_last_ring;
    ring_event_number_t current_ring_number;
} comms_status_summary_t;

extern comms_status_summary_t comms_status_summary;

typedef int send_broadcast_function_t(const void *buffer, int length);

/**
 * @brief Plugin point for transport layer send method.
 */
extern send_broadcast_function_t *comms_send_callback;

/**
 * @brief Initialise the comms layer
 * 
 * @param my_node_flags 
 */
void comms_init(uint8_t my_node_flags);

/**
 * @brief Handle an incoming packet.
 * This method is not reentrant. It is assumed that the transport layer will deliver packets one at a time.
 * 
 * @param buffer packet data
 * @param length length of packet data
 */
void comms_on_packet(void *buffer, int length);

/**
 * @brief Construct and send a ring event (which may take multiple packets)
 * 
 * @param ring_number 
 */
void comms_send_ring();

