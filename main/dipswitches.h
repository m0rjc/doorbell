#pragma once

#include <stdint.h>

extern uint8_t dip_switches;

#define DIP_SWITCH_BIT_RINGER (1)
#define DIP_SWITCH_BIT_BUTTON (2)
#define DIP_SWITCH_BITS_MODE (4)
// The top two switches can allow 4 modes. Eg WiFi, ESPNow, Mesh, Config
#define DIP_SWITCH_MODE_CONFIG (4)
#define DIP_SWITCH_MODE_RUN_WIFI (0)

#define DIP_HAS_RINGER ((dip_switches & DIP_SWITCH_BIT_RINGER) != 0)
#define DIP_HAS_BUTTON ((dip_switches & DIP_SWITCH_BIT_BUTTON) != 0)
#define DIP_IS_MODE_CONFIG ((dip_switches & DIP_SWITCH_BITS_MODE) == DIP_SWITCH_MODE_CONFIG)
#define DIP_IS_MODE_RUN ((dip_switches & DIP_SWITCH_BITS_MODE) != DIP_SWITCH_MODE_CONFIG)
#define DIP_IS_MODE_RUN_WIFI ((dip_switches & DIP_SWITCH_BITS_MODE) == DIP_SWITCH_MODE_RUN_WIFI)

void dip_switchs_init();