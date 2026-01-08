# Mesh OTA Updates - Design Specification

## Overview

Distributed over-the-air (OTA) firmware updates for ESP32-based meshgrid devices. Updates propagate through the mesh network via epidemic gossip protocol, surviving node failures and network partitions. Secured with Ed25519 signatures, auto-applied when battery levels are safe.

**Status:** Design phase - not yet implemented

**Key Differentiator:** MeshCore does NOT support mesh OTA. This feature makes meshgrid uniquely suited for distributed "war mesh" deployments where physical access to devices is impractical.

## Architecture

### High-Level Flow

```
┌─────────────────────────────────────────────────────────────┐
│ 1. CLI Push                                                 │
│    meshgrid-cli ota push firmware.signed                    │
│    → Sends to one device via USB                            │
└────────────────────┬────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────┐
│ 2. Manifest Flood (5 min)                                   │
│    Device verifies Ed25519 signature                        │
│    → Floods MANIFEST to all reachable nodes                 │
│    → Nodes verify signature, initialize chunk bitmap        │
└────────────────────┬────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────┐
│ 3. Chunk Distribution (10-30 min)                           │
│    CLI streams chunks to connected device                   │
│    → Device floods chunks (ROUTE_FLOOD)                     │
│    → Nodes gossip chunks peer-to-peer                       │
│    → Nodes request missing chunks                           │
└────────────────────┬────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────┐
│ 4. Verification & Apply                                     │
│    All chunks received                                      │
│    → Verify SHA-256 hash of complete firmware               │
│    → Check battery > 50%                                    │
│    → Write to OTA partition, set boot flag, reboot          │
└────────────────────┬────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────┐
│ 5. Boot Validation & Rollback                               │
│    ESP32 boots from new partition                           │
│    → Firmware validates (radio init, version check)         │
│    → If OK: mark valid, cancel rollback                     │
│    → If FAIL: watchdog triggers auto-rollback to old FW     │
└─────────────────────────────────────────────────────────────┘
```

## Protocol Specification

### Packet Format

All OTA packets use **PAYLOAD_MULTIPART (type 10)** with 1-byte subtype header.

**Max payload:** 184 bytes (MeshCore compatible)
**Chunk data:** 176 bytes (184 - 8 bytes overhead)

### OTA_MANIFEST (Subtype 0)

Announces new firmware availability to the mesh.

```
Byte Offset  Size  Field                 Description
-----------  ----  --------------------  ---------------------------
0            1     Subtype               0x00 (MANIFEST)
1-4          4     Session ID            Random identifier
5-36         32    Version String        "0.0.2-abc123" (null-term)
37-40        4     Firmware Size         Total bytes (uint32_t LE)
41-42        2     Chunk Size            Default 176 (uint16_t LE)
43-46        4     Total Chunks          Count (uint32_t LE)
47-78        32    SHA-256 Hash          Hash of complete firmware
79-142       64    Ed25519 Signature     Signs bytes [5-78]
143-174      32    Signing Public Key    Release key
175-182      8     Reserved              Zero-filled

Total: 183 bytes
```

**Signature Payload:** version(32) + size(4) + chunk_size(2) + chunks(4) + hash(32) = 74 bytes

### OTA_CHUNK (Subtype 1)

Contains firmware chunk data.

```
Byte Offset  Size  Field                 Description
-----------  ----  --------------------  ---------------------------
0            1     Subtype               0x01 (CHUNK)
1-4          4     Session ID            Matches manifest
5-6          2     Chunk Index           0-based (uint16_t LE)
7-8          2     Data Length           Actual bytes in chunk
9-184        176   Chunk Data            Firmware bytes

Total: 185 bytes (exceeds max payload - use 183 bytes, reduce chunk data to 174)
```

**Adjusted for 184-byte limit:**
```
9-182        174   Chunk Data            (reduced from 176)
Total: 183 bytes
```

### OTA_REQUEST (Subtype 2)

Requests missing chunks from peers.

```
Byte Offset  Size  Field                 Description
-----------  ----  --------------------  ---------------------------
0            1     Subtype               0x02 (REQUEST)
1-4          4     Session ID            Session to request from
5            1     Request Type          0=list, 1=range
6-...        Var   Chunk Identifiers     uint16_t list or range

Request Type 0 (Specific chunks):
  [6-7, 8-9, ...] = list of uint16_t chunk indices

Request Type 1 (Range):
  [6-7] = start_chunk (uint16_t)
  [8-9] = end_chunk (uint16_t)
```

### OTA_STATUS (Subtype 3)

Broadcasts download progress to peers.

```
Byte Offset  Size  Field                 Description
-----------  ----  --------------------  ---------------------------
0            1     Subtype               0x03 (STATUS)
1-4          4     Session ID            Current session
5            1     State                 0=idle, 1=downloading, 2=verifying, 3=ready
6-7          2     Chunks Received       Count (uint16_t LE)
8-9          2     Total Chunks          Expected (uint16_t LE)
10           1     Battery Percent       0-100
11           1     Error Code            0=none, or error enum

Total: 12 bytes
```

### OTA_ABORT (Subtype 4)

Cancels active OTA session.

```
Byte Offset  Size  Field                 Description
-----------  ----  --------------------  ---------------------------
0            1     Subtype               0x04 (ABORT)
1-4          4     Session ID            Session to abort
5            1     Reason Code           0=user, 1=error, 2=timeout

Total: 6 bytes
```

## Distribution Algorithm

### Epidemic Gossip Protocol

Combines initial flooding with ongoing peer-to-peer chunk exchange.

**Phase 1: Manifest Propagation (5 minutes)**
```
1. CLI sends MANIFEST via serial to Device A
2. Device A verifies signature:
   - Check signing_pubkey == OTA_RELEASE_PUBKEY (compiled-in)
   - Verify Ed25519 signature over manifest fields
3. If valid:
   - Store manifest in RAM
   - Initialize chunk bitmap (all zeros)
   - Flood MANIFEST (ROUTE_FLOOD, priority 2)
4. Neighbors receive, verify, flood onwards
5. Continue re-flooding for 5 minutes (ensure coverage)
```

**Phase 2: Chunk Seeding (CLI-driven)**
```
1. CLI waits 30 seconds post-manifest (let flood complete)
2. CLI streams chunks 0→N via serial
3. Device A:
   - Writes chunk to OTA partition (Update.write())
   - Marks chunk in bitmap
   - Floods chunk (ROUTE_FLOOD, priority 5)
   - 50-200ms jitter between chunks
4. Neighbors receive chunks:
   - Check bitmap (already have?)
   - If new: write to OTA partition, mark bitmap, re-flood
   - If duplicate: drop silently (deduplication)
```

**Phase 3: Gossip Exchange (continuous until complete)**
```
Every 30 seconds:
  1. Broadcast OTA_STATUS (my progress)
  2. On receiving peer STATUS:
     - Compare their chunks_received vs. mine
     - If they have chunks I need:
       → Send OTA_REQUEST for missing chunks
  3. On receiving REQUEST:
     - Check bitmap for requested chunks
     - Send OTA_CHUNK packets (ROUTE_DIRECT if path known)

Completion Detection:
  - chunks_received == total_chunks
  - Verify SHA-256 hash
  - Transition to OTA_STATE_READY
```

**Phase 4: Application**
```
When OTA_STATE_READY:
  1. Check battery_pct > 50
  2. If yes:
     - Update.end(true)  // Finalize and verify
     - esp_ota_set_boot_partition()
     - Reboot
  3. If no:
     - Stay in READY state
     - Check every 5 minutes
     - Log: "OTA ready, waiting for battery > 50%"
```

### Chunk Request Strategy

**Initial request (chunks 0-99% complete):**
- Send OTA_REQUEST type=1 (range) for first contiguous gap
- Example: Missing chunks 50-149 → request range [50, 149]
- Request interval: 30 seconds

**Final cleanup (>99% complete):**
- Send OTA_REQUEST type=0 (specific) for remaining chunks
- Example: Missing only [42, 105, 2001] → request specific list
- Request interval: 10 seconds (more aggressive)
- Retry limit: 3 attempts per chunk before giving up

## Security Design

### Cryptographic Components

**Ed25519 Signatures:**
- Release keypair generated once (offline, secured)
- Private key: Signs firmware manifests (CLI-side only)
- Public key: Compiled into firmware (all devices verify)

**SHA-256 Hashing:**
- Computed on full firmware binary (CLI-side during signing)
- Verified on device after all chunks assembled
- Uses ESP32 hardware accelerated SHA-256 (mbedtls)

### Threat Model

**Threats Mitigated:**
1. ✅ **Unauthorized updates**: Only firmware signed by release key accepted
2. ✅ **Corrupted firmware**: SHA-256 verification before install
3. ✅ **Man-in-the-middle**: Ed25519 signature can't be forged
4. ✅ **Rollback attacks**: Version comparison (reject older versions)
5. ✅ **Bricking**: Dual partitions + auto-rollback

**Threats NOT Mitigated (future work):**
1. ⚠️ **Replay attacks**: No signature timestamp validation yet
2. ⚠️ **Downgrade attacks**: Version check not enforced yet
3. ⚠️ **Key compromise**: No key rotation mechanism

### Signature Verification Process

**Device-side (on MANIFEST receipt):**
```c
bool ota_verify_manifest(struct ota_manifest *m) {
    // Step 1: Verify signer is trusted
    if (memcmp(m->signing_pubkey, OTA_RELEASE_PUBKEY, 32) != 0) {
        log_event("OTA: Unauthorized signer - rejecting");
        return false;
    }

    // Step 2: Reconstruct signed payload
    uint8_t payload[74];
    memcpy(&payload[0], m->version, 32);
    memcpy(&payload[32], &m->firmware_size, 4);
    memcpy(&payload[36], &m->chunk_size, 2);
    memcpy(&payload[38], &m->total_chunks, 4);
    memcpy(&payload[42], m->firmware_hash, 32);

    // Step 3: Verify Ed25519 signature (constant-time)
    if (!crypto_verify(m->signature, payload, 74, m->signing_pubkey)) {
        log_event("OTA: Invalid signature - rejecting");
        return false;
    }

    log_event("OTA: Manifest verified OK");
    return true;
}
```

**CLI-side (signing):**
```rust
fn sign_firmware(
    firmware: &[u8],
    version: &str,
    privkey: &[u8; 64],
    pubkey: &[u8; 32]
) -> OtaManifest {
    // Compute SHA-256 hash
    let hash = sha256(firmware);

    // Build signing payload
    let mut payload = Vec::new();
    payload.extend_from_slice(version.as_bytes().pad_to(32));
    payload.extend_from_slice(&(firmware.len() as u32).to_le_bytes());
    payload.extend_from_slice(&(176u16).to_le_bytes());  // chunk_size
    payload.extend_from_slice(&((firmware.len() / 174) as u32).to_le_bytes());
    payload.extend_from_slice(&hash);

    // Sign with Ed25519
    let signature = ed25519_sign(&payload, privkey);

    OtaManifest {
        session_id: random_u32(),
        version,
        firmware_size: firmware.len(),
        chunk_size: 174,
        total_chunks: (firmware.len() + 173) / 174,
        firmware_hash: hash,
        signature,
        signing_pubkey: *pubkey,
    }
}
```

## ESP32 Integration

### Partition Layout

**Required partitions.csv:**
```csv
# Name,     Type, SubType, Offset,   Size,     Flags
nvs,        data, nvs,     0x9000,   0x5000,   # Config storage
otadata,    data, ota,     0xe000,   0x2000,   # OTA state
app0,       app,  ota_0,   0x10000,  0x1E0000, # Active FW (1.875MB)
app1,       app,  ota_1,   0x1F0000, 0x1E0000, # Update FW (1.875MB)
spiffs,     data, spiffs,  0x3D0000, 0x30000,  # File system
```

**Flash requirements:**
- Minimum: 4MB flash
- Recommended: 8MB+ (typical on ESP32-S3)
- Current firmware: ~492KB (fits comfortably in 1.875MB partitions)

### ESP32 Update API

**Write Process:**
```cpp
#include <Update.h>

// Initialize OTA partition for writing
Update.begin(firmware_size, U_FLASH);

// Write chunks sequentially (order matters!)
for (uint16_t i = 0; i < total_chunks; i++) {
    Update.write(chunk_data, chunk_len);
}

// Finalize (verifies SHA-256 internally)
bool success = Update.end(true);

// Set boot partition and reboot
if (success) {
    ESP.restart();
}
```

**Rollback Protection:**
```cpp
void setup() {
    // Check if first boot after OTA
    const esp_partition_t* partition = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    esp_ota_get_state_partition(partition, &ota_state);

    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        // Validate new firmware
        if (firmware_is_valid()) {
            esp_ota_mark_app_valid_cancel_rollback();
        } else {
            // Auto-rollback to previous firmware
            esp_ota_mark_app_invalid_rollback_and_reboot();
        }
    }
}
```

### Out-of-Order Chunk Handling

**Challenge:** ESP32 Update.write() expects sequential writes, but mesh delivers chunks out-of-order.

**Solution 1 (Simple):** Buffer out-of-order chunks in RAM
```c
// Store up to 10 out-of-order chunks (1.7KB buffer)
struct pending_chunk {
    uint16_t index;
    uint8_t data[174];
    uint16_t len;
    bool valid;
} pending_chunks[10];

// On chunk receipt:
if (chunk_index == expected_next) {
    Update.write(chunk_data, chunk_len);
    expected_next++;
    flush_pending_chunks();  // Write any now-sequential chunks
} else {
    buffer_chunk(chunk_index, chunk_data, chunk_len);
}
```

**Solution 2 (Advanced):** Use esp_partition_write() directly
```c
// Bypass Update API, write directly to partition
const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
esp_partition_write(update_partition, chunk_index * chunk_size, chunk_data, chunk_len);
```

## State Management

### OTA Session State

```c
enum ota_state {
    OTA_STATE_IDLE = 0,          // No active OTA
    OTA_STATE_MANIFEST_RX = 1,   // Manifest received, verifying
    OTA_STATE_DOWNLOADING = 2,   // Receiving chunks
    OTA_STATE_PAUSED = 3,        // Low battery pause
    OTA_STATE_VERIFYING = 4,     // All chunks RX, verifying hash
    OTA_STATE_READY = 5,         // Ready to apply
    OTA_STATE_APPLYING = 6,      // Applying update (reboot soon)
    OTA_STATE_FAILED = 7,        // Failed (sig/hash error)
};

struct ota_session {
    bool active;                 // OTA in progress?
    uint32_t session_id;
    uint8_t state;

    // Manifest data
    char version[32];
    uint32_t firmware_size;
    uint16_t chunk_size;
    uint32_t total_chunks;
    uint8_t firmware_hash[32];
    uint8_t signature[64];
    uint8_t signing_pubkey[32];

    // Progress tracking
    uint32_t chunks_received;    // Count
    uint8_t *chunk_bitmap;       // Bitmap (dynamically allocated)

    // Timing
    uint32_t manifest_time;
    uint32_t last_chunk_time;
    uint32_t last_status_broadcast;
    uint32_t last_request_time;

    // ESP32 Update API state
    bool update_begun;
    uint16_t expected_chunk;     // Next sequential chunk to write
    uint8_t error_code;
};
```

### Chunk Bitmap

**Purpose:** Track which chunks have been received (persistent across reboots).

**Size:** For 492KB firmware: 2,808 chunks → 351 bytes bitmap

**Operations:**
```c
// Check if chunk received
bool ota_has_chunk(uint16_t idx) {
    return (chunk_bitmap[idx / 8] & (1 << (idx % 8))) != 0;
}

// Mark chunk as received
void ota_mark_chunk(uint16_t idx) {
    chunk_bitmap[idx / 8] |= (1 << (idx % 8));
    chunks_received++;
}

// Count received chunks
uint16_t ota_count_received() {
    uint16_t count = 0;
    for (uint32_t i = 0; i < total_chunks; i++) {
        if (ota_has_chunk(i)) count++;
    }
    return count;
}

// Find first missing chunk
int16_t ota_find_missing_chunk() {
    for (uint32_t i = 0; i < total_chunks; i++) {
        if (!ota_has_chunk(i)) return i;
    }
    return -1;  // All received
}
```

### NVS Persistence

**Namespace:** "ota" (separate from main "meshgrid" namespace)

**Keys:**
```c
"active"        // bool - OTA in progress
"session_id"    // uint32_t - Session ID
"state"         // uint8_t - Current state
"version"       // string - Target version
"fw_size"       // uint32_t - Firmware size
"chunk_size"    // uint16_t - Chunk size
"total_chunks"  // uint32_t - Total chunks
"fw_hash"       // blob[32] - SHA-256 hash
"signature"     // blob[64] - Ed25519 signature
"signing_key"   // blob[32] - Signer pubkey
"chunks_rx"     // uint32_t - Chunks received
"bitmap"        // blob[351] - Chunk bitmap
"manifest_time" // uint32_t - Manifest RX timestamp
```

**Persistence Strategy:**
- Write on state changes (MANIFEST_RX → DOWNLOADING → READY)
- Batch bitmap writes every 50 chunks (minimize flash wear)
- On boot: check "active" flag, resume if true

**Code Example:**
```cpp
void ota_save_state_to_nvs() {
    prefs.begin("ota", false);  // Read-write
    prefs.putBool("active", ota_session.active);
    prefs.putUInt("session_id", ota_session.session_id);
    prefs.putUChar("state", ota_session.state);
    prefs.putString("version", ota_session.version);
    prefs.putUInt("fw_size", ota_session.firmware_size);
    prefs.putUShort("chunk_size", ota_session.chunk_size);
    prefs.putUInt("total_chunks", ota_session.total_chunks);
    prefs.putBytes("fw_hash", ota_session.firmware_hash, 32);
    prefs.putBytes("signature", ota_session.signature, 64);
    prefs.putBytes("signing_key", ota_session.signing_pubkey, 32);
    prefs.putUInt("chunks_rx", ota_session.chunks_received);

    // Bitmap (largest item)
    size_t bitmap_size = OTA_BITMAP_SIZE(ota_session.total_chunks);
    prefs.putBytes("bitmap", ota_session.chunk_bitmap, bitmap_size);

    prefs.end();
}
```

## Resilience Mechanisms

### Surviving Node Failures

**Problem:** Source node (CLI-connected) fails mid-OTA.

**Solution:**
- Chunks already distributed to multiple nodes
- Any node with chunks can respond to REQUESTs
- Gossip continues autonomously
- No single point of failure

**Example:**
```
Network: A (source) ← B ← C ← D

1. A sends chunks 0-100, B has them
2. A fails (powered off)
3. C requests chunks from B (via STATUS/REQUEST)
4. D requests chunks from C
5. OTA completes without A
```

### Surviving Reboots

**Problem:** Node reboots mid-download, loses RAM state.

**Solution:**
```
On Boot:
  1. Check NVS "ota" namespace for "active" flag
  2. If true:
     - Load session state (session_id, version, chunks, etc.)
     - Load bitmap from NVS
     - Resume: OTA_STATE_DOWNLOADING
  3. Continue requesting missing chunks
  4. Eventually complete download
```

**Code:**
```cpp
void ota_resume_from_nvs() {
    prefs.begin("ota", true);  // Read-only
    if (!prefs.getBool("active", false)) {
        return;  // No active OTA
    }

    // Restore session
    ota_session.session_id = prefs.getUInt("session_id", 0);
    ota_session.state = prefs.getUChar("state", OTA_STATE_IDLE);
    // ... load all fields ...

    // Restore bitmap
    size_t bitmap_size = OTA_BITMAP_SIZE(ota_session.total_chunks);
    ota_session.chunk_bitmap = (uint8_t*)malloc(bitmap_size);
    prefs.getBytes("bitmap", ota_session.chunk_bitmap, bitmap_size);

    prefs.end();

    log_event("OTA: Resuming session " + String(ota_session.session_id, HEX));
}
```

### Surviving Network Partitions

**Problem:** Network splits during OTA (nodes A-B isolated from C-D).

**Solution:**
- Each partition continues gossiping internally
- Chunks propagate within each partition
- When partitions heal (nodes move, repeater restored):
  - STATUS broadcasts reveal chunks each side has
  - REQUESTs flow across partition boundary
  - Missing chunks transfer, both sides complete

**No action needed** - gossip protocol naturally handles this.

### Surviving Power Loss

**Problem:** Power loss during chunk write corrupts flash.

**Solution:**
- ESP32 Update API uses atomic sector writes
- Incomplete writes detected by partition metadata
- NVS bitmap checkpointed every 50 chunks
- Worst case: lose last 50 chunks of progress, re-request them

## CLI Commands

### ota generate-keypair

Generates Ed25519 keypair for firmware signing.

```bash
meshgrid-cli ota generate-keypair --output release_keys.json
```

**Output (release_keys.json):**
```json
{
  "private_key": "8a4f3c2b1d...e9a7 (128 hex chars = 64 bytes)",
  "public_key": "1a2b3c4d5e...f1a2 (64 hex chars = 32 bytes)",
  "created_at": "2026-01-08T19:00:00Z"
}
```

**Security:** Store `release_keys.json` offline, encrypted, with backup.

### ota sign

Signs firmware binary, creates OTA manifest.

```bash
meshgrid-cli ota sign firmware.bin \
  --keys release_keys.json \
  --version 0.0.2 \
  --output firmware-0.0.2.signed
```

**Output:** Binary file containing:
```
[MANIFEST (183 bytes)]
[FIRMWARE (N bytes)]
```

### ota push

Initiates mesh OTA update via connected device.

```bash
meshgrid-cli ota push firmware-0.0.2.signed --port /dev/ttyUSB0 [--rate slow|medium|fast]
```

**Workflow:**
1. Parse signed firmware file
2. Send MANIFEST via serial (floods network)
3. Wait 30 seconds (manifest propagation)
4. Stream chunks 0→N via serial
5. Show progress:
   ```
   Pushing OTA update: v0.0.2 (492 KB, 2808 chunks)

   [==============>           ] 1500/2808 chunks (53.4%)

   Network Status:
   - 0x31 (mg-E4): 1500/2808 (53.4%) - Downloading
   - 0x1a (NL-ENS): 1450/2808 (51.6%) - Downloading
   - 0xA7 (mg-A7): 1502/2808 (53.5%) - Downloading

   Est. Time Remaining: 12m 30s
   ```

**Rate options:**
- `--rate slow`: 200ms between chunks (safe, regulatory compliant)
- `--rate medium`: 100ms between chunks (default)
- `--rate fast`: 50ms between chunks (maximum speed, may exceed duty cycle)

### ota status

Queries OTA status from connected device.

```bash
meshgrid-cli ota status --port /dev/ttyUSB0
```

**Output:**
```
OTA Status:
  Session:  0xABCD1234
  Version:  0.0.2-git.abc123
  State:    Downloading
  Progress: 1500/2808 chunks (53.4%)
  Battery:  75% (sufficient for auto-apply)
  Error:    None

  Started:  2026-01-08 19:00:15
  Updated:  2026-01-08 19:12:45 (2s ago)
```

### ota abort

Cancels active OTA session.

```bash
meshgrid-cli ota abort --session 0xABCD1234 --port /dev/ttyUSB0
```

Sends OTA_ABORT packet (floods network), all nodes discard session state.

## Performance Characteristics

### Timing Estimates

**For 492KB firmware (2,808 chunks @ 174 bytes):**

| Scenario | Duration | Notes |
|----------|----------|-------|
| CLI → single device (serial) | 8-15 min | Limited by serial/flash write speed |
| 2 nodes (direct link) | 10-20 min | Includes verification time |
| 5 nodes (2-hop max) | 15-30 min | Gossip + multi-hop delays |
| 10 nodes (3-hop max) | 25-45 min | Full mesh propagation |
| 50 nodes (5-hop max) | 1-2 hours | Large network, may need optimization |

**Bottlenecks:**
1. **Airtime budget**: 33% duty cycle limits TX rate
2. **Flash write**: ~50ms per chunk write to OTA partition
3. **Mesh propagation**: Multi-hop delays accumulate

### Bandwidth Usage

**Per-node traffic:**
- Receive: ~492KB (firmware) + overhead = ~600KB
- Transmit (forwarding): ~300-400KB (partial rebroadcast)
- Total: ~1MB per node

**Network-wide (10 nodes):**
- Total mesh traffic: ~8-10MB
- Overhead: 1.6-2x firmware size (acceptable for gossip protocol)

### Memory Footprint

**RAM usage:**
```
ota_session struct:       ~200 bytes
chunk_bitmap:             ~351 bytes (2808 chunks / 8)
pending_chunks buffer:    ~1740 bytes (10 chunks × 174 bytes)
Total OTA RAM:            ~2.3 KB
```

**Flash usage:**
- OTA code: ~15-20KB
- Release pubkey: 32 bytes
- NVS storage: ~500 bytes per session

**Total overhead:** <25KB (negligible)

## Implementation Roadmap

### Phase 1: Foundation

**Files to Create:**
- `ota/ota.h` - Core structures, enums, API declarations
- `ota/ota.cpp` - Initialization and main loop hook
- `ota/ota_state.cpp` - State machine implementation
- `ota/ota_storage.cpp` - NVS persistence layer
- `partitions_ota.csv` - ESP32 partition table

**Deliverable:** OTA subsystem compiles, state machine works, NVS persistence tested.

### Phase 2: Protocol

**Files to Create:**
- `ota/ota_packets.cpp` - Encode/decode MANIFEST, CHUNK, REQUEST, STATUS
- `ota/ota_crypto.cpp` - Signature verification wrapper

**Files to Modify:**
- `app/messaging.cpp` - Add PAYLOAD_MULTIPART handler
- `net/protocol.h` - Add OTA subtype definitions

**Deliverable:** Can receive and parse OTA packets, signature verification works.

### Phase 3: ESP32 Update Integration

**Files to Create:**
- `ota/ota_update.cpp` - Update.h wrapper with chunk buffering

**Files to Modify:**
- `main.cpp` - Add boot validation and rollback logic
- `platformio.ini` - Add partition table config

**Deliverable:** Can write chunks to OTA partition, rollback works.

### Phase 4: Distribution

**Files to Create:**
- `ota/ota_distribution.cpp` - Flood/gossip logic, request handling

**Files to Modify:**
- `app/messaging.cpp` - Integrate OTA packet forwarding

**Deliverable:** Multi-node OTA works, chunks propagate via gossip.

### Phase 5: CLI Tools

**Files to Create (meshgrid-cli):**
- `src/ota/mod.rs` - OTA module entry
- `src/ota/keys.rs` - Keypair generation
- `src/ota/signing.rs` - Firmware signing with Ed25519
- `src/ota/push.rs` - Push command with progress UI
- `src/ota/status.rs` - Status monitoring

**Files to Modify:**
- `src/main.rs` - Add OTA subcommand group

**Deliverable:** Complete CLI workflow (generate-keypair → sign → push → monitor).

### Phase 6: Testing & Hardening

**Test Coverage:**
- Unit tests for bitmap, packets, crypto
- 2-node direct OTA
- 5-node multi-hop OTA
- Node failure scenarios
- Low battery deferral
- Rollback validation

**Deliverable:** Production-ready OTA system for ESP32 devices.

## Future Optimizations

### Delta Updates (Phase 2)

**Goal:** Reduce transfer size by 70-90%.

**Approach:**
- Use bsdiff algorithm (binary diff/patch)
- Requires: old firmware + patch → new firmware
- ESP32 must read current partition, apply patch to new partition

**Complexity:**
- Need bsdiff library (port to embedded C)
- Requires 2 partition reads simultaneously
- Patch verification more complex

**Savings:**
- Typical firmware update: 492KB → 50KB delta
- Chunks: 2,808 → 288 chunks
- Time: 30 minutes → 5 minutes

### Compression (Phase 2)

**Goal:** Further reduce transfer size.

**Options:**
- GZIP: ~50% compression, widely supported
- LZ4: ~40% compression, faster decompression
- ZSTD: ~55% compression, best ratio

**Integration:**
- Compress chunks before sending (CLI-side)
- Decompress on-device before writing to OTA partition
- Minimal RAM overhead (streaming decompression)

### Automatic Update Checking (Phase 3)

**Goal:** Devices self-discover new firmware availability.

**Approach:**
- ROOM nodes periodically check update server (MQTT/HTTP)
- Broadcast availability via CONTROL packets
- Clients poll ROOM for updates
- User can disable auto-updates via config

## Comparison with Competitors

| Feature | MeshCore | Meshtastic | Meshgrid |
|---------|----------|------------|----------|
| USB OTA | ✅ Yes | ✅ Yes | ✅ Yes |
| WiFi OTA | ❌ No | ✅ Yes | 🔄 Planned |
| **Mesh OTA** | **❌ No** | **❌ No** | **✅ Planned** |
| OTA Rollback | N/A | ✅ Yes | ✅ Yes |
| Delta Updates | N/A | ❌ No | 🔄 Planned |
| Multi-platform | ESP32 | All | ESP32 (+ nRF planned) |

**Unique Selling Point:** Only MeshCore-compatible firmware with distributed mesh OTA capability. Critical for "war mesh" deployments where devices are inaccessible.

## Security Considerations

### Attack Vectors

**1. Malicious Firmware Injection**
- **Attack:** Adversary floods fake MANIFEST with malware
- **Defense:** Ed25519 signature verification (only release key accepted)
- **Result:** Attack fails (signature invalid)

**2. Replay Attacks**
- **Attack:** Replay old firmware MANIFEST to downgrade devices
- **Defense (future):** Add timestamp to signature, reject old manifests
- **Current:** Version check could reject downgrades

**3. Man-in-the-Middle**
- **Attack:** Intercept and modify chunks in transit
- **Defense:** Final SHA-256 verification catches any modifications
- **Result:** OTA rejected if hash doesn't match

**4. Denial of Service**
- **Attack:** Flood mesh with fake OTA packets
- **Defense:** Signature verification rejects invalid manifests quickly
- **Impact:** Minimal (invalid packets dropped early)

**5. Key Compromise**
- **Attack:** Attacker obtains release private key
- **Defense:** Keep release key offline, use secure key ceremony
- **Mitigation:** Key rotation mechanism (future enhancement)

### Best Practices

1. **Release Key Security:**
   - Generate offline on air-gapped system
   - Store in hardware security module (HSM) or encrypted vault
   - Never commit to git or share
   - Use key ceremony for critical releases

2. **Firmware Validation:**
   - Build reproducibility (deterministic builds)
   - Code signing in CI/CD pipeline
   - Multiple signatures for critical updates (threshold signing - future)

3. **Version Policy:**
   - Semantic versioning (MAJOR.MINOR.PATCH)
   - Reject downgrades (prevent rollback attacks)
   - Maintain changelog with security notices

## Open Questions

1. **Chunk size optimization:** 174 bytes vs. smaller (more chunks) or larger (fewer chunks)?
2. **Request strategy:** Aggressive (fast completion) vs. polite (less bandwidth)?
3. **Version comparison:** Strict semver or allow any newer version?
4. **Rollback policy:** Auto-rollback only on boot failure, or also on runtime errors?
5. **Flash wear limits:** How many OTA cycles before NVS wear concerns?

## References

- ESP-IDF OTA Documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html
- Ed25519 RFC: https://www.rfc-editor.org/rfc/rfc8032
- LoRa Airtime Calculator: https://www.loratools.nl/#/airtime
- MeshCore Protocol: `docs/MESHCORE_PROTOCOL_REFERENCE.md`

## Status

**Current:** Design/specification phase
**Next Step:** Implement Phase 1 (Foundation) when ready
**Target Release:** Meshgrid v0.1.0
**Priority:** High - unique differentiator vs. MeshCore
