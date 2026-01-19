# meshgrid-v1 Protocol Library

**Status:** In Development (Phase 1 - Foundation)
**Version:** 1.0.0-alpha

## Overview

The meshgrid-v1 protocol library implements an enhanced mesh networking protocol with improved security, efficient multi-hop discovery, and OTA update capabilities. It is designed to be reusable across different platforms and applications.

## Features (Planned)

### Phase 1: Foundation (Current)
- [x] Packet encoding/decoding (MeshCore-compatible baseline)
- [x] 1-byte node hashing (v0 compatibility)
- [ ] 2-byte node hashing (v1 enhanced)
- [ ] Library structure and build system

### Phase 2: Enhanced Cryptography
- [ ] AES-256-GCM authenticated encryption
- [ ] HMAC-SHA256 (16-byte MAC)
- [ ] 4-byte sequence numbers for replay protection
- [ ] ChaCha20-Poly1305 support (for platforms without AES hardware)

### Phase 3: Advanced Discovery
- [ ] Attenuated Bloom filters (4-level, multi-hop)
- [ ] Trickle algorithm (RFC 6206) for adaptive beaconing
- [ ] Multi-TTL beacon scheduling

### Phase 4: OTA Updates
- [ ] Epidemic gossip protocol
- [ ] Ed25519-signed manifests
- [ ] Chunk distribution with bitmap tracking
- [ ] Firmware verification

## Directory Structure

```
lib/meshgrid-v1/
├── src/
│   ├── protocol/           # Core protocol
│   │   ├── packet.c        # Packet encode/decode (Phase 1)
│   │   ├── packet.h        # Protocol structures
│   │   ├── crypto.c        # Cryptography (Phase 2)
│   │   ├── hash.c          # Hashing & sequences (Phase 2)
│   │   └── forward.c       # Forwarding logic (Phase 2)
│   ├── discovery/          # Enhanced discovery (Phase 3)
│   │   ├── bloom.c         # Bloom filters
│   │   ├── trickle.c       # Trickle algorithm
│   │   └── beacon.c        # Multi-TTL beacons
│   ├── ota/                # OTA protocol (Phase 4)
│   │   ├── manifest.c      # Manifest handling
│   │   ├── gossip.c        # Epidemic gossip
│   │   ├── chunks.c        # Chunk distribution
│   │   └── verify.c        # Verification
│   └── utils/              # Utilities
│       ├── constants.h     # Protocol constants
│       └── types.h         # Data structures
├── library.json            # PlatformIO library metadata
└── README.md               # This file
```

## Current Status (Phase 1)

**Completed:**
- Copied baseline packet.c/packet.h from src/network/protocol.[ch]
- Created library structure
- Set up PlatformIO library metadata

**Next Steps:**
1. Verify v0 protocol still works (run test suite)
2. Begin Phase 2: Implement enhanced crypto
3. Refactor packet.h to support both v0 and v1 formats

## Protocol Versions

### v0 (MeshCore Compatibility)
- 1-byte node hash (256 values, collision at ~17 nodes)
- 2-byte MAC (weak, but compatible)
- AES-128 ECB encryption
- No replay protection
- Status: **Supported for backward compatibility**

### v1 (meshgrid Enhanced)
- 2-byte node hash (65,536 values, collision at ~300 nodes)
- 16-byte HMAC-SHA256
- AES-256-GCM authenticated encryption
- 4-byte sequence numbers (replay protection)
- Bloom filters for multi-hop discovery
- Trickle algorithm for adaptive beaconing
- Status: **In Development**

## Usage

The library is designed to be used through a callback interface. Applications implement callbacks for:
- Getting shared secrets for peers
- Storing received messages
- Transmitting packets via radio
- Handling discovered nodes

See `src/core/integration/meshgrid_v1_bridge.cpp` (planned) for integration example.

## Security

See [SECURITY_ANALYSIS.md](../../docs/SECURITY_ANALYSIS.md) for detailed security analysis of v0 vs v1 protocols.

Key improvements in v1:
- **16-byte HMAC** (vs 2-byte MAC) - Prevents forgery
- **AES-GCM** (vs ECB) - No pattern leakage
- **2-byte hash** (vs 1-byte) - Prevents identity spoofing
- **Sequence numbers** - Replay protection
- **Nonces** - Prevents cryptographic attacks

## Testing

Run the v0 baseline test to ensure backward compatibility:

```bash
cd meshgrid-firmware
./test_v0_baseline.sh
```

This ensures that v1 development doesn't break v0 compatibility.

## License

MIT License - See LICENSE file for details

## References

- [LIB_ARCHITECTURE_PLAN.md](../../docs/LIB_ARCHITECTURE_PLAN.md) - Full architecture plan
- [SECURITY_ANALYSIS.md](../../docs/SECURITY_ANALYSIS.md) - Security analysis
- [HOPPING_ADVERT_PROTOCOL.md](../../docs/HOPPING_ADVERT_PROTOCOL.md) - Discovery protocol
- [OTA_DESIGN.md](../../docs/OTA_DESIGN.md) - OTA update design

## Contributing

This library is under active development. Contributions welcome!

**Current Focus:** Phase 1 - Foundation
**Help Needed:**
- Code review of packet.c/packet.h
- Platform testing (ESP32, nRF52840, RP2040)
- Performance benchmarking
