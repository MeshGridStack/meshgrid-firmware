# MeshCore Protocol Quick Reference

**Last Updated:** 2026-01-08

---

## Advertisement Packet Structure

### Wire Format

```
┌─────────────────────────────────────────────────────────────┐
│ Header (2 bytes)                                            │
├─────────────────────────────────────────────────────────────┤
│ Path (0-64 bytes, variable)                                 │
├─────────────────────────────────────────────────────────────┤
│ Payload:                                                     │
│   - Public Key (32 bytes)                                   │
│   - Timestamp (4 bytes, little-endian)                      │
│   - Signature (64 bytes)                                    │
│   - App Data (variable):                                    │
│       • Flags byte (1 byte)                                 │
│       • Optional: lat/lon (8 bytes) if bit 4 set           │
│       • Optional: feat1 (2 bytes) if bit 5 set             │
│       • Optional: feat2 (2 bytes) if bit 6 set             │
│       • Name (remainder, NOT null-terminated) if bit 7 set  │
└─────────────────────────────────────────────────────────────┘
```

### Header Byte Layout

```
Byte 0:  [Route Type: 4 bits][Payload Type: 4 bits]
Byte 1:  [Version: 4 bits][Transport Codes: 4 bits]
```

**Route Types:**
- `0x00` - ROUTE_DIRECT (zero-hop, local only)
- `0x01` - ROUTE_FLOOD (multi-hop, network-wide)
- `0x02` - ROUTE_DATAGRAM (encrypted, point-to-point)
- `0x03` - ROUTE_GROUP (encrypted, group channel)

**Payload Types:**
- `0x00` - TEXT (plain text message)
- `0x01` - DATAGRAM_ANON (anonymous encrypted)
- `0x02` - GROUP_MSG (group message)
- `0x03` - ACK (acknowledgment)
- `0x04` - ADVERT (node advertisement) ← Most common
- `0x05` - PATH_RETURN (path discovery)
- `0x06` - CONTROL (control messages)

**Version:**
- `0x01` - PAYLOAD_VER_MESHCORE

### Flags Byte (App Data)

```
Bit 7 (0x80): ADV_NAME_MASK     - Name present (remainder of app_data)
Bit 6 (0x40): ADV_FEAT2_MASK    - Feature 2 present (2 bytes)
Bit 5 (0x20): ADV_FEAT1_MASK    - Feature 1 present (2 bytes)
Bit 4 (0x10): ADV_LATLON_MASK   - Lat/Lon present (8 bytes)
Bits 0-3:     Node Type         - See below
```

**Node Types:**
- `0x00` - ADV_TYPE_NONE
- `0x01` - ADV_TYPE_CHAT (regular clients)
- `0x02` - ADV_TYPE_REPEATER (mesh relays)
- `0x03` - ADV_TYPE_ROOM (servers)
- `0x04` - ADV_TYPE_SENSOR (IoT devices)

**Example Flags:**
- `0x81` = Has name + Chat type
- `0x82` = Has name + Repeater type
- `0x91` = Has name + Has lat/lon + Chat type

---

## Signature Algorithm

### What Gets Signed

```
message = pubkey (32 bytes) + timestamp (4 bytes) + app_data (variable)
```

**NOT signed:**
- Header
- Path
- Signature itself

### Algorithm

- **Type:** Ed25519
- **Signature Size:** 64 bytes
- **Public Key Size:** 32 bytes
- **Private Key Size:** 32 bytes

### C Code Example

```c
// Signing (transmission)
uint8_t message[PUB_KEY_SIZE + 4 + app_data_len];
int msg_len = 0;
memcpy(&message[msg_len], pubkey, PUB_KEY_SIZE);
msg_len += PUB_KEY_SIZE;
memcpy(&message[msg_len], &timestamp, 4);
msg_len += 4;
memcpy(&message[msg_len], app_data, app_data_len);
msg_len += app_data_len;

ed25519_sign(signature, message, msg_len, pubkey, privkey);

// Verification (reception)
bool valid = ed25519_verify(signature, message, msg_len, pubkey);
```

---

## Name Encoding Rules

### ⚠️ CRITICAL: Names Are NOT Null-Terminated

**Encoding (Transmission):**
```c
// ✅ CORRECT
uint8_t flags = 0x80 | node_type;  // Set name bit
app_data[0] = flags;
int i = 1;
// ... handle optional fields ...
memcpy(&app_data[i], name, name_len);
i += name_len;
// NO null terminator!
```

**Decoding (Reception):**
```c
// ✅ CORRECT
uint8_t flags = app_data[0];
int i = 1;
// Skip optional fields based on flags
if (flags & 0x10) i += 8;  // lat/lon
if (flags & 0x20) i += 2;  // feat1
if (flags & 0x40) i += 2;  // feat2

// Name is remainder of app_data
if (flags & 0x80) {
    int name_len = app_data_len - i;
    memcpy(name, &app_data[i], name_len);
    name[name_len] = '\0';  // Add terminator for C string use
}
```

### Common Mistake

```c
// ❌ WRONG - DO NOT DO THIS
memcpy(&app_data[i], name, name_len);
i += name_len;
app_data[i++] = '\0';  // ← This breaks MeshCore compatibility!
```

---

## Packet Size Examples

### Chat Client with 5-char Name

```
Total: 108 bytes
├─ Header: 2 bytes
├─ Path: 0 bytes (ROUTE_DIRECT)
└─ Payload: 106 bytes
   ├─ Public Key: 32 bytes
   ├─ Timestamp: 4 bytes
   ├─ Signature: 64 bytes
   └─ App Data: 6 bytes
      ├─ Flags: 1 byte (0x81)
      └─ Name: 5 bytes ("mg-E4")
```

### Repeater with 20-char Name + Lat/Lon

```
Total: 131 bytes
├─ Header: 2 bytes
├─ Path: 1 byte (ROUTE_FLOOD with own hash)
└─ Payload: 128 bytes
   ├─ Public Key: 32 bytes
   ├─ Timestamp: 4 bytes
   ├─ Signature: 64 bytes
   └─ App Data: 28 bytes
      ├─ Flags: 1 byte (0x92 = repeater + name + lat/lon)
      ├─ Lat/Lon: 8 bytes
      └─ Name: 20 bytes
```

---

## Radio Setup Requirements

### Critical: Interrupt Callback

**RadioLib requires explicit interrupt registration:**

```cpp
// 1. Define ISR
static volatile bool radio_interrupt_flag = false;

#if defined(ARCH_ESP32) || defined(ARCH_ESP32S3)
void ICACHE_RAM_ATTR radio_isr(void) {
    radio_interrupt_flag = true;
}
#else
void radio_isr(void) {
    radio_interrupt_flag = true;
}
#endif

// 2. Register callback
radio->setPacketReceivedAction(radio_isr);

// 3. Check flag in main loop
void loop() {
    if (radio_interrupt_flag) {
        radio_interrupt_flag = false;
        // Read packet...
        radio->startReceive();  // Re-enter RX mode
    }
}
```

**Without `setPacketReceivedAction()`, no packets will be received!**

### LoRa Parameters

**MeshCore Standard (EU868):**
```
Frequency: 869.618 MHz
Bandwidth: 62.5 kHz
Spreading Factor: 8
Coding Rate: 4/8
Sync Word: 0x12 (RADIOLIB_SX126X_SYNC_WORD_PRIVATE)
Preamble: 16 symbols
CRC: Enabled (mode 1)
Header: Explicit
```

---

## Repeater Behavior

### Neighbor Table Filtering

**Repeaters ONLY track other repeaters in neighbor table:**

```cpp
void onAdvertRecv(Packet *packet, Identity &id, uint32_t timestamp,
                  const uint8_t *app_data, size_t app_data_len) {
  if (packet->path_len == 0) {  // Zero-hop (direct)
    AdvertDataParser parser(app_data, app_data_len);
    if (parser.getType() == ADV_TYPE_REPEATER) {  // ← Filter!
      putNeighbour(id, timestamp, packet->getSNR());
    }
  }
}
```

**This is intentional design:**
- Repeaters form mesh topology
- Track signal quality between repeaters
- Clients/rooms tracked in separate contact table

### Two Tables

| Table | Purpose | Contents |
|-------|---------|----------|
| **Neighbor Table** | Mesh topology | Only repeaters, zero-hop only |
| **Contact Table** | All nodes | All types, any hop count |

**CLI Commands:**
- `neighbors` → Shows neighbor table (repeaters only)
- No standard CLI for contact table (internal use)

---

## Common Pitfalls

### 1. Forgetting Interrupt Callback
**Symptom:** No RX packets
**Fix:** Add `radio->setPacketReceivedAction(isr_function)`

### 2. Adding Null Terminator to Names
**Symptom:** MeshCore can't parse names
**Fix:** Don't add `\0` to wire format, use length-based encoding

### 3. Looking for Null Terminator
**Symptom:** Garbage in names
**Fix:** Calculate length as `app_data_len - offset`, don't search for `\0`

### 4. Expecting Chat Clients in Repeater Neighbors
**Symptom:** Repeater shows no neighbors
**Fix:** This is normal! Repeaters only track repeaters

### 5. Not Re-entering RX Mode
**Symptom:** Only receive one packet
**Fix:** Call `radio->startReceive()` after reading packet

### 6. Wrong Signature Message
**Symptom:** Signature verification fails
**Fix:** Sign `pubkey + timestamp + app_data`, not full packet

---

## Debugging Checklist

### RX Not Working?

- [ ] Called `radio->setPacketReceivedAction()`?
- [ ] ISR function marked `ICACHE_RAM_ATTR` (ESP32)?
- [ ] Checking interrupt flag in main loop?
- [ ] Calling `radio->startReceive()` after reading?
- [ ] Radio parameters match (freq, BW, SF, CR)?
- [ ] Antenna connected?

### Names Corrupted?

- [ ] Parsing based on length, not null terminator?
- [ ] Skipping optional fields (lat/lon, feat1, feat2)?
- [ ] Not searching for `\0` in wire format?
- [ ] Adding `\0` only to local C string buffer?

### Signature Fails?

- [ ] Signing correct bytes (pubkey + timestamp + app_data)?
- [ ] Not including header or path in signature?
- [ ] Timestamp in little-endian (memcpy on ESP32)?
- [ ] Using same Ed25519 library as MeshCore?

### Repeater Doesn't See Me?

- [ ] Check repeater log - is it receiving packets?
- [ ] Verify node type - repeaters only track repeaters
- [ ] This is normal for CHAT/ROOM types!

---

## Protocol Version Compatibility

### MeshCore Versions

- **V1 (PAYLOAD_VER_MESHCORE = 1)**: Current standard
- Signature required
- Ed25519 cryptography
- Name encoding as described

### MeshGrid Compatibility

✅ **Fully compatible** with MeshCore V1 protocol
- Same packet format
- Same signature algorithm
- Same name encoding
- Same node types

---

## Performance Notes

### Timing

- **Signature generation:** ~1-2 ms (ESP32-S3 @ 240 MHz)
- **Signature verification:** ~1-2 ms
- **Packet encoding:** <1 ms
- **Interrupt latency:** <100 µs

### Memory

- **Packet buffer:** 256 bytes typical
- **Neighbor entry:** ~70 bytes
- **Contact entry:** ~100 bytes
- **Signature scratch:** 256 bytes

### Radio

- **Air time (108 bytes @ SF8):** ~185 ms
- **Max throughput:** ~5 packets/second
- **Typical latency:** 200-500 ms

---

## Example Packet Dumps

### Chat Client Advertisement

```
Header: 14 01
  Route: 0 (DIRECT), Type: 4 (ADVERT), Ver: 1

Payload (106 bytes):
  Pubkey: be 3e 22 d0 9b ff 62 2e 1b fa b3 c4 9b 18 73 0d
          a3 a9 2c ec 99 57 81 17 ff b6 2d fd 0a c2 89 ff

  Timestamp: 12 34 56 78 (little-endian = 0x78563412)

  Signature: [64 bytes of Ed25519 signature]

  App Data (6 bytes):
    Flags: 81 (0x80 = has name, 0x01 = CHAT)
    Name: "mg-E4" (5 bytes, NO null terminator)
```

### Repeater Advertisement with Location

```
Header: 15 01
  Route: 1 (FLOOD), Type: 4 (ADVERT), Ver: 1

Path (1 byte): 1a (repeater's own hash)

Payload (128 bytes):
  Pubkey: [32 bytes]
  Timestamp: [4 bytes]
  Signature: [64 bytes]

  App Data (28 bytes):
    Flags: 92 (0x80 = name, 0x10 = lat/lon, 0x02 = REPEATER)
    Lat: 12 34 56 78 (int32, x1E6 format)
    Lon: 9a bc de f0 (int32, x1E6 format)
    Name: "NL-ENS-IRCT-PESCHECK" (20 bytes)
```

---

## Quick Reference Card

```
╔═══════════════════════════════════════════════════════════╗
║  MESHCORE PROTOCOL QUICK REFERENCE                        ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  Packet:  [Header:2][Path:0-64][Payload:Variable]        ║
║  Payload: [PubKey:32][Time:4][Sig:64][AppData:Var]       ║
║  AppData: [Flags:1][Optional][Name:Remainder]            ║
║                                                           ║
║  Signature = Ed25519(PubKey + Time + AppData)            ║
║                                                           ║
║  ⚠️  NAMES ARE NOT NULL-TERMINATED ON WIRE               ║
║  ⚠️  MUST CALL setPacketReceivedAction() FOR RX          ║
║  ⚠️  REPEATERS ONLY TRACK OTHER REPEATERS                ║
║                                                           ║
║  Node Types: 0=NONE 1=CHAT 2=REPEATER 3=ROOM 4=SENSOR   ║
║  Flags: 0x80=Name 0x40=Feat2 0x20=Feat1 0x10=LatLon     ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
```

---

**Document Version:** 1.0
**Created:** 2026-01-08
**MeshCore Version:** Compatible with all V1 implementations
