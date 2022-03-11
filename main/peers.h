#pragma once

#include "comms.h"
#include "common.h"

#define MAX_PEERS 10

typedef struct  {
    bool is_active;
    bool acknowledged;
    char name[NODE_NAME_LEN + 1];
    uint8_t node_id[NODE_ID_LEN];
    uint8_t node_flags;
    uint64_t last_seen_time;
} peer_info_t;

extern peer_info_t peer_infos[MAX_PEERS];

typedef struct {
    int peers;
    int max_peers;
    int peers_with_button;
    int peers_with_ringer;
    int ringers_acknowledged_last_ring;
} comms_status_summary_t;

extern comms_status_summary_t comms_status_summary;

void peers_init();

void peers_on_heartbeat(packet_type_heartbeat_t *heartbeat);

void peers_clear_acknowledge_status();

void peers_set_acknowledged(uint8_t *node_id);

void peers_count_acknowledgements(int *expected, int *found);