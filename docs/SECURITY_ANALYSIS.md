# MeshCore Protocol Security Analysis

**Date:** 2026-01-12
**Status:** Research findings for future meshgrid improvements
**Purpose:** Document security flaws inherited from MeshCore compatibility layer

---

## Executive Summary

This document analyzes the security design of the MeshCore mesh networking protocol. Since meshgrid implements MeshCore compatibility for interoperability, we inherit these vulnerabilities when communicating with MeshCore nodes. This analysis identifies critical flaws that should be addressed in future meshgrid protocol versions.

**Architecture:**
meshgrid uses a **versioned encryption system** where the packet header indicates which encryption scheme to use:
- **MeshCore encryption (v0)**: Default baseline, never changes, always supported
- **meshgrid v1 encryption**: Improved security (future implementation)
- **meshgrid v2 encryption**: Future upgrades as standards evolve

Devices negotiate the highest common version they both support, falling back to MeshCore encryption (v0) for maximum compatibility.

**Key Findings:**
- 🔴 **5 Critical vulnerabilities** (authentication broken, identity spoofing possible)
- 🟠 **8 High severity issues** (buffer overflows, DoS vectors, replay attacks)
- 🟡 **6 Medium severity issues** (information leakage, timing attacks)

---

## 1. CRITICAL VULNERABILITIES

### 1.1 Two-Byte MAC is Catastrophically Weak

**Severity:** CRITICAL
**File:** `MeshCore.h:15`
**Code:**
```c
#define CIPHER_MAC_SIZE      2    // Only 2 bytes!
```

**Description:**
The Message Authentication Code (MAC) uses only 2 bytes (16 bits), providing minimal authentication security. An attacker can forge messages with a 1/65,536 (0.0015%) success rate per attempt.

**Impact:**
- Brute force attack: 1 million attempts = 95% success rate
- Any node can inject forged encrypted messages
- No reliable message authentication
- Cannot trust sender identity

**Attack Scenario:**
1. Attacker captures encrypted message from node A to node B
2. Modifies encrypted payload (flips bits)
3. Brute forces 2-byte MAC (averages 32,768 attempts)
4. Node B accepts forged message as authentic
5. Result: Arbitrary message injection

**Recommendation:**
- Use 16-byte HMAC-SHA256 (full output)
- Or use authenticated encryption (AES-GCM with 16-byte tag)

---

### 1.2 One-Byte Node Hash - Identity Spoofing

**Severity:** CRITICAL
**File:** `MeshCore.h:16`
**Code:**
```c
#define PATH_HASH_SIZE       1    // Only 1 byte!
```

**Description:**
Node identities are represented by a 1-byte hash of their public key. With only 256 possible values, hash collisions occur with high probability in small networks.

**Impact:**
- Birthday paradox: 50% collision probability with just 17 nodes
- Identity spoofing: Attacker creates node with matching hash
- Route poisoning: Packets routed to wrong destination
- Path corruption: Cannot reliably build return paths
- Neighbor table confusion: Multiple nodes share same hash

**Attack Scenario:**
1. Target victim has hash 0x52
2. Attacker generates keypairs until hash matches: `hash(pubkey) & 0xFF == 0x52`
3. Attacker broadcasts with stolen identity
4. Network cannot distinguish attacker from victim
5. Messages intended for victim are routed to attacker

**Mathematics:**
- Collision probability: P(n) = 1 - e^(-n²/2m) where m=256
- 17 nodes: 50% collision
- 32 nodes: 95% collision
- 64 nodes: 99.9% collision

**Recommendation:**
- Use minimum 4-byte hash (32 bits) → collision at ~77,000 nodes
- Meshgrid v1: Use 2-byte hash (65,536 values) → collision at ~300 nodes

---

### 1.3 AES-128 ECB Mode - Pattern Leakage

**Severity:** CRITICAL
**File:** `Utils.cpp:30-61`
**Code:**
```c
int Utils::decrypt(const uint8_t* shared_secret, uint8_t* dest,
                   const uint8_t* src, int src_len) {
  AES128 aes;
  aes.setKey(shared_secret, CIPHER_KEY_SIZE);

  while (sp - src < src_len) {
    aes.decryptBlock(dp, sp);    // ECB mode - each block independent
    dp += 16; sp += 16;
  }
  return sp - src;
}
```

**Description:**
Electronic Codebook (ECB) mode encrypts each 16-byte block independently with no initialization vector (IV). Identical plaintext blocks always produce identical ciphertext blocks.

**Impact:**
- Message structure visible even when encrypted
- Repeated 16-byte patterns are distinguishable
- Known plaintext attacks easier
- Encrypted images show clear patterns (famous ECB penguin)
- Cannot hide message lengths or repetition

**Visual Example:**
```
Plaintext:  "HELLO WORLD!!!!!" "HELLO WORLD!!!!!" "GOODBYE FRIEND!!"
Ciphertext: [block A........] [block A........] [block B........]
                    ↑ Same ciphertext reveals same plaintext
```

**Attack Scenario:**
1. Attacker observes encrypted messages
2. Notices repeated 16-byte ciphertext blocks
3. Learns message has repeated structure
4. Correlates patterns across multiple messages
5. Gains partial plaintext recovery

**Recommendation:**
- Use AES-GCM (authenticated encryption with associated data)
- Or AES-CTR mode with unique nonce per message
- Never reuse nonce with same key

---

### 1.4 No Replay Protection

**Severity:** CRITICAL
**File:** `Mesh.cpp:122-177`
**Code:**
```c
case PAYLOAD_TYPE_TXT_MSG: {
  uint8_t dest_hash = pkt->payload[i++];
  uint8_t src_hash = pkt->payload[i++];
  uint8_t* macAndData = &pkt->payload[i];

  int len = Utils::MACThenDecrypt(secret, data, macAndData,
                                   pkt->payload_len - i);
  if (len > 0) {
    // Message accepted! No timestamp validation here
    onPeerDataRecv(pkt, pkt->getPayloadType(), j, secret, data, len);
  }
}
```

**Description:**
The core mesh protocol has ZERO replay attack protection. While encrypted messages contain timestamps, validation only occurs at the application layer (`BaseChatMesh`), not the protocol layer.

**Impact:**
- Captured messages can be replayed indefinitely
- Attacker doesn't need to decrypt messages
- Old commands can be re-executed (door unlock, device control)
- Different routing path = different packet hash = not detected as duplicate
- Replay attacks work across years if message is preserved

**Attack Scenario:**
1. Attacker captures "unlock door" encrypted message in 2025
2. Message is properly encrypted, attacker cannot read it
3. In 2026, attacker replays exact same packet
4. Protocol accepts it (different path = different hash)
5. Application may accept it (if timestamp check is weak)
6. Door unlocks without valid current authorization

**Why Packet Deduplication Doesn't Help:**
- Duplicate detection uses packet hash including path
- Replayed message takes different route = different hash
- Only protects against immediate re-broadcast, not replay attack

**Recommendation:**
- Add 4-byte sequence number per conversation
- Reject messages with sequence ≤ last_received_seq
- Store last sequence number in NVS
- Use nonce in authenticated encryption (AES-GCM)

---

### 1.5 Zero Padding Without Length Field

**Severity:** CRITICAL
**File:** `Utils.cpp:53-59`
**Code:**
```c
if (src_len > 0) {  // remaining partial block
  uint8_t tmp[16];
  memset(tmp, 0, 16);           // Zero padding
  memcpy(tmp, src, src_len);
  aes.encryptBlock(dp, tmp);
  dp += 16;
}
```

**Description:**
Plaintext is padded to 16-byte boundary using zero bytes. No length field indicates the original plaintext size, making it impossible to distinguish padding from legitimate data.

**Impact:**
- Padding oracle attack possible
- Multiple plaintexts can produce identical ciphertext
- Decrypted data has trailing zeros (ambiguous)
- Cannot reliably recover original message length
- Data corruption if application expects exact length

**Attack Scenario (Padding Oracle):**
1. Attacker captures ciphertext
2. Modifies last block, tries decryption
3. Observes if device accepts or rejects (timing, error messages)
4. Device rejection reveals padding validity
5. Repeat with different modifications
6. Decrypt entire message without key (padding oracle attack)

**Example Problem:**
```
Plaintext:  "hello" (5 bytes)
Padded:     "hello\0\0\0\0\0\0\0\0\0\0\0" (16 bytes)
Decrypted:  "hello\0\0\0\0\0\0\0\0\0\0\0" (but where does padding start?)

If original was "hello\0\0\0", decryption produces same result!
```

**Recommendation:**
- Use PKCS#7 padding (RFC 5652)
- Or include explicit length field before encryption
- Or use authenticated encryption with length (AES-GCM)

---

## 2. HIGH SEVERITY VULNERABILITIES

### 2.1 TRACE Packet Buffer Overflow

**Severity:** HIGH
**File:** `Mesh.cpp:671-673`
**Code:**
```c
if (packet->getPayloadType() == PAYLOAD_TYPE_TRACE) {
  // For TRACE packets, path is appended to end of PAYLOAD
  memcpy(&packet->payload[packet->payload_len], path, path_len);
  packet->payload_len += path_len;  // NO BOUNDS CHECK!
  packet->path_len = 0;
}
```

**Description:**
When creating TRACE packets, the path is appended to the payload without verifying buffer capacity. The `payload` array has a fixed size (184 bytes), but no check ensures `payload_len + path_len` doesn't exceed this.

**Impact:**
- Stack buffer overflow
- Memory corruption
- Remote code execution possible
- Crash/reboot of device

**Attack Scenario:**
1. Attacker crafts TRACE packet with `payload_len = 100`
2. Attacker provides `path_len = 100` (route through 100 nodes)
3. `memcpy` writes 100 bytes starting at `payload[100]`
4. Total: 200 bytes written to 184-byte buffer
5. Overflow corrupts stack → RCE or crash

**Recommendation:**
```c
if (packet->payload_len + path_len > MAX_PACKET_PAYLOAD) {
  return NULL;  // Reject oversized packet
}
memcpy(&packet->payload[packet->payload_len], path, path_len);
```

---

### 2.2 Packet Flooding - No Rate Limiting

**Severity:** HIGH
**File:** `Dispatcher.cpp` (entire file - no rate limiting present)

**Description:**
The mesh protocol has no rate limiting, throttling, or DoS protection mechanisms. Any node can flood the network with unlimited packets.

**Impact:**
- Network-wide denial of service
- All nodes re-broadcast flood packets
- Packet pool exhaustion
- Radio channel saturation
- Battery drain on all devices
- Network becomes unusable

**Attack Scenario:**
1. Attacker sends 1000 flood packets per second
2. Every node re-broadcasts each packet
3. 10-node network: 10,000 packets/second total traffic
4. Radio bandwidth exhausted
5. Legitimate messages cannot get through
6. All devices drain battery re-broadcasting garbage

**Recommendation:**
- Implement per-source rate limiting (max packets/second)
- Add backoff algorithm for flood packets
- Limit total forwarding rate per device
- Implement priority queues (ACKs > data > floods)
- Add reputation system (blacklist flooding nodes)

---

### 2.3 Duplicate Packet Table Bypass

**Severity:** HIGH
**File:** `SimpleMeshTables.h:43-81`
**Code:**
```c
bool hasSeen(const mesh::Packet* packet) override {
  // ... calculate 8-byte hash ...

  const uint8_t* sp = _hashes;
  for (int i = 0; i < MAX_PACKET_HASHES; i++, sp += MAX_HASH_SIZE) {
    if (memcmp(hash, sp, MAX_HASH_SIZE) == 0) {
      return true;    // Already seen
    }
  }

  // Store new hash
  memcpy(&_hashes[_next_idx*MAX_HASH_SIZE], hash, MAX_HASH_SIZE);
  _next_idx = (_next_idx + 1) % MAX_PACKET_HASHES;  // Cyclic!
  return false;
}
```

**Description:**
The duplicate packet detection table only stores the last 128 packet hashes in a cyclic buffer. After 129 new packets, the oldest hash is overwritten and forgotten.

**Impact:**
- Replay attack: Send 129 new packets, then replay old packet
- Old packet hash is forgotten → treated as new
- Infinite packet replay possible
- Network resources consumed by repeated packets
- Can force re-broadcast of attacker's chosen packets

**Attack Scenario:**
1. Attacker saves interesting packet (e.g., door unlock command)
2. Attacker sends 129 junk flood packets
3. Oldest entry in hash table is overwritten
4. Attacker replays saved packet
5. Network treats it as new, re-broadcasts it
6. Repeat indefinitely

**Additional Issues:**
- No TTL/timeout on stored hashes
- Hash collision (8-byte hash) can overwrite unrelated entry
- No garbage collection of old entries

**Recommendation:**
- Increase table size to 1024+ entries
- Add timestamp to each entry, expire after 60 seconds
- Use 16-byte hash to prevent collisions
- Implement LRU eviction instead of cyclic overwrite

---

### 2.4 Out-of-Bounds Path Deserialization

**Severity:** HIGH
**File:** `Mesh.cpp:150-151`
**Code:**
```c
uint8_t path_len = data[k++];
uint8_t* path = &data[k]; k += path_len;  // No bounds check!
```

**Description:**
When deserializing PATH packets, `path_len` is read from untrusted encrypted data without validating it fits within the remaining buffer.

**Impact:**
- Out-of-bounds read beyond buffer
- Stack/heap memory disclosure
- Information leakage (addresses, keys, other data)
- Potential crash if accessing unmapped memory

**Attack Scenario:**
1. Attacker sends encrypted PATH packet
2. Sets `path_len = 255` in encrypted payload
3. Remaining buffer only has 20 bytes
4. Code reads 255 bytes starting at `data[k]`
5. Reads past end of buffer into adjacent stack memory
6. Memory contents leaked to attacker via subsequent operations

**Recommendation:**
```c
uint8_t path_len = data[k++];
if (k + path_len > len) {
  return;  // Invalid packet
}
uint8_t* path = &data[k];
k += path_len;
```

---

### 2.5 Integer Overflow in Packet Size Check

**Severity:** HIGH
**File:** `Dispatcher.cpp:263-264`
**Code:**
```c
if (len + outbound->payload_len > MAX_TRANS_UNIT) {
  MESH_DEBUG_PRINTLN("FATAL: packet too large");
}
```

**Description:**
If `len` or `payload_len` are very large unsigned integers, their sum can wrap around (integer overflow), bypassing the size check.

**Impact:**
- Size check bypassed
- Buffer overflow when allocating packet
- Heap corruption
- Potential remote code execution

**Attack Scenario:**
1. Attacker sets `len = 0xFFFFFF00` (near max uint32)
2. Attacker sets `payload_len = 200`
3. Calculation: `0xFFFFFF00 + 200 = 0x000000C8` (wraps to 200)
4. Check: `200 > MAX_TRANS_UNIT` (1024) → FALSE (passes)
5. Allocate 200-byte buffer
6. Copy 0xFFFFFF00 bytes → massive overflow

**Recommendation:**
```c
if (len > MAX_TRANS_UNIT || outbound->payload_len > MAX_TRANS_UNIT) {
  return;  // Individual components too large
}
if (len + outbound->payload_len > MAX_TRANS_UNIT) {
  return;  // Combined too large
}
```

---

### 2.6 Inadequate Packet Hash (8-byte collision)

**Severity:** HIGH
**File:** `SimpleMeshTables.h:63-68`
**Code:**
```c
#define MAX_HASH_SIZE 8  // Only 8 bytes of SHA256
uint8_t _hashes[MAX_PACKET_HASHES*MAX_HASH_SIZE];

for (int i = 0; i < MAX_PACKET_HASHES; i++, sp += MAX_HASH_SIZE) {
  if (memcmp(hash, sp, MAX_HASH_SIZE) == 0) {
    return true;  // Duplicate detected
  }
}
```

**Description:**
Packet deduplication uses only 8 bytes (64 bits) of SHA256 hash. With 128 stored hashes, birthday collision probability is approximately 0.2%.

**Impact:**
- Hash collision causes false duplicate detection
- Legitimate new packet rejected as "already seen"
- Attacker can force collisions to block packets
- Different packets treated as duplicates

**Mathematics:**
- 64-bit hash space: 2^64 = ~18 quintillion values
- Birthday collision at ~sqrt(2^64) = 4 billion packets
- With 128 stored: collision probability ≈ 128²/(2*2^64) ≈ 0.0002%
- Low but non-zero; attackers can force collisions

**Attack Scenario:**
1. Attacker computes 8-byte hash of target packet: `hash(packet_A) = 0xAABBCCDDEEFF0011`
2. Attacker generates packets until finding collision: `hash(packet_B) = 0xAABBCCDDEEFF0011`
3. Attacker floods network with packet_B
4. When packet_A arrives, it's rejected as duplicate
5. Target packet never reaches destination

**Recommendation:**
- Use full 32-byte SHA256 hash (no collision risk)
- Or use 16-byte hash (collision at 2^64 packets)

---

### 2.7 Heap Exhaustion - Unbounded Multipart Fragments

**Severity:** HIGH
**File:** `Mesh.cpp:390-395`
**Code:**
```c
Packet* packet = obtainNewPacket();
if (packet == NULL) {
  MESH_DEBUG_PRINTLN("error, packet pool empty");
  return NULL;
}
```

**Description:**
When receiving multipart packets, each fragment consumes a packet from the limited pool. No timeout or garbage collection removes incomplete fragments, allowing heap exhaustion.

**Impact:**
- Denial of service
- Attacker sends many multipart packets, never completes them
- Packet pool exhausted
- Legitimate packets cannot be allocated
- Device becomes unusable

**Attack Scenario:**
1. Packet pool has 16 slots
2. Attacker sends 16 multipart "part 1 of 10" packets
3. Each fragment allocates a packet
4. Attacker never sends remaining parts
5. All 16 slots consumed by incomplete messages
6. Legitimate packets fail to allocate → DoS

**Recommendation:**
- Add fragment timeout (30 seconds)
- Garbage collect incomplete fragments
- Limit fragments per sender
- Implement fragment reassembly timeout

---

### 2.8 Group Channel Hash Collision

**Severity:** HIGH
**File:** `Mesh.cpp:206-230`
**Code:**
```c
case PAYLOAD_TYPE_GRP_TXT: {
  uint8_t channel_hash = pkt->payload[i++];

  // Scan for matching channels (max 4 matches)
  GroupChannel channels[4];
  int num = searchChannelsByHash(&channel_hash, channels, 4);

  for (int j = 0; j < num; j++) {
    int len = Utils::MACThenDecrypt(channels[j].secret, data,
                                    macAndData, pkt->payload_len - i);
    if (len > 0) {
      // Decryption succeeded
      onGroupDataRecv(pkt, pkt->getPayloadType(), channels[j], data, len);
      break;
    }
  }
}
```

**Description:**
Group channel identifiers use 1-byte hash. With only 256 possible values, collisions are guaranteed even with a small number of channels. The code tries to decrypt with each matching channel's secret.

**Impact:**
- Channel hash collision forces incorrect decryption attempts
- Decryption with wrong secret produces garbage
- Information leakage if garbage is processed
- Denial of service if many collisions
- Cannot scale beyond ~15 channels per node (birthday collision)

**Attack Scenario:**
1. User is member of 4 channels: 0x3a, 0x78, 0xd9, 0xca
2. Attacker creates channel with hash collision: 0x3a
3. Attacker sends message to their 0x3a channel
4. Victim's device tries to decrypt with victim's 0x3a secret
5. Decryption produces garbage but may process it
6. Result: Resource waste, potential info leak

**Recommendation:**
- Use 2-byte channel hash (65,536 values)
- Or use 4-byte hash for scalability
- Add channel version field to disambiguate

---

## 3. MEDIUM SEVERITY VULNERABILITIES

### 3.1 Non-Constant-Time MAC Comparison

**Severity:** MEDIUM
**File:** `Utils.cpp:84`
**Code:**
```c
if (memcmp(hmac, src, CIPHER_MAC_SIZE) == 0) {
  return decrypt(shared_secret, dest, src + CIPHER_MAC_SIZE,
                 src_len - CIPHER_MAC_SIZE);
}
```

**Description:**
`memcmp()` returns immediately on first byte mismatch. Attacker can measure comparison duration to determine how many bytes matched.

**Impact:**
- Timing attack reveals MAC bytes one at a time
- Attacker forges MAC through timing analysis
- With 2-byte MAC, only 512 timing measurements needed (256 per byte)

**Attack Scenario:**
1. Attacker sends packet with MAC = 0x0000
2. Measures comparison time: 5μs (failed on byte 0)
3. Tries MAC = 0x0100, 0x0200, ..., 0xFF00
4. Finds MAC = 0xAB00 takes 8μs (matched byte 0)
5. Tries MAC = 0xAB01, 0xAB02, ..., 0xABFF
6. Finds MAC = 0xABCD takes 10μs (matched both bytes)
7. Full MAC recovered: 0xABCD

**Recommendation:**
```c
bool constant_time_compare(const uint8_t* a, const uint8_t* b, size_t len) {
  uint8_t diff = 0;
  for (size_t i = 0; i < len; i++) {
    diff |= a[i] ^ b[i];
  }
  return diff == 0;
}
```

---

### 3.2 Weak ACK CRC (32-bit)

**Severity:** MEDIUM
**File:** `Mesh.cpp:540-551`
**Code:**
```c
Packet* Mesh::createAck(uint32_t ack_crc) {
  Packet* packet = obtainNewPacket();
  memcpy(packet->payload, &ack_crc, 4);
  packet->payload_len = 4;
  return packet;
}
```

**Description:**
ACK packets use 32-bit CRC/hash to identify which packet is being acknowledged. This provides only moderate security against forgery.

**Impact:**
- 1 in 4 billion chance of crafting false ACK
- Attacker can confuse sender about delivery status
- Denial of service by sending false ACKs
- Sender stops retransmitting, thinking message delivered

**Attack Scenario:**
1. Target sends important packet, expects ACK
2. Attacker intercepts but doesn't forward packet
3. Attacker computes CRC (or brute forces 4 billion values)
4. Attacker sends forged ACK packet
5. Target thinks delivery succeeded, stops retransmitting
6. Real recipient never receives message

**Recommendation:**
- Use 8-byte or 16-byte hash for ACK identification
- Or use sequence numbers instead of CRC

---

### 3.3 Weak Multipart Sequence Number

**Severity:** MEDIUM
**File:** `Mesh.cpp:278-302`
**Code:**
```c
case PAYLOAD_TYPE_MULTIPART:
  if (pkt->payload_len > 2) {
    uint8_t remaining = pkt->payload[0] >> 4;  // 4 bits (0-15)
    uint8_t type = pkt->payload[0] & 0x0F;
```

**Description:**
Multipart packets use 4-bit sequence numbers (0-15), with no message ID to correlate fragments. Fragments from different messages can be mixed.

**Impact:**
- Cannot handle messages with >15 fragments
- Fragments from different messages may be incorrectly reassembled
- No way to correlate which fragments belong together
- Garbage data delivered to application

**Attack Scenario:**
1. Alice sends multipart message A: [A1, A2, A3]
2. Bob sends multipart message B: [B1, B2, B3]
3. Both arrive interleaved: A1, B1, A2, B2, A3, B3
4. Receiver reassembles: [A1, B1, A2] → garbage
5. Application processes corrupted data

**Recommendation:**
- Add 4-byte message ID to each fragment
- Increase sequence to 8 bits (256 fragments)
- Implement fragment reassembly timeout

---

### 3.4 Information Leakage via Debug Logs

**Severity:** MEDIUM
**File:** `Mesh.cpp:172, 266`
**Code:**
```c
if (found) {
  pkt->markDoNotRetransmit();
} else {
  MESH_DEBUG_PRINTLN("recv matches no peers, src_hash=%02X",
                     (uint32_t)src_hash);
}

// ...

bool is_ok = id.verify(signature, message, msg_len);
if (!is_ok) {
  MESH_DEBUG_PRINTLN("received advertisement with forged signature! (app_data_len=%d)",
                     app_data_len);
}
```

**Description:**
Debug logs expose internal protocol details including sender hashes that failed decryption and size of forged advertisement data.

**Impact:**
- Attacker learns network topology (which nodes are known)
- Attacker learns message sizes before authentication
- Brute force attacks revealed through failed decryption logs
- Information leakage aids further attacks

**Recommendation:**
- Remove sensitive information from logs
- Or disable debug logs in production builds
- Use rate-limited generic error messages

---

### 3.5 Unbounded Path Accumulation

**Severity:** MEDIUM
**File:** `Mesh.cpp:328-330`
**Code:**
```c
if (packet->isRouteFlood() &&
    packet->path_len + PATH_HASH_SIZE <= MAX_PATH_SIZE &&
    allowPacketForward(packet)) {
  packet->path_len += self_id.copyHashTo(&packet->path[packet->path_len]);
```

**Description:**
Flood packets can accumulate up to 64 hops (MAX_PATH_SIZE = 64, PATH_HASH_SIZE = 1). No TTL or hop limit prevents packets from traversing indefinitely.

**Impact:**
- Very long paths consume memory
- Packets can loop in network topology errors
- Resource exhaustion with giant paths
- Increased packet size → more airtime → slower network

**Recommendation:**
- Add TTL field (e.g., max 16 hops)
- Decrement TTL at each hop
- Drop packet when TTL reaches 0

---

### 3.6 String-to-Path Conversion Data Loss

**Severity:** MEDIUM
**File:** `Mesh.cpp:149-154`
**Code:**
```c
uint8_t path_len = data[k++];
uint8_t* path = &data[k]; k += path_len;
uint8_t extra_type = data[k++] & 0x0F;
uint8_t* extra = &data[k];
uint8_t extra_len = len - k;   // Includes padding zeros!
```

**Description:**
After decryption, `extra_len` includes trailing zero-byte padding. Application cannot distinguish padding from real data.

**Impact:**
- Application receives extra data with trailing zeros
- Cannot determine original data length
- May process padding as legitimate data
- Information corruption

**Recommendation:**
- Store explicit length before encryption
- Or use PKCS#7 padding with length encoding

---

## 4. ATTACK SCENARIOS

### Scenario 1: Complete Network Takeover

**Attacker Goal:** Control all routing in network
**Steps:**
1. Exploit 1-byte hash collision: Generate node with hash matching target
2. Broadcast with higher power to dominate routing
3. Exploit weak MAC: Forge PATH responses with attacker's route
4. All nodes learn attacker's route as "best path" to target
5. All traffic to target now flows through attacker
6. Attacker can read, modify, or drop messages

**Defenses Bypassed:**
- Identity verification (1-byte hash collision)
- Message authentication (2-byte MAC forgery)
- Route validation (no cryptographic path validation)

---

### Scenario 2: Message Injection Attack

**Attacker Goal:** Send fake messages from Alice to Bob
**Steps:**
1. Capture legitimate encrypted message from Alice to Bob
2. Learn Alice's hash (0x52) and Bob's hash (0x12)
3. Craft new message with src=0x52, dst=0x12
4. Brute force 2-byte MAC (average 32,768 attempts = seconds on modern CPU)
5. Send forged message
6. Bob's device decrypts successfully, accepts as authentic

**Defenses Bypassed:**
- Message authentication (2-byte MAC brute forced)
- Sender verification (hash easily spoofed)
- Replay protection (none exists)

---

### Scenario 3: Denial of Service

**Attacker Goal:** Make network unusable
**Steps:**
1. Flood network with 1000 packets/second
2. Each node re-broadcasts (10 nodes = 10,000 pkt/s total)
3. Fill duplicate detection table with 129 junk packets
4. Replay old legitimate packets (table overflow allows it)
5. Exhaust packet pool with incomplete multipart messages
6. Network collapses under load

**Defenses Bypassed:**
- Rate limiting (none exists)
- Duplicate detection (table overflow)
- Resource limits (no fragment timeout)

---

## 5. MESHGRID IMPROVEMENTS (Future v1 Protocol)

### Short-Term (MeshCore Compatibility Mode)
- ✅ Document vulnerabilities (this file)
- ✅ Warn users about security limitations
- ⚠️ No breaking changes to maintain interoperability

### Medium-Term (meshgrid v0.x enhancements)
- Add rate limiting per source
- Implement fragment timeout and garbage collection
- Add bounds checking on all deserialization
- Fix buffer overflow in TRACE handler
- Increase duplicate detection table to 1024 entries
- Add TTL field to flood packets

### Long-Term (meshgrid v1 Protocol - Breaking Changes)

#### Cryptography Improvements
- **16-byte HMAC-SHA256** (vs 2-byte MAC)
- **AES-256-GCM** authenticated encryption (vs ECB mode)
- **2-byte node hash** (vs 1-byte), collision at ~300 nodes
- **4-byte sequence numbers** per conversation
- **Unique nonce** per encrypted message

#### Protocol Improvements
- **Replay protection**: Sequence numbers enforced at protocol layer
- **Rate limiting**: Max 10 flood packets/sec per source
- **TTL field**: Max 16 hops for flood packets
- **Fragment timeout**: 30 seconds for multipart reassembly
- **Proper padding**: PKCS#7 with length encoding
- **Constant-time comparisons** for all MAC/hash checks

#### Encryption Version System
The protocol uses a **version field** in the packet header to indicate which encryption scheme to use:

**MeshCore Encryption (v0)** - Default, never changes
- 2-byte MAC, 1-byte hash, AES-128 ECB
- Used for: MeshCore ↔ MeshCore, MeshCore ↔ meshgrid, meshgrid ↔ meshgrid (if either doesn't support v1+)
- Always supported for backward compatibility
- This is the baseline that remains fixed forever

**meshgrid v1 Encryption** - Improved security (future)
- 16-byte HMAC, 2-byte hash, AES-256-GCM
- 4-byte sequence numbers, replay protection
- Used when: Both endpoints advertise v1 support
- Packet header version field = 1

**meshgrid v2 Encryption** - Future upgrades
- TBD: Post-quantum crypto, enhanced features
- Used when: Both endpoints advertise v2 support
- Packet header version field = 2

**Version Negotiation:**
- Devices advertise highest version they support in ADVERT packets
- When sending, use highest common version both endpoints support
- Always fall back to MeshCore encryption (v0) if versions don't match
- Per-packet version field determines which encryption is used

**Example:**
- MeshCore device (supports: v0 only) ↔ meshgrid device (supports: v0, v1)
  → Uses MeshCore encryption (v0)
- meshgrid device (supports: v0, v1) ↔ meshgrid device (supports: v0, v1)
  → Uses meshgrid v1 encryption
- meshgrid future device (supports: v0, v1, v2) ↔ meshgrid device (supports: v0, v1)
  → Uses meshgrid v1 encryption
- meshgrid future device (supports: v0, v1, v2) ↔ meshgrid future device (supports: v0, v1, v2)
  → Uses meshgrid v2 encryption

---

## 6. RISK ASSESSMENT

### Current State (MeshCore Compatibility)

| Security Property | Status | Risk Level |
|---|---|---|
| **Confidentiality** | ⚠️ Weak | MEDIUM |
| - AES-128 key strength | ✅ Strong | LOW |
| - ECB mode pattern leakage | ❌ Broken | HIGH |
| **Integrity** | ❌ Broken | CRITICAL |
| - 2-byte MAC | ❌ Forgeable | CRITICAL |
| - Padding oracle | ❌ Exploitable | HIGH |
| **Authentication** | ❌ Broken | CRITICAL |
| - Node identity (1-byte hash) | ❌ Spoofable | CRITICAL |
| - Message sender verification | ❌ Weak | HIGH |
| **Replay Protection** | ❌ None | CRITICAL |
| **Availability** | ❌ Vulnerable | HIGH |
| - No rate limiting | ❌ DoS possible | HIGH |
| - Buffer overflows | ❌ Crash risk | HIGH |

### Future State (meshgrid v1)

| Security Property | Status | Risk Level |
|---|---|---|
| **Confidentiality** | ✅ Strong | LOW |
| - AES-256-GCM | ✅ Secure | LOW |
| **Integrity** | ✅ Strong | LOW |
| - 16-byte HMAC | ✅ Secure | LOW |
| - PKCS#7 padding | ✅ Secure | LOW |
| **Authentication** | ✅ Strong | LOW |
| - 2-byte hash | ✅ Adequate | LOW |
| - Verified sender | ✅ Secure | LOW |
| **Replay Protection** | ✅ Present | LOW |
| - Sequence numbers | ✅ Enforced | LOW |
| **Availability** | ✅ Protected | LOW |
| - Rate limiting | ✅ Implemented | LOW |
| - Input validation | ✅ Complete | LOW |

---

## 7. CONCLUSION

The MeshCore protocol is a **proof-of-concept research design** suitable for hobbyist applications and learning, but **not production-ready** for security-critical deployments. The combination of:
- 2-byte MAC (forgeable)
- 1-byte node hash (identity spoofing)
- No replay protection
- Multiple buffer overflows

creates a **critically insecure** foundation. While individual flaws are concerning, the cumulative effect is a protocol with **no reliable security guarantees**.

For meshgrid's future, we should:
1. **Always support MeshCore encryption (v0)** as the default and baseline - this never changes
2. **Implement meshgrid v1 encryption** for improved security when both endpoints support it
3. **Use version field in packets** to indicate which encryption scheme (v0, v1, v2, etc.)
4. **Negotiate per-conversation** using highest common version both devices support
5. **Default to MeshCore encryption (v0)** for maximum compatibility
6. **Future-proof with v2, v3, etc.** as encryption standards evolve

This analysis provides a roadmap for building a production-grade mesh protocol while maintaining backward compatibility with the hobbyist ecosystem.

---

**Document Version:** 1.0
**Last Updated:** 2026-01-12
**Contributors:** Security analysis team
**Status:** Research findings - not yet implemented
