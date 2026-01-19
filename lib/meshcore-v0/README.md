# MeshCore Protocol v0 Library

Self-contained implementation of MeshCore protocol v0 for meshgrid-firmware.

## Overview

This library provides a clean, self-contained implementation of the MeshCore protocol v0, extracted from the original MeshCore project. It uses proper dependency injection and callback patterns instead of global state.

## Architecture

The library follows MeshCore's original clean architecture:

```
meshcore-v0/
├── src/
│   ├── MeshCore.h         - Constants and definitions
│   ├── Packet.h/cpp       - Wire format encoding/decoding
│   ├── Utils.h/cpp        - Crypto primitives (AES, HMAC, SHA256)
│   ├── Identity.h/cpp     - Ed25519 key management, ECDH
│   ├── Mesh.h/cpp         - Protocol packet handling
│   ├── Dispatcher.h/cpp   - Radio TX/RX queue management
│   └── MeshgridAdapter.h/cpp - Adapter layer for meshgrid integration
└── lib/
    └── ed25519/           - Ed25519 cryptography implementation
```

## Key Features

- **Self-contained**: No external global dependencies
- **Callback-based**: Uses virtual methods and dependency injection
- **Testable**: Can be unit tested independently
- **Proven**: Based on production-tested MeshCore code
- **Compatible**: 100% wire-format compatible with MeshCore devices

## Integration

To integrate with meshgrid-firmware, you need to:

1. Implement adapter classes for your hardware
2. Provide callback functions for neighbor lookup, message storage, etc.
3. Initialize the mesh instance in your main code

Example:

```cpp
#include <MeshgridAdapter.h>

// Create adapters
MeshgridRadio radio_adapter(get_radio());
MeshgridClock clock_adapter;
// ... other adapters

// Create callbacks
MeshgridCallbacks callbacks = {
    .get_secret = neighbor_get_shared_secret,
    .find_neighbor = neighbor_find,
    // ... other callbacks
};

// Create mesh instance
MeshgridMesh mesh_v0(radio_adapter, clock_adapter, rng_adapter,
                     rtc_adapter, packet_mgr, tables_adapter, callbacks);

void setup() {
    mesh_v0.begin();
}

void loop() {
    mesh_v0.loop();
}
```

## Dependencies

- **RadioLib** (^7.3.0): LoRa radio abstraction
- **Crypto** (^0.4.0): AES, SHA256, HMAC implementations
- **Ed25519** (bundled): Digital signatures and ECDH

## License

MIT License (same as MeshCore)

## Credits

Based on MeshCore by the MeshCore team: https://github.com/ripplebiz/MeshCore
