#pragma once

#include "comms.h"

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