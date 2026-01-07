# meshgrid-firmware

PlatformIO-based firmware for meshgrid mesh networking devices.

## Supported Devices

| Board | MCU | Radio | Status |
|-------|-----|-------|--------|
| Heltec V3 | ESP32-S3 | SX1262 | Ready |
| Heltec V4 | ESP32-S3 | SX1262 | Ready |
| LilyGo T3S3 | ESP32-S3 | SX1262 | Ready |
| LilyGo T-Beam | ESP32 | SX1276 | Ready |
| LilyGo T-Beam Supreme | ESP32-S3 | SX1262 | Ready |
| LilyGo T-Echo | nRF52840 | SX1262 | Ready |
| RAK4631 | nRF52840 | SX1262 | Ready |
| Station G2 | ESP32-S3 | SX1262 | Ready |
| Nano G1 | ESP32 | SX1276 | Ready |
| ThinkNode M1 | nRF52840 | SX1262 | Ready |
| Seeed T1000-E | nRF52840 | LR1110 | WIP |

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

The easiest way to flash is using meshgrid-cli:

```bash
# Install CLI
cargo install --path ../meshgrid-cli

# Flash device
meshgrid flash heltec-v3 --monitor
meshgrid flash rak4631
meshgrid flash tbeam
```

## Development

### Adding a New Board

1. Create board config in `boards/<vendor>/<board>.h`
2. Add environment to `platformio.ini`
3. Include header in `main.cpp`

### Directory Structure

```
meshgrid-firmware/
├── platformio.ini      # Build configurations
├── main.cpp            # Entry point
├── lib/
│   └── board.h         # Board abstraction
├── boards/             # Board definitions
│   ├── heltec/
│   ├── lilygo/
│   ├── rak/
│   ├── seeed/
│   ├── bq/
│   └── elecrow/
├── drivers/            # Hardware drivers
│   ├── radio/
│   ├── display/
│   └── gps/
├── net/                # Protocol implementation
└── crypto/             # Cryptography
```

## License

MIT
