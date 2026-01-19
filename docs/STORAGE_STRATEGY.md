# Flash Storage Strategy

## Current Firmware Sizes (2026-01-13)

| Build Type | Binary Size | Flash Usage | Status |
|------------|-------------|-------------|--------|
| Non-BLE | 553 KB | 33% of 1.7MB partition | ✅ Fits all platforms |
| BLE | ~1.1 MB | 66% of 1.7MB partition | ⚠️ Too large for nRF52840 |

## Platform Flash Specifications

### ESP32-S3 (38 devices) - ✅ EXCELLENT
- **Flash:** 4-16MB
- **OTA Partition:** 1.656 MB - 5 MB per slot
- **Current Usage:** 553KB / 1.7MB = **33%**
- **Strategy:** Keep default `-O2` optimization
- **Headroom:** 1+ MB free for features

**Devices:** Heltec V3/V4/Vision Master series, LilyGo T-Beam Supreme/T3-S3/T-Deck series, Seeed SenseCAP, Elecrow ThinkNode/Crowpanel, RAK3312, TrackSenger, etc.

### ESP32 Classic (10 devices) - ✅ GOOD
- **Flash:** 4MB
- **OTA Partition:** 1.656 MB per slot
- **Current Usage:** 553KB / 1.7MB = **33%**
- **Strategy:** Keep default `-O2` optimization

**Devices:** LilyGo T-Beam/T-LoRa V2.1, RAK 11200, B&Q Nano/Station G1, M5Stack, RadioMaster 900 Bandit, DIY boards

### ESP32-C3 (1 device) & ESP32-C6 (1 device) - ✅ GOOD
- **Flash:** 4MB
- **Current Usage:** 33%
- **Strategy:** Keep default optimization

**Devices:** Heltec HT62 (C3), M5Stack Unit C6L (C6)

### nRF52840 (28 devices) - ⚠️ CONSTRAINED
- **Flash:** 1MB total
- **Bootloader:** ~48KB
- **SoftDevice (BLE):** ~152KB (mandatory)
- **Available:** ~800KB
- **Current Usage:** 553KB / 800KB = **69%**
- **Strategy:** Size optimization applied (`-Os` + dead code elimination)
- **Status:** Needs testing on actual hardware

**Devices:** LilyGo T-Echo series, RAK 4631/WisMesh series, Heltec Mesh Node T114, Seeed trackers, Elecrow ThinkNode M1/M3, muzi, NomadStar Meteor Pro, Canary One

### RP2040 (4 devices) - ✅ GOOD
- **Flash:** 2-16MB (board dependent)
- **Current Usage:** 553KB / 2MB = **28%**
- **Strategy:** Keep default optimization

**Devices:** Raspberry Pi Pico/Pico W, RAK 11310, Waveshare RP2040 LoRa

## Optimization Test Results

We tested three optimization levels on ESP32-S3 (Heltec V3):

| Optimization | Binary Size | Code Size | Result |
|--------------|-------------|-----------|--------|
| `-O2` (default) | 553 KB | 405,128 bytes | Baseline |
| `-Os` (size) | 553 KB | 405,332 bytes | **+332 bytes larger!** |
| `-O3` (speed) | 553 KB | 405,332 bytes | Same size, potentially faster |

**Key Finding:** Size optimization (`-Os`) provides **no benefit** on ESP32 with Xtensa GCC. It actually makes the binary slightly larger while reducing performance by 5-15%.

## Current Build Configuration

### ESP32-S3/C3/C6 Platforms
```ini
[esp32s3_base]
platform = espressif32
framework = arduino
build_flags =
    -DARCH_ESP32S3
    # Default -O2 is optimal for size and speed
```

### nRF52840 Platforms
```ini
[nrf52840_base]
platform = nordicnrf52
framework = arduino
build_flags =
    -DARCH_NRF52840
    -Os                        # Optimize for size (ARM GCC may differ)
    -ffunction-sections        # Enable dead code elimination
    -fdata-sections
    -DCORE_DEBUG_LEVEL=0      # Remove debug overhead
```

**Note:** ARM GCC compiler may behave differently than Xtensa GCC. Size optimization is applied preventatively but needs testing on actual nRF52840 hardware.

## If nRF52840 Still Doesn't Fit

If optimization doesn't reduce size enough:

### Option 1: Reduce Buffer Sizes
```cpp
#ifdef SIZE_OPTIMIZED
  #define LOG_BUFFER_SIZE 32           // Down from 64
  #define NEIGHBOR_TABLE_SIZE 16       // Down from 32
  #define PUBLIC_MESSAGE_BUFFER_SIZE 16
  #define DIRECT_MESSAGE_BUFFER_SIZE 16
#endif
```
**Savings:** ~20-30KB

### Option 2: Disable Protocol v1
```cpp
#define DISABLE_PROTOCOL_V1  // Use only v0 (MeshCore compatible)
```
**Savings:** ~30-40KB (removes AES-256, HMAC-SHA256, sequence tracking)

### Option 3: Remove Custom Channels
```cpp
#define DISABLE_CUSTOM_CHANNELS  // Public channel only
```
**Savings:** ~20KB

### Option 4: No OTA Updates
Use single-slot partition table (no OTA support):
```csv
# 1MB flash, no OTA
app0, app, factory, 0x10000, 0xF0000,  # 960KB app
```
**Gain:** ~400KB more space (updates require USB flashing)

## Partition Tables

### 4MB Flash (Most ESP32 devices)
```csv
# partitions/partitions_4mb_ota.csv
app0, app, ota_0, 0x10000,  0x1A0000,  # 1.656 MB
app1, app, ota_1, 0x1B0000, 0x1A0000,  # 1.656 MB
spiffs, data, spiffs, 0x350000, 0xA0000,  # 640 KB
```

### 8MB Flash (T-Beam Supreme, T-Deck)
```csv
# partitions/partitions_8mb_ota.csv
app0, app, ota_0, 0x10000,  0x300000,  # 3 MB
app1, app, ota_1, 0x310000, 0x300000,  # 3 MB
spiffs, data, spiffs, 0x610000, 0x1E0000,  # 1.875 MB
```

### 16MB Flash (Heltec V4)
```csv
# partitions/partitions_16mb_ota.csv
app0, app, ota_0, 0x10000,  0x500000,  # 5 MB
app1, app, ota_1, 0x510000, 0x500000,  # 5 MB
spiffs, data, spiffs, 0xA10000, 0x5E0000,  # 6 MB
```

## BLE Build Variants

**ESP32-S3/C3/C6:** Offer both non-BLE and BLE variants
```ini
[env:heltec_v3]      # Non-BLE: 553 KB
[env:heltec_v3_ble]  # BLE: ~1.1 MB
```

**nRF52840:** BLE mandatory (SoftDevice required)
- No separate BLE variant needed
- BLE stack consumes 152KB of 1MB flash

**RP2040:** No native BLE support
- Skip BLE variants entirely

## Testing Requirements

### nRF52840 Priority Testing
When nRF52840 hardware becomes available:

1. **Build and measure:**
   ```bash
   pio run -e lilygo_techo
   size .pio/build/lilygo_techo/firmware.elf
   ```

2. **Check if it fits:**
   - Target: < 700KB (leaves 100KB safety margin)
   - Acceptable: < 750KB (tight but works)
   - Critical: > 800KB (needs feature reduction)

3. **Test functionality:**
   - Radio TX/RX working
   - Neighbor discovery
   - Message send/receive
   - Protocol v1 features

4. **If too large:**
   - Apply Option 1 (reduce buffers): ~20KB savings
   - If still too large, apply Option 2 (disable v1): ~30KB savings
   - Last resort: Option 4 (no OTA): +400KB space

## Summary

| Platform Count | Strategy | Status |
|----------------|----------|--------|
| 38 ESP32-S3 | Keep `-O2`, plenty of space | ✅ Optimal |
| 10 ESP32 Classic | Keep `-O2`, adequate space | ✅ Good |
| 2 ESP32-C3/C6 | Keep default, adequate space | ✅ Good |
| 28 nRF52840 | `-Os` applied, needs testing | ⏳ Pending |
| 4 RP2040 | Keep default, adequate space | ✅ Good |

**Bottom Line:** Only nRF52840 requires special attention. All other platforms have plenty of headroom for features.
