# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESP32-based network doorbell system that allows ringing doorbells remotely over WiFi. The project sends doorbell signals over the house WiFi network using UDP multicast when short-range wireless protocols (ESP-Now) proved unreliable due to thick walls.

Hardware: ESP32 development boards (DOIT kits), relay modules for driving bells, custom enclosures.

## Build System

This project uses ESP-IDF (Espressif IoT Development Framework).

### Building and Flashing

```bash
# Configure project (if needed)
idf.py menuconfig

# Build and flash to device
idf.py flash

# Monitor debug output
idf.py monitor

# Build, flash, and monitor in one command
idf.py flash monitor
```

### Project Structure

- `main/` - All application source code
- `sdkconfig` - ESP-IDF configuration (committed to repo)
- `CMakeLists.txt` - Project-level CMake configuration
- `main/CMakeLists.txt` - Component registration and source file list

## Architecture

### Event-Driven Design

The system is built around FreeRTOS with a central event queue (`main_queue`) that processes events in `main_loop_task()` (main/main.c:45). All asynchronous events from different subsystems flow through this queue.

Event types (mainQueue.h:20-26):
- `EVENT_TYPE_NETWORK_CHANGE` - WiFi connection state changes
- `EVENT_TYPE_PEER_COUNT_CHANGE` - Peer discovery/loss
- `EVENT_TYPE_BELL_BUTTON_PUSH` - Local button pressed
- `EVENT_TYPE_REMOTE_BELL_BUTTON_PUSH` - Remote button pressed (via network)
- `EVENT_TYPE_ACKNOWLEDGE` - Acknowledgment from remote ringer

### Operating Modes

The device can operate in two modes controlled by DIP switches (dipswitches.h):

1. **Config Mode** (`DIP_SWITCH_MODE_CONFIG`): Entered by holding red button during boot. Creates WiFi access point at 192.168.4.1 for configuration. Web UI allows setting WiFi credentials and device name. Boots into config mode automatically if no SSID is configured.

2. **Run Mode** (`DIP_SWITCH_MODE_RUN`): Normal operation. Connects to configured WiFi network, discovers peers via multicast, and operates as button/ringer.

### Device Roles

Devices can be configured (via DIP switches) as:
- **Button** (`DIP_HAS_BUTTON`): Monitors physical button, sends ring commands
- **Ringer** (`DIP_HAS_RINGER`): Receives ring commands, activates relay
- **Both**: Can act as both button and ringer simultaneously

### Communication Protocol

**Transport Layer**: UDP multicast (comms_multicast.c) for peer-to-peer communication over WiFi.

**Protocol Layer** (comms.c, comms.h): Custom packet-based protocol with CRC validation and magic number. Three packet types:

1. `PACKET_TYPE_HEARTBEAT` - Periodic announcements with node info (ID, name, capabilities, heap stats)
2. `PACKET_TYPE_RING_EVENT` - Ring command with event number and pattern
3. `PACKET_TYPE_RING_ACKNOWLEDGE` - Confirmation from ringer that ring was received

**Peer Discovery**: Peers tracked in peers.c with timeout-based expiry. Heartbeats refresh peer "last seen" time. System tracks peer capabilities (button/ringer flags).

**Reliability**: Ring events use retry mechanism (main.c:94-104). Sends ring command up to `RING_RETRY_COUNT` (5) times with `RING_RETRY_DELAY` (500ms) between retries until all expected ringers acknowledge.

### Key Subsystems

- **nvs.c/nvs.h**: Non-volatile storage for WiFi credentials and device name using ESP-IDF NVS API
- **wifi.c/wifi.h**: WiFi station mode initialization and connection management
- **webui.c/webui.h**: HTTP server for configuration and status display
- **web/formparams.c, web/stringbuilder.c**: HTTP form parsing and HTML generation utilities
- **leds.c/leds.h**: LED status indicators (config mode, WiFi, ready state, TX/RX activity)
- **pushbutton.c/pushbutton.h**: Physical button debouncing and event generation
- **ringer.c/ringer.h**: Relay control for activating physical bell
- **sleep.c/sleep.h**: Power management (if implemented)

### Status Indicators (LEDs)

- **Red LED** (`LED_STATUS_CONFIG`): Config mode active
- **Blue LED** (`LED_STATUS_WIFI`): WiFi connected
- **Green LED** (`LED_STATUS_READY`): Meaningful connection established (button has ringer peer or vice versa)
- **Yellow LEDs**: TX/RX packet activity indicators

### Configuration Storage

Configuration stored in NVS (nvs.h, nvs.c):
- `name`: Device name (max `NODE_NAME_LEN` = 25 chars)
- `ssid`: WiFi SSID
- `password`: WiFi password

Empty SSID forces device into config mode on boot.

## Development Notes

- The main event loop uses dynamic timeout: `portMAX_DELAY` when idle, `RING_RETRY_DELAY` when actively retrying ring events
- Node IDs are 8 bytes (`NODE_ID_LEN`), typically MAC address
- Maximum 10 peers tracked simultaneously (`MAX_PEERS`)
- Task notifications use index 0 (`TASK_NOTIFY_INDEX`)
- All components should post events to `main_queue` rather than directly calling other subsystems
- When adding new packet types, update `packet_type_id_t` enum and `packet_info_t` union in comms.h
