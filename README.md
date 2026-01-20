# meshgrid-firmware

PlatformIO-based firmware for LoRa mesh networking devices with dual-protocol support (v0 and v1).

## For MeshCore Enthusiasts

**meshgrid** builds on MeshCore v0 with a more secure v1 protocol (AES-256-GCM, authentication, replay protection) while staying fully compatible. We've also created **meshgrid-cli** for device management and flashing. The firmware automatically uses v1 when supported, v0 for compatibility.

## Features

- **Dual Protocol Support**:
  - **v0 (MeshCore)**: Legacy compatibility with existing MeshCore networks
  - **v1 (meshgrid)**: Enhanced security protocol with AES-256-GCM encryption
- **Auto-Protocol Selection**: Automatically uses v1 when both peers support it, falls back to v0 for compatibility
- **Enhanced Security (v1)**:
  - AES-256-GCM authenticated encryption
  - ECDH (Curve25519) key exchange for shared secrets
  - 16-byte authentication tags
  - Replay protection with sequence numbers
  - 12-byte nonces (timestamp + random)
- **Time Synchronization**: RTC with NVS persistence for accurate message timestamps
- **Channel Messaging**: Encrypted group channels (v0 and v1 support)
- **Direct Messaging**: Peer-to-peer encrypted messaging
- **Neighbor Discovery**: Advertisement-based discovery with protocol capability negotiation

## Supported Devices

| Board | MCU | Radio | Power Chip | Status |
|-------|-----|-------|------------|--------|
| Heltec V3 | ESP32-S3 | SX1262 | GPIO | Ready |
| Heltec V4 | ESP32-S3 | SX1262 | GPIO | Ready |
| LilyGo T3S3 | ESP32-S3 | SX1262 | - | Ready |
| LilyGo T-Beam v1.0/1.1 | ESP32 | SX1276 | AXP192 | Ready |
| LilyGo T-Beam v1.2 | ESP32 | SX1276 | AXP2101 | Ready |
| LilyGo T-Beam Supreme | ESP32-S3 | SX1262 | - | Ready |
| LilyGo T-Echo | nRF52840 | SX1262 | - | Ready |
| RAK4631 | nRF52840 | SX1262 | - | Ready |
| Station G2 | ESP32-S3 | SX1262 | - | Ready |
| Nano G1 | ESP32 | SX1276 | - | Ready |
| ThinkNode M1 | nRF52840 | SX1262 | - | Ready |
| Seeed T1000-E | nRF52840 | LR1110 | - | WIP |

**Note:** T-Beam automatically detects AXP192 (v1.0/1.1) or AXP2101 (v1.2) power chips.

## Prerequisites

Install PlatformIO CLI:

```bash
# Using pip
pip install platformio

# Or using installer
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py | python3
```

## Building

```bash
cd meshgrid-firmware

# Build for specific board
pio run -e heltec_v3
pio run -e rak4631
pio run -e lilygo_tbeam

# Build all
pio run
```

## Flashing

```bash
# Flash with auto-detect
pio run -e heltec_v3 -t upload

# Flash specific port
pio run -e heltec_v3 -t upload --upload-port /dev/ttyUSB0

# Flash and monitor
pio run -e heltec_v3 -t upload -t monitor
```

## Using meshgrid-cli (Recommended)

The easiest way to flash and interact with devices is using meshgrid-cli:

```bash
# Install CLI
cargo install --path ../meshgrid-cli

# Flash device
meshgrid-cli flash heltec-v3 --monitor
meshgrid-cli flash rak4631
meshgrid-cli flash tbeam

# Test V1 protocol
meshgrid-cli -p /dev/ttyUSB0 info
meshgrid-cli -p /dev/ttyUSB0 neighbors     # Check protocol versions
meshgrid-cli -p /dev/ttyUSB0 send --to "node-name" -- "Test V1 message"

# Time synchronization
meshgrid-cli -p /dev/ttyUSB0 time sync    # Sync with computer time
meshgrid-cli -p /dev/ttyUSB0 time show    # Show current device time

# Channel messaging
meshgrid-cli -p /dev/ttyUSB0 channels add "test" "<base64_psk>"
meshgrid-cli -p /dev/ttyUSB0 send --channel "test" -- "V1 channel message"
```

## Protocol Architecture

### V0 Protocol (MeshCore)
- Legacy compatibility with existing MeshCore mesh networks
- AES-128-CTR encryption with ECDH shared secrets
- Single-byte node hashes
- Compatible with all MeshCore clients

### V1 Protocol (meshgrid)
- **Enhanced encryption**: AES-256-GCM with 16-byte authentication tags
- **2-byte node hashes**: SHA256-based addressing for larger networks
- **Replay protection**: Sequence numbers prevent replay attacks
- **Timestamp synchronization**: Messages include sender's timestamp
- **Packet format**: `[header][nonce(12)][ciphertext][auth_tag(16)]`

### Protocol Selection
Devices advertise v1 capability via flag bit (0x08) in advertisements. When both peers support v1, messages automatically use v1 protocol. Falls back to v0 for legacy compatibility.

## Hardware Support

### T-Beam v1.2 (AXP2101)
The firmware includes automatic power chip detection for T-Beam boards:
- **v1.0/1.1**: Uses AXP192 power driver
- **v1.2**: Uses AXP2101 power driver via XPowersLib
- Auto-detects at runtime, no manual configuration needed
- Fixes serial freeze issues specific to ESP32-S3 USB CDC

### Power Management
Board-specific power drivers handle:
- Display power rails
- LoRa radio power
- GPS module power
- Battery monitoring
- USB power detection

## Development

### Adding a New Board

1. Create board config in `src/hardware/boards/<vendor>/<board>.h`
2. Add environment to `platformio.ini`
3. Include header in `src/main.cpp`
4. Add power driver if needed in `src/hardware/power/`

### Directory Structure

```
meshgrid-firmware/
├── platformio.ini              # Build configurations
├── src/
│   ├── main.cpp                # Entry point
│   ├── hardware/
│   │   ├── boards/             # Board-specific configs
│   │   │   ├── heltec/
│   │   │   ├── lilygo/
│   │   │   ├── rak/
│   │   │   └── seeed/
│   │   ├── power/              # Power management drivers
│   │   │   ├── power_axp192.cpp    # T-Beam v1.0/1.1
│   │   │   ├── power_axp2101.cpp   # T-Beam v1.2
│   │   │   └── power_axp_auto.cpp  # Auto-detection
│   │   ├── radio/              # Radio drivers
│   │   ├── crypto/             # Crypto hardware
│   │   └── telemetry/          # Sensors
│   ├── core/
│   │   ├── neighbors.cpp       # Neighbor table management
│   │   ├── messaging.cpp       # Message handling
│   │   ├── channels.cpp        # Channel management
│   │   ├── integration/        # Protocol integration
│   │   │   ├── meshgrid_v1_bridge.cpp  # V1 protocol
│   │   │   └── protocol_selector.cpp    # Auto-selection
│   │   ├── meshcore_integration.cpp  # V0 protocol
│   │   └── commands/           # Serial commands
│   ├── network/
│   │   └── protocol.h          # Packet format definitions
│   └── utils/
│       ├── cobs.c              # COBS framing
│       └── debug.cpp           # Debug output
└── lib/
    ├── meshcore-v0/            # MeshCore v0 library
    └── meshgrid-v1/            # meshgrid v1 library
```

## Troubleshooting

### Device Not Responding
1. Check serial port: `meshgrid-cli ports`
2. Try different baud rate: `meshgrid-cli -b 115200 info`
3. Reset device and reconnect USB

### Protocol Version Mismatch
- Check neighbor table: `meshgrid-cli neighbors` to see protocol versions
- Send fresh advertisements: `meshgrid-cli advert`
- Ensure both devices are running latest firmware

### Time Sync Issues
- Sync device time: `meshgrid-cli time sync`
- Time is stored in NVS and persists across reboots
- Check current time: `meshgrid-cli time show`

## License

MIT
