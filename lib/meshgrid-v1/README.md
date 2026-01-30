# meshgrid-v1 Protocol Library

**Status:** Library Complete - Ready for Integration
**Version:** 1.0.0

## Overview

The meshgrid-v1 protocol library implements an enhanced mesh networking protocol with improved security, efficient multi-hop discovery, and OTA update capabilities. It is designed to be reusable across different platforms and applications.

## Features

### ✅ Phase 1: Foundation (COMPLETE)
- [x] Packet encoding/decoding (MeshCore-compatible baseline)
- [x] 1-byte node hashing (v0 compatibility)
- [x] 2-byte node hashing (v1 enhanced)
- [x] Library structure and build system

### ✅ Phase 2: Enhanced Cryptography (COMPLETE)
- [x] AES-256-GCM authenticated encryption
- [x] HMAC-SHA256 (16-byte MAC)
- [x] 4-byte sequence numbers for replay protection
- [x] Nonce generation and management
- [x] Constant-time comparison (timing attack prevention)

### ✅ Phase 3: Advanced Discovery (COMPLETE)
- [x] Attenuated Bloom filters (4-level, multi-hop)
- [x] Trickle algorithm (RFC 6206) for adaptive beaconing
- [x] Multi-TTL beacon scheduling
- [x] Advertisement with bloom filters
- [x] Bloom filter parsing and merging

### ✅ Phase 4: OTA Updates (COMPLETE)
- [x] Epidemic gossip protocol
- [x] Ed25519-signed manifests
- [x] Chunk distribution with bitmap tracking
- [x] Firmware verification

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

## Library Status

**✅ All Core Features Implemented:**
- **Discovery System:** Bloom filters, beacon scheduling, trickle algorithm
- **Protocol System:** Packet encoding/decoding, v0 and v1 advertisement support
- **Cryptography:** AES-256-GCM, HMAC-SHA256, sequence numbers, nonce generation
- **OTA System:** Manifest handling, gossip protocol, chunk distribution, verification

**Ready for Integration:**
The library is complete and self-contained. Integration involves:
1. Including the library in your firmware project
2. Initializing bloom filters and beacon scheduling in your main application
3. Using `meshgrid_create_advert_with_bloom()` to send v1 advertisements
4. Using `meshgrid_parse_advert_with_bloom()` to receive v1 advertisements
5. See integration example in `src/core/integration/meshgrid_v1_bridge.cpp`

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

- [SECURITY_ANALYSIS.md](../../docs/SECURITY_ANALYSIS.md) - Security analysis of v0 vs v1
- [HOPPING_ADVERT_PROTOCOL.md](../../docs/HOPPING_ADVERT_PROTOCOL.md) - Discovery protocol
- [OTA_DESIGN.md](../../docs/OTA_DESIGN.md) - OTA update technical specification
- [OTA_WORKFLOW.md](../../docs/OTA_WORKFLOW.md) - OTA deployment workflow and implementation

## API Overview

### Discovery API
```c
#include "discovery/bloom.h"
#include "discovery/beacon.h"
#include "discovery/trickle.h"

// Initialize bloom filters
struct meshgrid_bloom_set bloom;
meshgrid_bloom_init(&bloom);
meshgrid_bloom_add(&bloom, 0, node_hash);  // Add to level 0

// Initialize beacon scheduling
struct meshgrid_beacon_schedule schedule;
meshgrid_beacon_init(&schedule, millis());

// Check when to send beacons
if (meshgrid_beacon_should_send_local(&schedule, millis())) {
    // Send local beacon (TTL=2)
}
```

### Protocol API
```c
#include "protocol/packet.h"

// Create v1 advertisement with bloom filters
struct meshgrid_packet pkt;
meshgrid_create_advert_with_bloom(&pkt, pubkey, name, timestamp, &bloom);

// Parse v1 advertisement
uint8_t pubkey[32];
char name[17];
uint32_t timestamp;
struct meshgrid_bloom_set received_bloom;
meshgrid_parse_advert_with_bloom(&pkt, pubkey, name, sizeof(name),
                                 &timestamp, &received_bloom);

// Compute v1 2-byte hash
uint16_t hash = meshgrid_v1_hash_pubkey(pubkey);
```

### Cryptography API
```c
#include "protocol/crypto.h"

// AES-256-GCM encryption
uint8_t key[32], nonce[12], ciphertext[256], tag[16];
meshgrid_v1_aes_gcm_encrypt(key, nonce, aad, aad_len, plaintext, pt_len,
                            ciphertext, tag);

// HMAC-SHA256
uint8_t mac[16];
meshgrid_v1_hmac_sha256(key, key_len, data, data_len, mac);
```

## Contributing

This library is feature-complete for v1.0.0. Contributions welcome!

**Areas for Contribution:**
- Platform testing (ESP32, nRF52840, RP2040)
- Performance benchmarking
- Integration examples
- Unit tests
- Documentation improvements
