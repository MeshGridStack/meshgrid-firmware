# meshgrid-firmware

Multi-platform LoRa mesh networking firmware supporting **72 boards** across 6 architectures with dual-protocol support (v0 and v1).

[![Build](https://github.com/MeshGridStack/meshgrid-firmware/workflows/Build%20Test/badge.svg)](https://github.com/MeshGridStack/meshgrid-firmware/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## Disclaimer

This project is not affiliated with, endorsed by, or an official part of the MeshCore project. MeshCore is referenced solely to describe protocol compatibility.

## For MeshCore-Compatible Networks

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

## Multi-Architecture Support

meshgrid-firmware supports **72 boards** across 6 architectures with a unified codebase:

### ✅ Production Ready (43 boards)

| Architecture | Boards | Status | Notes |
|--------------|--------|--------|-------|
| **ESP32-S3** | 30 boards | ✅ Ready | Full support with NVS storage |
| **ESP32** | 12 boards | ✅ Ready | Original ESP32, full support |
| **ESP32-C3** | 1 board | ✅ Ready | Heltec HT62 |

### 🚧 In Development (28 boards)

| Architecture | Boards | Status | Blocking Issue |
|--------------|--------|--------|----------------|
| **nRF52840** | 20 boards | 🚧 WIP | Needs NVS storage abstraction |
| **ESP32-C6** | 1 board | 🚧 WIP | Arduino framework not yet available |
| **RP2040** | 4 boards | 🚧 WIP | Board definitions need registry updates |

### Featured Boards

| Board | MCU | Radio | Display | Status |
|-------|-----|-------|---------|--------|
| Heltec V3/V4 | ESP32-S3 | SX1262 | OLED 128x64 | ✅ Ready |
| LilyGo T3S3 | ESP32-S3 | SX1262 | OLED 128x64 | ✅ Ready |
| LilyGo T-Beam | ESP32 | SX1276 | OLED 128x64 | ✅ Ready |
| LilyGo T-Beam Supreme | ESP32-S3 | SX1262 | - | ✅ Ready |
| Station G2 | ESP32-S3 | SX1262 | OLED 128x64 | ✅ Ready |
| Heltec HT62 | ESP32-C3 | SX1262 | - | ✅ Ready |
| RAK4631 | nRF52840 | SX1262 | - | 🚧 WIP |
| LilyGo T-Echo | nRF52840 | SX1262 | E-ink | 🚧 WIP |

**Note:** T-Beam automatically detects AXP192 (v1.0/1.1) or AXP2101 (v1.2) power chips.

### All Supported Boards

<details>
<summary><b>ESP32-S3 Boards (30)</b></summary>

- Heltec V3, V4, Wireless Stick Lite V3, Wireless Tracker, Wireless Paper
- Heltec Vision Master T190, E213, E290
- LilyGo T3S3, T-Beam Supreme, T-Deck, T-Deck Pro, T-LoRa Pager, T-Watch S3, T3S3 E-Ink
- B&Q Station G2
- Seeed SenseCap Indicator, XIAO ESP32S3
- Elecrow ThinkNode M2, M5, CrowPanel 2.4", 3.5", 4.3"
- RAK3312
- Ebyte EORA-S3
- Tracksenger Small/Big
- Pi Computer S3
- unPhone

</details>

<details>
<summary><b>ESP32 (Original) Boards (12)</b></summary>

- LilyGo T-Beam, T-LoRa V2.1 (1.6/1.8)
- B&Q Nano G1, Nano G1 Explorer, Station G1
- RAK11200
- RadioMaster 900 Bandit
- M5Stack
- DIY V1, Hydra

</details>

<details>
<summary><b>ESP32-C3 Boards (1)</b></summary>

- Heltec HT62

</details>

<details>
<summary><b>nRF52840 Boards (20) - WIP</b></summary>

- LilyGo T-Echo
- RAK4631, RAK WisMesh Repeater/Tap/Tag, RAK3401-1W
- Heltec Mesh Node T114, Mesh Pocket
- Seeed T1000-E, XIAO nRF52840, SenseCap Solar, Wio Tracker L1/L1 E-ink, Wio-WM1110
- Elecrow ThinkNode M1, M3
- B&Q Nano G2 Ultra
- Muzi Base, R1 Neo
- NomadStar Meteor Pro
- Canary One
- DIY nRF52 Pro Micro

</details>

<details>
<summary><b>ESP32-C6 Boards (1) - WIP</b></summary>

- M5Stack Unit C6L

</details>

<details>
<summary><b>RP2040 Boards (4) - WIP</b></summary>

- RAK11310
- Generic RP2040 LoRa
- Raspberry Pi Pico, Pico W

</details>

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

# Build ESP32-S3 boards
pio run -e heltec_v3
pio run -e lilygo_t3s3
pio run -e station_g2

# Build ESP32 (original) boards
pio run -e lilygo_tbeam
pio run -e nano_g1

# Build ESP32-C3 boards
pio run -e heltec_ht62

# Build all working boards (43 boards)
pio run

# List all available environments
pio run --list-targets
```

### Build Status by Architecture

✅ **Ready to build:** ESP32-S3, ESP32, ESP32-C3 (43 boards total)
🚧 **In development:** nRF52840, RP2040, ESP32-C6 (29 boards)

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

## Architecture

### Unified Cross-Platform Codebase

meshgrid-firmware uses a **flat, unified architecture** rather than separate platform directories:

- ✅ **Single codebase** for all platforms
- ✅ **Platform guards** (`#if defined(ARCH_*)`) for platform-specific code
- ✅ **RadioLib** provides radio abstraction across all platforms
- ✅ **Arduino framework** for cross-platform APIs
- ✅ **Board abstraction layer** with runtime ops pointers

**Key Design Decisions:**
- Core mesh logic, radio, and network code are platform-independent
- ESP32-specific features (Preferences, mbedtls) are conditionally compiled
- Memory buffers sized appropriately for each platform's RAM
- Board configs define hardware pins and capabilities

### Hardware Support

#### T-Beam v1.2 (AXP2101)
The firmware includes automatic power chip detection for T-Beam boards:
- **v1.0/1.1**: Uses AXP192 power driver
- **v1.2**: Uses AXP2101 power driver via XPowersLib
- Auto-detects at runtime, no manual configuration needed
- Fixes serial freeze issues specific to ESP32-S3 USB CDC

#### Power Management
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
3. Include header in `src/hardware/boards/boards.h`
4. Add power driver if needed in `src/hardware/power/`
5. Test build: `pio run -e <board_name>`

### Directory Structure

```
meshgrid-firmware/
├── platformio.ini              # Build configurations (72 boards)
├── .github/workflows/
│   ├── build.yml              # CI builds (ESP32 variants)
│   ├── release.yml            # Release builds (43 boards)
│   └── lint.yml               # Code formatting checks
├── src/
│   ├── main.cpp                # Universal entry point (all platforms)
│   ├── core/                   # Platform-independent mesh logic
│   │   ├── neighbors.cpp       # Neighbor table management
│   │   ├── messaging.cpp       # Message handling
│   │   ├── channels.cpp        # Channel management (ESP32 NVS)
│   │   ├── integration/        # Protocol integration
│   │   │   ├── meshgrid_v1_bridge.cpp  # V1 protocol
│   │   │   └── protocol_selector.cpp   # Auto-selection
│   │   ├── meshcore_integration.cpp    # V0 protocol
│   │   └── commands/           # Serial commands
│   ├── radio/                  # RadioLib HAL (cross-platform)
│   ├── network/                # Protocol definitions (cross-platform)
│   ├── hardware/               # Hardware abstraction
│   │   ├── board.h             # Board abstraction interface
│   │   ├── boards/             # 72 board configs
│   │   │   ├── boards.h        # Master board selector
│   │   │   ├── heltec/         # Heltec boards (ESP32-S3, ESP32-C3, nRF52)
│   │   │   ├── lilygo/         # LilyGo boards (ESP32-S3, ESP32, nRF52)
│   │   │   ├── rak/            # RAKwireless (nRF52, ESP32-S3, RP2040)
│   │   │   ├── seeed/          # Seeed Studio (ESP32-S3, nRF52)
│   │   │   ├── elecrow/        # Elecrow (ESP32-S3, nRF52)
│   │   │   ├── bq/             # B&Q (ESP32-S3, ESP32, nRF52)
│   │   │   ├── m5stack/        # M5Stack (ESP32, ESP32-C6)
│   │   │   ├── rpi/            # Raspberry Pi (RP2040)
│   │   │   ├── muzi/           # Muzi (nRF52)
│   │   │   ├── nomadstar/      # NomadStar (nRF52)
│   │   │   ├── canary/         # Canary (nRF52)
│   │   │   └── diy/            # DIY boards (ESP32, nRF52)
│   │   ├── power/              # Power management
│   │   │   ├── power_axp192.cpp    # T-Beam v1.0/1.1
│   │   │   ├── power_axp2101.cpp   # T-Beam v1.2
│   │   │   └── power_axp_auto.cpp  # Auto-detection
│   │   ├── crypto/             # Cryptographic operations
│   │   ├── telemetry/          # Battery/system monitoring (platform guards)
│   │   ├── bluetooth/          # BLE UART (ESP32 only)
│   │   └── mqtt/               # Optional MQTT bridge
│   ├── ui/                     # Display rendering (cross-platform)
│   └── utils/
│       ├── memory.h            # Platform-specific buffer sizes
│       ├── cobs.c              # COBS framing
│       └── debug.cpp           # Debug output
├── lib/
│   ├── meshcore-v0/            # MeshCore v0 library
│   ├── meshgrid-v1/            # meshgrid v1 library
│   └── meshgrid-protocol-auto/ # Protocol auto-selection
└── boards/                     # Custom board definitions
    ├── wiscore_rak4631.json    # nRF52840 boards
    ├── seeed_xiao_nrf52840_sense.json
    └── ttgo_t_echo.json
```

### Platform-Specific Features

| Feature | ESP32/S3/C3 | nRF52840 | RP2040 | ESP32-C6 |
|---------|-------------|----------|--------|----------|
| NVS Storage | ✅ Yes | 🚧 TODO | 🚧 TODO | ✅ Yes |
| BLE UART | ✅ Yes | 🚧 Possible | ❌ No | ✅ Yes |
| MQTT | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes |
| mbedtls | ✅ Built-in | ❌ External | ❌ External | ✅ Built-in |
| OTA Updates | ✅ Yes | 🚧 Possible | 🚧 Possible | ✅ Yes |

### Memory Footprint by Platform

| Platform | RAM Usage | Flash Usage | Max Neighbors | Max Channels |
|----------|-----------|-------------|---------------|--------------|
| ESP32-S3 | ~170 KB | ~565 KB | 512 | 50 |
| ESP32 | ~70 KB | ~597 KB | 50 | 10 |
| ESP32-C3 | ~85 KB | ~546 KB | 128 | 20 |
| nRF52840 | ~52 KB est. | TBD | 256 | 30 |
| RP2040 | ~52 KB est. | TBD | 256 | 30 |

## Contributing

### Completing nRF52840 Support

The nRF52840 platform needs NVS storage abstraction to work properly. Current blockers:

**Required Work:**
1. Abstract NVS storage layer to support LittleFS or EEPROM
2. Replace ESP32-specific functions in function bodies:
   - `prefs.begin()`, `prefs.putBytes()`, `prefs.getBytes()`
   - `ESP.restart()` → use platform-agnostic reset
   - `esp_random()` → use hardware RNG abstraction
3. Test build and runtime on RAK4631 or T-Echo

**Files needing storage abstraction:**
- `src/core/channels.cpp` - Channel storage
- `src/core/config.cpp` - Configuration storage
- `src/core/identity.cpp` - Key storage
- `src/core/neighbors.cpp` - Neighbor cache
- `src/core/security.cpp` - PIN storage

### Completing RP2040 Support

The RP2040 platform needs board definitions registered with PlatformIO.

**Required Work:**
1. Add board JSON files to PlatformIO registry or local boards directory
2. Test builds with correct board IDs (`pico`, `picow`, etc.)
3. Implement storage abstraction (same as nRF52840)

### Completing ESP32-C6 Support

ESP32-C6 support is blocked by Arduino framework availability.

**Status:** Wait for `arduino-esp32` to add C6 support in PlatformIO platform

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
