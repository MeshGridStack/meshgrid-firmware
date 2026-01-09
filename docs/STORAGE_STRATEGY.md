# Storage Strategy & Capacity Planning

## Overview

This document defines the storage strategy for meshgrid firmware, considering current usage, OTA requirements (from OTA_DESIGN.md), and future growth.

## ESP32 Flash Layout

### Recommended Partition Table (4MB minimum)

```csv
# Name,     Type, SubType, Offset,   Size,     Usage
nvs,        data, nvs,     0x9000,   0x5000,   20 KB  - Config & cache
otadata,    data, ota,     0xe000,   0x2000,   8 KB   - OTA metadata
app0,       app,  ota_0,   0x10000,  0x1E0000, 1.9 MB - Active firmware
app1,       app,  ota_1,   0x1F0000, 0x1E0000, 1.9 MB - Update firmware
spiffs,     data, spiffs,  0x3D0000, 0x30000,  192 KB - Optional file storage
```

**Total: 4 MB** (fits ESP32-S3 with 4MB+ flash)

---

## Storage Allocation by Type

### 1. NVS (Non-Volatile Storage) - 20 KB Partition

**Purpose:** Small persistent data that survives reboots
**Technology:** Wear-leveled key-value store
**Capacity:** 20,480 bytes total

#### Current NVS Usage

| Namespace | Current Size | Max Growth | Purpose |
|-----------|--------------|------------|---------|
| **meshgrid** | ~1,095 bytes | ~3,000 bytes | Config, radio, logs, RTC |
| **neighbors** | ~1,005 bytes | ~2,000 bytes | Cached neighbor secrets (10 max) |
| **ota** | 0 bytes | ~851 bytes | OTA session state + bitmap (future) |
| **channels** | 0 bytes | ~200 bytes | Custom channel persistence (future) |
| **TOTAL** | **~2,100 bytes** | **~6,051 bytes** | |

**Available:** ~14 KB (70% headroom)

#### NVS Namespace Details

**"meshgrid" (config):**
```
saved         bool      1 byte     Config saved flag
freq          float     4 bytes    LoRa frequency
bw            float     4 bytes    Bandwidth
sf            uint8     1 byte     Spreading factor
cr            uint8     1 byte     Coding rate
preamble      uint16    2 bytes    Preamble length
power         int8      1 byte     TX power
mode          uint8     1 byte     Device mode
log_en        bool      1 byte     Logging enabled
name          String    17 bytes   Node name
rtc_valid     bool      1 byte     RTC time set
rtc_epoch     uint32    4 bytes    Boot epoch
log_cnt       uint8     1 byte     Log entry count
log0-log19    String    50 each    Saved log entries (max 20)
---
Total: ~1,095 bytes (can grow to ~3,000 if all logs filled)
```

**"neighbors" (cached contacts):**
```
count           uint8       1 byte      Number saved
n0_hash         uint8       1 byte      Hash
n0_pubkey       bytes[32]   32 bytes    Public key
n0_name         String      17 bytes    Name
n0_secret       bytes[32]   32 bytes    ECDH shared secret
[repeat n1-n9]

Per entry: 82 bytes × 10 = 820 bytes
Total: ~1,005 bytes (max 10 neighbors)
```

**"ota" (future - not yet implemented):**
```
session_id      uint32      4 bytes     Active OTA session
state           uint8       1 byte      Download state
version         String      32 bytes    Target version
fw_size         uint32      4 bytes     Firmware size
chunk_size      uint16      2 bytes     Chunk size
total_chunks    uint32      4 bytes     Total chunks
fw_hash         bytes[32]   32 bytes    SHA-256 hash
signature       bytes[64]   64 bytes    Ed25519 sig
signing_key     bytes[32]   32 bytes    Signer pubkey
chunks_rx       uint32      4 bytes     Chunks received
bitmap          bytes[351]  351 bytes   Chunk bitmap (2808 chunks)
manifest_time   uint32      4 bytes     Manifest RX time
---
Total: ~534 bytes + 351 bitmap = 885 bytes per OTA session
```

**"channels" (future - should add):**
```
count           uint8       1 byte      Number saved
c0_name         String      17 bytes    Channel name
c0_hash         uint8       1 byte      Channel hash
c0_secret       bytes[32]   32 bytes    Channel PSK
[repeat c1-c3]

Per entry: 50 bytes × 4 = 200 bytes
Total: ~201 bytes (max 4 channels)
```

---

### 2. OTA Partitions (app0/app1) - 3.8 MB Total

**Purpose:** Dual boot partitions for firmware
**Technology:** Direct flash write via ESP32 Update API
**Capacity:** 1.875 MB each (1,966,080 bytes)

| Partition | Current Use | Max Use | Purpose |
|-----------|-------------|---------|---------|
| **app0** | 509 KB | 1.9 MB | Active running firmware |
| **app1** | 0 bytes | 1.9 MB | OTA update target (written during OTA) |

**OTA Process:**
1. Download chunks to **app1** partition (not NVS!)
2. Save progress bitmap to **NVS "ota"** namespace
3. After complete: verify, reboot, ESP32 swaps to app1
4. If app1 boots successfully: becomes active, app0 is old backup

**Critical:** Firmware chunks go to OTA partition, NOT NVS. Only tiny metadata (bitmap, session state) goes to NVS.

---

### 3. SPIFFS (Optional) - 192 KB Partition

**Purpose:** File storage for non-critical data
**Technology:** Lightweight filesystem
**Capacity:** 196,608 bytes

**Currently:** Not implemented/used

**Future Uses:**
- Message history archives (move old messages from RAM to SPIFFS)
- Channel definitions (backup/restore)
- Logs (archive old logs beyond 20 entries)
- Cached firmware metadata
- Configuration backups

**Why Not Used Yet:**
- Current data fits in NVS comfortably
- SPIFFS adds complexity
- Not needed until we hit NVS limits

---

## RAM vs NVS vs SPIFFS Decision Matrix

| Data Type | Current Location | Should Move To | Reason |
|-----------|------------------|----------------|--------|
| **Neighbors (active)** | RAM (32 entries) | Keep in RAM | Fast lookup, frequently accessed |
| **Neighbors (cache)** | NVS (10 entries) | ✓ Correct | Persist ECDH secrets across reboots |
| **Messages (recent)** | RAM (20 msgs) | Keep in RAM | Fast access, transient |
| **Messages (archive)** | Not saved | SPIFFS (future) | Old messages can be archived |
| **Event logs (recent)** | RAM (50 events) | Keep in RAM | Fast logging |
| **Event logs (archive)** | NVS (20 events) | ⚠️ Move to SPIFFS? | NVS wear concerns |
| **Custom channels** | RAM (4 channels) | ⚠️ Add to NVS | Should persist across reboots |
| **Radio config** | NVS | ✓ Correct | Small, rarely changes |
| **Device mode** | NVS | ✓ Correct | Persists mode setting |
| **RTC time** | NVS | ✓ Correct | Persists time sync |
| **OTA session state** | Not implemented | NVS "ota" | Small metadata only |
| **OTA chunk bitmap** | Not implemented | NVS "ota" | 351 bytes, critical for resume |
| **OTA firmware chunks** | Not implemented | **app0/app1 partition** | Large binary data |

---

## Storage Capacity Analysis

### Current NVS Usage (20 KB partition)

```
Namespace        Current    Max Growth   Total Max   % of 20KB
─────────────────────────────────────────────────────────────
meshgrid         1,095      3,000        3,000       15.0%
neighbors        1,005      2,000        2,000       10.0%
ota (future)     0          885          885         4.4%
channels (future) 0         201          201         1.0%
─────────────────────────────────────────────────────────────
TOTAL            2,100      6,086        6,086       30.4%
AVAILABLE        -          -            13,914      69.6%
```

**Conclusion:** Even with OTA + channels, only using **30% of NVS**. Safe headroom.

---

## Storage Limits & Quotas

### Hard Limits (Prevent Fill-Up)

| Item | RAM Limit | NVS Saved | Quota Logic |
|------|-----------|-----------|-------------|
| **Neighbors** | 32 max | 10 newest | When 32nd added, evict oldest. Save top 10 by last_seen |
| **Messages** | 20 max | 0 (not saved) | Circular buffer, oldest auto-evicted |
| **Event Log** | 50 max | 20 newest | Circular buffer in RAM. Save last 20 to NVS |
| **Custom Channels** | 4 max | 4 max (future) | Hard limit, reject 5th channel |
| **OTA Sessions** | 1 active | 1 active | Only 1 concurrent OTA session |
| **TX Queue** | 16 max | 0 | Circular queue, drop if full |

### Storage Full Handling

**When NVS approaches capacity (>18 KB used):**

1. **Auto-cleanup triggers:**
   ```cpp
   if (nvs_usage > 18000) {  // 90% full
       // Purge old logs beyond 10 (keep only 10 newest)
       // Purge old neighbors beyond 5 (keep only 5 newest)
       // Log warning
   }
   ```

2. **Graceful degradation:**
   - Stop saving new logs to NVS (keep RAM logging)
   - Reduce neighbor cache to 5 instead of 10
   - Warn user: "Storage low - some data not persisted"

3. **User commands:**
   ```
   CONFIG RESET    - Clear all NVS (factory reset)
   LOG CLEAR       - Clear saved logs
   NEIGHBORS CLEAR - Clear saved neighbors cache
   ```

---

## Recommended Storage Strategy

### Phase 1: Current (Implemented)

**NVS Usage:**
- "meshgrid": Config, radio, mode, RTC, logs (20 max)
- "neighbors": Top 10 neighbors with secrets

**Flash Usage:**
- app0: Active firmware (~509 KB)

**SPIFFS:** Not used yet

**Total:** ~2.1 KB NVS (10% of capacity) ✓

---

### Phase 2: Add Channel Persistence (Recommended)

**New NVS keys:**
```cpp
// In "meshgrid" namespace or new "channels" namespace
ch_count        uint8       Number of custom channels
ch0_name        String      Channel name
ch0_hash        uint8       Channel hash
ch0_secret      bytes[16]   Channel PSK (16 bytes for 128-bit keys)
[repeat ch1-ch3]
```

**Impact:** +200 bytes NVS
**Total:** ~2.3 KB NVS (11.5% of capacity) ✓

---

### Phase 3: OTA Implementation (Planned - from OTA_DESIGN.md)

**NVS "ota" namespace:**
- Session metadata: ~534 bytes
- Chunk bitmap: ~351 bytes (for 492 KB firmware)
- **Total: ~885 bytes**

**OTA Partition usage:**
- app0: Active firmware (~509 KB)
- app1: Update firmware during download (~509 KB)
- After successful OTA: app1 becomes active, app0 is old backup

**Impact:** +885 bytes NVS, uses app1 partition
**Total:** ~3.2 KB NVS (16% of capacity) ✓

---

### Phase 4: Add SPIFFS for Archives (Optional)

**Use SPIFFS (192 KB) for:**

1. **Message Archive** (old messages beyond 20)
   ```
   /messages/2026-01-10.log    (~10 KB per day)
   /messages/2026-01-11.log
   ```

2. **Log Archive** (old events beyond 50)
   ```
   /logs/boot_2026-01-10_143015.log
   ```

3. **Neighbor History** (track node appearances over time)
   ```
   /neighbors/history.db   (~50 KB)
   ```

4. **Channel Export/Import**
   ```
   /channels/private_channels.json
   ```

**Benefits:**
- Offload large data from NVS
- Reduce NVS wear (NVS has ~100K write cycles per key)
- Archive old data without losing it

**Drawbacks:**
- Adds complexity (filesystem APIs)
- Slower access than NVS
- Not needed yet (NVS has plenty of space)

**Recommendation:** Implement SPIFFS only when NVS usage exceeds 50% (10 KB)

---

## Storage Growth Projections

### Scenario 1: Heavy Mesh Use (50 neighbors, 10 channels, full logs)

| Item | Count | NVS Size | Notes |
|------|-------|----------|-------|
| Config | 1 | 50 bytes | Radio + mode + RTC |
| Logs | 20 | 1,000 bytes | Full log buffer |
| Neighbors | 10 | 1,005 bytes | Top 10 cached |
| Channels | 4 | 201 bytes | All custom channels |
| OTA state | 1 | 885 bytes | Active OTA session |
| **TOTAL** | | **3,141 bytes** | **15.3% of 20 KB** |

**Verdict:** ✓ Safe - still 84.7% free

---

### Scenario 2: Extended Operation (6 months, 200 neighbors total lifetime)

| Item | Count | NVS Size | Notes |
|------|-------|----------|-------|
| Config | 1 | 50 bytes | |
| Logs | 20 | 1,000 bytes | Last 20 events |
| Neighbors | 10 | 1,005 bytes | Most recent 10 only |
| Channels | 4 | 201 bytes | |
| OTA state | 0 | 0 bytes | Cleared after success |
| **TOTAL** | | **2,256 bytes** | **11% of 20 KB** |

**Verdict:** ✓ Safe - circular buffers prevent unbounded growth

---

### Scenario 3: Maximum Capacity Test

**What if we hit NVS limits?**

Assuming we save:
- 50 neighbors (instead of 10): 4,100 bytes
- 100 log entries (instead of 20): 5,000 bytes
- 10 custom channels (instead of 4): 501 bytes
- OTA session active: 885 bytes
- Config: 50 bytes

**Total:** ~10,536 bytes = **51% of 20 KB**

**Verdict:** ⚠️ Getting tight, but still safe. Would trigger cleanup at 90% (18 KB).

---

## Storage Limits & Cleanup Policy

### Automatic Cleanup Thresholds

```cpp
// Trigger cleanup when NVS usage exceeds thresholds
#define NVS_WARNING_THRESHOLD  (16 * 1024)  // 16 KB (80% full)
#define NVS_CRITICAL_THRESHOLD (18 * 1024)  // 18 KB (90% full)

void nvs_check_usage() {
    size_t used = nvs_get_used_size();  // ESP-IDF API

    if (used > NVS_CRITICAL_THRESHOLD) {
        // CRITICAL: Aggressively purge
        log_event("STORAGE CRITICAL: Purging old data");
        purge_old_logs(5);        // Keep only 5 logs
        purge_old_neighbors(3);   // Keep only 3 neighbors
    } else if (used > NVS_WARNING_THRESHOLD) {
        // WARNING: Moderate cleanup
        log_event("STORAGE WARNING: Cleaning up");
        purge_old_logs(10);       // Keep 10 logs
        purge_old_neighbors(5);   // Keep 5 neighbors
    }
}
```

### Manual Cleanup Commands

**User-invokable cleanup:**
```
CONFIG RESET       - Erase all NVS (factory reset)
LOG CLEAR          - Clear saved logs
NEIGHBORS CLEAR    - Clear saved neighbors cache
CHANNELS CLEAR     - Clear custom channels (future)
OTA ABORT          - Clear OTA session state (future)
```

### NVS Write Limits (Wear Leveling)

**ESP32 NVS:** ~100,000 write cycles per key before wear-out

**Our Write Patterns:**
| Data | Write Frequency | Lifetime Writes | Wear Risk |
|------|----------------|-----------------|-----------|
| Config | ~1/day | ~365/year | ✓ Safe |
| Mode | ~1/week | ~52/year | ✓ Safe |
| RTC time | ~1/day | ~365/year | ✓ Safe |
| Neighbors | ~1/5 neighbors | ~100/year (assuming 500 new neighbors/year) | ✓ Safe |
| Logs | ~1/50 events | ~1,000/year (assuming 50K events/year) | ✓ Safe |
| OTA state | ~1/50 chunks | ~56/OTA session (2808 chunks) | ⚠️ Moderate |

**Most Wear:** OTA bitmap updates (~56 writes per OTA). Assuming 1 OTA/month = 672 writes/year. At 100K limit = **148 years before wear-out**. ✓ Safe.

---

## Recommendations

### Immediate Actions (Current Implementation)

1. ✓ **Keep current NVS usage** - well within limits
2. ✓ **Save top 10 neighbors** - balance persistence vs. wear
3. ✓ **Save last 20 logs** - useful for debugging without excessive writes
4. ⚠️ **Add custom channel persistence** - users expect channels to persist

### Short-Term (Pre-OTA)

1. **Implement channel persistence:**
   ```cpp
   void channels_save_to_nvs() {
       // Save custom_channels[] to NVS "channels" namespace
       // ~200 bytes total
   }
   ```

2. **Add storage monitoring:**
   ```cpp
   void storage_stats() {
       // Report NVS usage via INFO/STATS command
       // Warn if approaching limits
   }
   ```

3. **Implement cleanup triggers:**
   ```cpp
   // Before saving anything to NVS, check usage
   if (nvs_usage > WARNING_THRESHOLD) cleanup();
   ```

### Medium-Term (OTA Implementation)

1. **OTA partition strategy:**
   - ✓ Chunks write to **app1 partition** (1.9 MB available)
   - ✓ Session metadata in **NVS "ota"** (~885 bytes)
   - ✓ Clear OTA state after successful update

2. **OTA bitmap optimization:**
   - Write to NVS every 50 chunks (not every chunk)
   - Reduces wear: 2808 chunks / 50 = ~56 writes per OTA
   - Total OTA NVS impact: <1 KB

3. **Partition table:**
   - Use partitions_ota.csv (from OTA_DESIGN.md)
   - Ensures adequate space for dual firmware + NVS

### Long-Term (Scalability)

1. **If NVS exceeds 10 KB (50% full):**
   - Enable SPIFFS partition
   - Move log archives to SPIFFS
   - Move old neighbor history to SPIFFS
   - Keep only recent data in NVS

2. **If message history needed:**
   - Implement SPIFFS-based message archive
   - Auto-archive messages older than 24 hours
   - CLI command: `messages history` to read archives

3. **If many custom channels (>4):**
   - Increase MAX_CUSTOM_CHANNELS to 16
   - Store in SPIFFS instead of NVS
   - Adds ~800 bytes (16 channels × 50 bytes)

---

## Critical: What Happens When Storage Fills

### NVS Full (>19.5 KB used)

**Symptoms:**
- `prefs.put*()` calls return `false` (write failure)
- New neighbors not saved (cache disabled)
- New logs not saved (logging continues in RAM only)

**Mitigation:**
```cpp
bool safe_nvs_write(const char* key, /* value */) {
    size_t free = nvs_get_free_size();
    if (free < 512) {  // Less than 512 bytes free
        log_event("NVS FULL - purging old data");
        emergency_cleanup();  // Delete oldest logs, neighbors
        free = nvs_get_free_size();
    }
    if (free > 256) {
        return prefs.put*(key, value);
    }
    log_event("NVS FULL - write failed");
    return false;
}
```

### OTA Partition Full (Firmware > 1.9 MB)

**Symptoms:**
- `Update.write()` returns error
- OTA fails with "insufficient space"

**Mitigation:**
- Reject OTA manifest if firmware_size > partition size
- Error: "Firmware too large (X MB) - partition only Y MB"

**Prevention:**
- Keep firmware <1.5 MB (leave 400 KB headroom)
- Current: 509 KB (72% headroom) ✓

### SPIFFS Full (If Enabled)

**Symptoms:**
- File writes fail
- Archive operations error

**Mitigation:**
- Implement FIFO: delete oldest archive when adding new
- Keep last 7 days of message/log archives
- CLI warning: "Archive storage 90% full"

---

## Storage Strategy Summary

### Data Placement Rules

| Data Size | Access Pattern | Persistence | Use |
|-----------|----------------|-------------|-----|
| **< 100 bytes** | Frequent | Required | NVS |
| **100-1000 bytes** | Moderate | Required | NVS |
| **1-10 KB** | Infrequent | Required | NVS or SPIFFS |
| **> 10 KB** | Archive | Optional | SPIFFS |
| **> 100 KB** | Binary data | Critical | Dedicated partition (OTA) |

### Current Implementation ✓ CORRECT

1. **NVS for small critical data:** Config, mode, RTC, top neighbors
2. **RAM for active data:** Full neighbor table, messages, logs
3. **OTA partitions for firmware:** Dual boot (app0/app1)
4. **SPIFFS: Reserved for future** if needed

### Future Enhancements (When Needed)

1. **Enable SPIFFS** when NVS > 50% full
2. **Archive old data** to SPIFFS (logs, messages, neighbor history)
3. **Implement storage monitoring** in STATS command
4. **Add cleanup commands** for user control

---

## Implementation Checklist

### Must-Do Before OTA

- [x] NVS namespace "meshgrid" for config ✓
- [x] NVS namespace "neighbors" for cache ✓
- [ ] Add NVS namespace "ota" for session state
- [ ] Add NVS namespace "channels" for custom channels
- [ ] Implement storage usage monitoring
- [ ] Add cleanup triggers (auto-purge at 90%)
- [ ] Create partitions_ota.csv partition table
- [ ] Test NVS full scenario (simulate 20 KB usage)

### Optional Enhancements

- [ ] Enable SPIFFS partition (192 KB)
- [ ] Implement message archive to SPIFFS
- [ ] Implement log archive to SPIFFS
- [ ] Add `/storage stats` CLI command
- [ ] Add storage warning system (display notification)

---

## Conclusion

**Current Strategy: ✓ Excellent**

- NVS usage: 2.1 KB / 20 KB (10.5%) - plenty of headroom
- Firmware: 509 KB / 1.9 MB (26.7%) - excellent headroom
- RAM: 15.3 KB / 320 KB (4.8%) - very efficient

**With OTA Added:**
- NVS usage: ~3 KB / 20 KB (15%) - still safe
- OTA chunks: app1 partition (1.9 MB) - NOT NVS
- No risk of filling storage

**Recommendations:**
1. ✅ Current implementation is solid
2. ✅ Add channel persistence to NVS (~200 bytes)
3. ✅ Implement storage monitoring before OTA
4. ⏸️ SPIFFS not needed yet (defer until NVS >50%)
5. ✅ Add auto-cleanup at 90% NVS usage (safety net)

**Storage will NOT be a problem** - even with OTA, we're only using ~16% of NVS capacity!
