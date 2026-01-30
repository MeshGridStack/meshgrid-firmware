# meshgrid OTA Update Workflow

**Date:** 2026-01-30
**Status:** Production Ready
**Current Firmware Size:** 564 KB (Heltec V3), 552 KB (T3S3)

## Overview

Simple three-step OTA update process:
1. **GitHub** builds and signs firmware automatically (when team member releases)
2. **You** download and push to one device via CLI (when convenient)
3. **Mesh network** distributes to all devices automatically (30-60 minutes)

**Key Principle:** Push to ONE device via USB, mesh handles the rest.

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│ Step 1: GitHub Actions (Fully Automated)               │
├─────────────────────────────────────────────────────────┤
│ Team member creates release:                            │
│   → GitHub UI: Actions → Release OTA → Run workflow     │
│                                                          │
│ GitHub automatically:                                    │
│   ✅ Builds firmware (all boards)                       │
│   ✅ Signs with authorized team member's key            │
│   ✅ Creates GitHub Release                             │
│   ✅ Attaches .signed files                             │
│                                                          │
│ Team member authorization:                              │
│   • Managed via GitHub team: "ota-release-signers"     │
│   • Each member has their own Ed25519 keypair           │
│   • All public keys compiled into firmware              │
│   • All private keys stored in org secret (JSON)        │
└─────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────┐
│ Step 2: Manual Push via CLI (When Convenient)          │
├─────────────────────────────────────────────────────────┤
│ You (or any authorized person):                         │
│   1. Connect ANY device in mesh via USB                 │
│      (Linux: /dev/ttyUSB0, Windows: COM3, macOS: /dev/tty.usb*) │
│                                                          │
│   2. Run single command:                                │
│      meshgrid-cli ota push-release \                    │
│        --repo yourorg/meshgrid-firmware \               │
│        --latest \                                        │
│        --port auto                                       │
│                                                          │
│   CLI automatically:                                     │
│     ✅ Fetches latest release from GitHub               │
│     ✅ Downloads signed firmware                        │
│     ✅ Verifies Ed25519 signature                       │
│     ✅ Detects connected device                         │
│     ✅ Pushes OTA update via USB                        │
│     ✅ Shows progress                                    │
└─────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────┐
│ Step 3: Mesh Distribution (Fully Automatic)            │
├─────────────────────────────────────────────────────────┤
│ Epidemic gossip protocol:                               │
│                                                          │
│ Phase 1 (5 min): Manifest flooding                      │
│   Device A → broadcasts manifest to neighbors           │
│   Neighbors verify signature → flood onwards            │
│   All reachable devices receive manifest                │
│                                                          │
│ Phase 2 (10-30 min): Chunk distribution                │
│   Device A → floods firmware chunks                     │
│   Devices gossip chunks peer-to-peer                    │
│   Devices request missing chunks                        │
│   Chunk deduplication prevents re-downloads             │
│                                                          │
│ Phase 3 (auto): Verification & Application             │
│   All chunks received → verify SHA-256 hash             │
│   Battery check (>50%) → apply update                   │
│   Device reboots into new firmware                      │
│   If boot fails → auto-rollback to old firmware         │
│                                                          │
│ Result: All 20+ devices updated automatically           │
└─────────────────────────────────────────────────────────┘
```

---

## Team Member Management (GitHub Organization)

### GitHub Team: ota-release-signers

**Purpose:** Controls who can release OTA firmware updates

**How it works:**
- GitHub team membership = OTA signing authorization
- Add member to team → auto-generates signing keypair
- Remove member from team → auto-revokes signing rights
- No manual key management needed

### Adding a Team Member

**1. Add to GitHub Team (Web UI):**
```
Organization → Teams → ota-release-signers → Add a member
Select: david → Add member
```

**2. Auto-Sync Workflow Runs:**
- Detects new team member
- Generates Ed25519 keypair for David
- Updates organization secret with David's keys
- Creates PR to update `src/ota/ota_trusted_keys.h`

**3. Review and Merge PR:**
```
Pull Requests → "OTA Keys: Sync with team membership"
Files changed: src/ota/ota_trusted_keys.h (added David's public key)
Review → Approve → Merge
```

**4. Release Firmware Update:**
```
Actions → Release OTA Firmware → Run workflow
Version: v0.0.4
```

**5. Done!** David can now release firmware, all devices trust his signatures.

### Removing a Team Member

**1. Remove from GitHub Team:**
```
Organization → Teams → ota-release-signers → Member → Remove
```

**2. Auto-Sync Workflow Runs:**
- Detects member removal
- Marks their key as inactive
- Creates PR to update trusted keys

**3. Merge PR → Release new firmware**

**4. All devices no longer trust removed member's signatures**

---

## Releasing Firmware (GitHub Web UI)

### Authorized Team Member Workflow

**1. Trigger Release from GitHub UI:**
```
Repository → Actions → "Release OTA Firmware" → Run workflow

Inputs:
  Version: v0.0.3
  Board: all (or specific board)

Click: "Run workflow"
```

**2. Workflow Runs Automatically:**
```
✅ Check authorization (is user in ota-release-signers team?)
✅ Build firmware for all boards
✅ Load user's signing key from organization secret
✅ Sign firmware with user's private key
✅ Verify signatures
✅ Create GitHub Release
✅ Upload signed .signed files
✅ Post release notes with deployment instructions
```

**3. Release Created:**
```
GitHub Releases → v0.0.3
  Signed by: @alice

  Files:
    📦 Signed Firmware (for OTA mesh updates - requires authorization):
      - firmware-heltec-v3-v0.0.3.signed (564 KB)
      - firmware-t3s3-v0.0.3.signed (552 KB)

    📦 Unsigned Firmware (for USB flashing - open source):
      - heltec_v3-firmware.bin (564 KB) + .sha256 checksum
      - lilygo_t3s3-firmware.bin (552 KB) + .sha256 checksum
```

**Time:** 2-3 minutes from clicking "Run workflow" to release published

---

## File Types Explained

### Unsigned .bin (USB Flashing Only)

**Purpose:** Allow anyone to flash individual devices via USB

**Security:** Physical access required (device in hand, USB cable)

**Use cases:**
- Initial device setup
- Development and testing
- Custom code modifications
- Emergency recovery
- Individual device updates

**Verification:** SHA256 checksum (integrity, not authorization)

```bash
# Anyone can do this (open source!)
meshgrid-cli flash heltec-v3 --version v0.0.3
  → Downloads .bin file
  → Verifies SHA256 checksum (detects corruption)
  → Flashes via USB
  → Only affects THIS device
```

### Signed .signed (OTA Mesh Updates)

**Purpose:** Authorized mesh-wide firmware distribution

**Security:** Ed25519 signature + SHA256 verification

**Use cases:**
- Update all devices in mesh remotely
- Controlled firmware distribution
- Production deployments

**Verification:** Ed25519 signature (authorization) + SHA256 hash (integrity)

```bash
# Only authorized signers can push to mesh
meshgrid-cli ota push-release --latest --port auto
  → Downloads .signed file
  → Verifies Ed25519 signature (checks authorization)
  → Verifies SHA256 hash (checks integrity)
  → Pushes to mesh
  → Updates ALL devices in network
```

### Why Both?

| | USB Flash (.bin) | OTA Mesh (.signed) |
|---|------------------|-------------------|
| **Access** | Physical device required | Any device in mesh |
| **Scope** | Single device | Entire network (20+ devices) |
| **Authorization** | Physical access = trusted | Ed25519 signature required |
| **Open Source** | ✅ Anyone can build & flash | ❌ Only authorized signers |
| **Recovery** | ✅ Always works | ❌ Needs working firmware |
| **Security Model** | Trust through possession | Trust through cryptography |

---

## Deploying OTA Updates (meshgrid-cli)

### Method 1: Push from GitHub Release (Recommended)

**Single command deployment:**
```bash
# Connect any device in the mesh via USB, then:
meshgrid-cli ota push-release \
  --repo yourorg/meshgrid-firmware \
  --latest \
  --port auto
```

**What happens:**
```
🔍 Fetching releases from GitHub...
📦 Latest release: v0.0.3
📥 Downloading firmware-heltec-v3-v0.0.3.signed...
[████████████████████] 564 KB/564 KB (2.3 MB/s)
✅ Downloaded
🔐 Verifying signature...
✅ Signature valid (signed by @alice)
🔍 Detecting connected devices...
✅ Found device at /dev/ttyUSB0 (mg-A1, v0.0.2)
📤 Pushing OTA to device...

Manifest: ✅ Sent
Chunks: [████████████████████] 3322/3322 (100%)

✅ OTA update pushed successfully!
📡 The mesh network will distribute to all devices in 30-60 minutes.

💡 Monitor progress: meshgrid-cli ota watch --port /dev/ttyUSB0
```

**Flags:**
```bash
--repo <org/repo>      # GitHub repository
--version <v0.0.3>     # Specific version (or --latest)
--latest               # Use latest release
--board <heltec_v3>    # Board type
--port <device>        # Serial port (or "auto")
```

### Method 2: Manual Download + Push

**If you prefer manual download:**
```bash
# Download from GitHub Releases page
wget https://github.com/yourorg/meshgrid-firmware/releases/download/v0.0.3/firmware-heltec-v3-v0.0.3.signed

# Push to device
meshgrid-cli ota push firmware-heltec-v3-v0.0.3.signed --port auto

# Same result!
```

### Cross-Platform Device Detection

**Auto-detect works on all platforms:**

**Linux:**
```bash
meshgrid-cli ota push-release --latest --port auto
# Tries: /dev/ttyUSB0, /dev/ttyACM0, /dev/ttyUSB1, ...
```

**macOS:**
```bash
meshgrid-cli ota push-release --latest --port auto
# Tries: /dev/tty.usbserial-*, /dev/tty.usbmodem-*, ...
```

**Windows:**
```powershell
meshgrid-cli ota push-release --latest --port auto
# Tries: COM3, COM4, COM5, ...
```

### List Available Devices

```bash
meshgrid-cli devices

# Output:
# Available meshgrid Devices:
#
# 1. /dev/ttyUSB0
#    Name:     mg-A1
#    Board:    Heltec V3
#    Version:  v0.0.2
#    Hash:     0xa1
#
# 2. /dev/ttyACM0
#    Name:     mg-B2
#    Board:    LilyGo T3S3
#    Version:  v0.0.2
#    Hash:     0xb2
#
# Use: meshgrid-cli ota push-release --port /dev/ttyUSB0
```

---

## Monitoring OTA Progress

### Watch Live Progress

```bash
meshgrid-cli ota watch --port /dev/ttyUSB0 --interval 5
```

**Output:**
```
meshgrid OTA Deployment Monitor
Session: 0xABCD1234
Version: v0.0.3
Started: 2026-01-30 16:45:00

┌──────────┬────────────┬──────────┬────────┬──────────────────┐
│ Device   │ Version    │ Progress │ Status │ Last Update      │
├──────────┼────────────┼──────────┼────────┼──────────────────┤
│ mg-A1    │ v0.0.3     │ 100%     │ Done   │ 2s ago           │
│ mg-B2    │ v0.0.2→0.3 │  89%     │ DL     │ 1s ago           │
│ mg-C3    │ v0.0.2→0.3 │  73%     │ DL     │ 3s ago           │
│ mg-D4    │ v0.0.2→0.3 │  45%     │ DL     │ 5s ago           │
│ mg-E5    │ v0.0.2→0.3 │  12%     │ DL     │ 8s ago           │
└──────────┴────────────┴──────────┴────────┴──────────────────┘

Network Status: 5 devices, 1 complete, 4 downloading
Estimated completion: 18 minutes

Press Ctrl+C to stop monitoring (OTA will continue in background)
```

### Query Individual Device

```bash
meshgrid-cli ota status --port /dev/ttyUSB0

# Output:
# OTA Status:
#   Session:  0xABCD1234
#   Version:  v0.0.3
#   State:    Downloading
#   Progress: 2956/3322 chunks (89%)
#   Battery:  78% (sufficient for auto-apply)
#   ETA:      ~5 minutes
```

### Check All Neighbors

```bash
meshgrid-cli neighbors --port /dev/ttyUSB0

# Output shows which devices have updated:
# Neighbor Table (5 nodes):
#
#   Hash   Ver    Name     RSSI  SNR  Firmware      Last
#   ────── ────── ──────── ───── ──── ───────────── ────────
#   0xa1   v1     mg-A1        0   12 v0.0.3        2s ago   ✅
#   0xb2   v1     mg-B2       -5   10 v0.0.2→0.3    5s ago   ⏳
#   0xc3   v1     mg-C3       -8    9 v0.0.2→0.3    8s ago   ⏳
#   0xd4   v1     mg-D4      -12    7 v0.0.2        12s ago  📥
```

---

## Security Model

### Key Management

**Storage:**
```
┌────────────────────────────────────────────────────────┐
│ authorized_signers.json (Never in Git!)                │
├────────────────────────────────────────────────────────┤
│ {                                                      │
│   "keys": [                                            │
│     {                                                  │
│       "github_login": "alice",                         │
│       "email": "alice@company.com",                    │
│       "private_key": "8a4f3c...",  ← Signs firmware   │
│       "public_key": "1a2b3c...",   ← In firmware      │
│       "active": true                                   │
│     },                                                 │
│     {                                                  │
│       "github_login": "bob",                           │
│       "email": "bob@company.com",                      │
│       "private_key": "f1e2d3...",                      │
│       "public_key": "a1b2c3...",                       │
│       "active": true                                   │
│     }                                                  │
│   ]                                                    │
│ }                                                      │
└────────────────────────────────────────────────────────┘
           ↓
    Stored in GitHub Organization Secret:
    OTA_SIGNING_KEYS
```

**Access Control:**
```
WHO CAN RELEASE:
✅ Members of GitHub team "ota-release-signers"
✅ Verified by GitHub API before workflow runs
✅ Each has their own signing key

WHO CAN PUSH TO MESH:
✅ Anyone with meshgrid-cli + access to any mesh device
✅ Firmware must be properly signed (GitHub provides this)
✅ Devices verify signature before accepting

WHO CANNOT RELEASE:
❌ Non-team members (GitHub blocks workflow)
❌ Can't forge signatures (don't have private keys)
❌ Can't modify signed firmware (SHA-256 verification fails)
```

### Cryptographic Security

**Two-Layer Verification (Defense in Depth):**

```
Layer 1: Ed25519 Signature (Authorization)
  ↓ WHO signed this firmware?
  ↓ Are they authorized?

Layer 2: SHA256 Hash (Integrity)
  ↓ Was firmware corrupted during transmission?
  ↓ Did chunks arrive intact?
```

**Why both?**
- **Ed25519**: Proves firmware is from authorized signer (can't be forged)
- **SHA256**: Proves firmware wasn't corrupted during mesh gossip (detects bit flips)

Both are needed:
- Signature alone doesn't detect transmission errors
- Hash alone doesn't prove authorization

**Firmware Signature Chain:**
```
1. Build firmware → firmware.bin (564 KB)

2. Compute SHA-256:
   hash = SHA256(firmware.bin)
   Purpose: Detect corruption after chunk reassembly

3. Create manifest:
   manifest = {
     version: "v0.0.3",
     size: 564000,
     chunks: 3322,
     hash: "a1b2c3d4...",  ← SHA256 of complete firmware
   }

4. Sign manifest with Ed25519:
   signature = Ed25519_Sign(manifest, private_key)
   Purpose: Prove this is from authorized signer

5. Package:
   firmware.signed = [manifest][signature][public_key][firmware.bin]
```

**Device Verification (Two-Layer Security):**
```
1. Receive firmware.signed via USB or mesh

2. Extract manifest, signature, signer_pubkey

3. LAYER 1: Verify Authorization (Ed25519)
   ├─ Check if signer_pubkey is in OTA_TRUSTED_KEYS[]
   │  ❌ Not trusted → reject immediately
   │
   └─ Verify Ed25519 signature:
      Ed25519_Verify(signature, manifest, signer_pubkey)
      ❌ Invalid → reject (not from authorized signer)
      ✅ Valid → proceed to download

4. Download all chunks via gossip protocol

5. LAYER 2: Verify Integrity (SHA256)
   └─ Verify SHA-256 hash of reassembled firmware:
      SHA256(firmware) == manifest.hash
      ❌ Mismatch → reject (corruption during transmission)
      ✅ Match → proceed to apply

6. Check battery > 50%
   ❌ Low battery → wait

7. Apply update → reboot
   ❌ Boot fails → auto-rollback
   ✅ Success → mark valid
```

**Why two layers?**
- Ed25519 catches: Unauthorized releases, forged signatures
- SHA256 catches: Transmission errors, corrupted chunks, bit flips

**Both are required for secure OTA!**

---

## CLI Commands Reference

### ota push-release

**Push OTA update directly from GitHub Release**

```bash
meshgrid-cli ota push-release [OPTIONS]

Options:
  --repo <org/repo>       GitHub repository
  --version <v0.0.3>      Specific version (or use --latest)
  --latest                Use latest release
  --board <board>         Board type (heltec_v3, lilygo_t3s3, etc.)
  --port <port>           Serial port (or "auto" to detect)
  --watch                 Watch progress after push

Examples:
  # Push latest release to auto-detected device
  meshgrid-cli ota push-release --repo yourorg/meshgrid-firmware --latest --port auto

  # Push specific version to specific device
  meshgrid-cli ota push-release --repo yourorg/meshgrid-firmware --version v0.0.3 --port /dev/ttyUSB0

  # Push and watch progress
  meshgrid-cli ota push-release --latest --port auto --watch
```

### ota push

**Push pre-downloaded signed firmware**

```bash
meshgrid-cli ota push <firmware.signed> [OPTIONS]

Options:
  --port <port>           Serial port (or "auto" to detect)
  --rate <rate>           Chunk send rate: slow/medium/fast (default: medium)
  --watch                 Watch progress after push

Examples:
  # Push downloaded firmware
  meshgrid-cli ota push firmware-heltec-v3-v0.0.3.signed --port auto

  # Push with slow rate (regulatory compliance)
  meshgrid-cli ota push firmware.signed --port /dev/ttyUSB0 --rate slow
```

### ota watch

**Monitor OTA deployment progress across mesh**

```bash
meshgrid-cli ota watch [OPTIONS]

Options:
  --port <port>           Serial port
  --interval <seconds>    Refresh interval (default: 5)
  --session <id>          Specific session ID (or "auto")

Example:
  meshgrid-cli ota watch --port /dev/ttyUSB0 --interval 5
```

### ota status

**Query OTA status of connected device**

```bash
meshgrid-cli ota status --port <port>

Example:
  meshgrid-cli ota status --port /dev/ttyUSB0

Output:
  OTA Status:
    Session:  0xABCD1234
    Version:  v0.0.3
    State:    Downloading
    Progress: 2956/3322 chunks (89%)
    Battery:  78%
    ETA:      ~5 minutes
```

### ota check

**Check for new releases without downloading**

```bash
meshgrid-cli ota check --repo <org/repo> [OPTIONS]

Options:
  --port <port>           Check current device version

Examples:
  # Check for new releases
  meshgrid-cli ota check --repo yourorg/meshgrid-firmware

  # Compare with device version
  meshgrid-cli ota check --repo yourorg/meshgrid-firmware --port /dev/ttyUSB0

Output:
  Latest release:  v0.0.3 (released 2 hours ago by @alice)
  Device version:  v0.0.2
  Update available: Yes

  To deploy: meshgrid-cli ota push-release --latest --port auto
```

### devices

**List all connected meshgrid devices**

```bash
meshgrid-cli devices

Output:
  Available meshgrid Devices:

  1. /dev/ttyUSB0
     Name:      mg-A1
     Board:     Heltec WiFi LoRa 32 V3
     Version:   v0.0.2
     Hash:      0xa1
     Neighbors: 4

  2. /dev/ttyACM0
     Name:      mg-B2
     Board:     LilyGo T3S3
     Version:   v0.0.2
     Hash:      0xb2
     Neighbors: 3

  Use: meshgrid-cli ota push-release --port /dev/ttyUSB0
```

---

## Firmware Size Tracking

### Current Sizes (2026-01-30)

| Board | Binary Size | Chunks (@174B) | Flash Partition | Headroom |
|-------|-------------|----------------|-----------------|----------|
| Heltec V3 (8MB) | 564 KB | 3,322 | 3 MB (OTA) | 5.3x |
| T3S3 (4MB) | 552 KB | 3,264 | 1.625 MB (OTA) | 2.9x |

### CI/CD Size Monitoring

**Automatic size checks in build workflow:**

```yaml
# .github/workflows/build.yml
- name: Check firmware sizes
  run: |
    for env in heltec_v3 lilygo_t3s3; do
      SIZE=$(stat -c%s .pio/build/$env/firmware.bin)
      SIZE_KB=$((SIZE / 1024))
      CHUNKS=$((SIZE / 174 + 1))

      echo "📦 $env: ${SIZE_KB} KB (${CHUNKS} chunks)"

      # Warn if over 600 KB
      if [ $SIZE -gt 614400 ]; then
        echo "::warning::$env firmware is ${SIZE_KB} KB (>600 KB) - OTA will be slow"
      fi

      # Fail if won't fit in OTA partition
      if [ "$env" = "heltec_v3" ] && [ $SIZE -gt 3145728 ]; then
        echo "::error::$env firmware too large for 3MB OTA partition"
        exit 1
      fi

      if [ "$env" = "lilygo_t3s3" ] && [ $SIZE -gt 1703936 ]; then
        echo "::error::$env firmware too large for 1.625MB OTA partition"
        exit 1
      fi
    done
```

### Size Limits

| Flash Size | OTA Partition | Max Firmware | Current Usage |
|------------|---------------|--------------|---------------|
| 8 MB | 3 MB | 3,072 KB | 564 KB (18%) ✅ |
| 4 MB | 1.625 MB | 1,664 KB | 552 KB (33%) ✅ |

**Plenty of headroom for future features!**

---

## Real-World Usage Examples

### Example 1: Quick Update (Office)

**Alice releases v0.0.3 from GitHub UI at 9:00 AM**

**You're in the office at 10:30 AM:**
```bash
# Check for updates
meshgrid-cli ota check --repo yourorg/meshgrid-firmware --port auto

# Output: New release v0.0.3 available (released 1.5 hours ago by @alice)

# Deploy
meshgrid-cli ota push-release --latest --port auto

# Done in 2 minutes, mesh distributes over next hour
```

**11:30 AM:** All 20 devices updated automatically.

### Example 2: Field Deployment (Remote Site)

**Bob releases v0.0.4 from GitHub UI**

**You're at remote site with laptop:**
```bash
# Download while you have internet
wget https://github.com/yourorg/meshgrid-firmware/releases/download/v0.0.4/firmware-heltec-v3-v0.0.4.signed

# Go offline, connect to device
meshgrid-cli ota push firmware-heltec-v3-v0.0.4.signed --port /dev/ttyUSB0

# Mesh distributes (no internet needed)
```

### Example 3: Windows User

**Charlie releases v0.0.5**

**You're on Windows laptop:**
```powershell
# Download from GitHub Releases in browser
# File: firmware-heltec-v3-v0.0.5.signed

# Open PowerShell
cd Downloads

# Connect device
meshgrid-cli ota push firmware-heltec-v3-v0.0.5.signed --port auto

# Output:
# ✅ Found device at COM4
# 📤 Pushing OTA...
# ✅ Done!
```

### Example 4: Multiple Meshes

**You have 3 separate mesh networks:**

```bash
# Network A (office)
meshgrid-cli ota push-release --latest --port /dev/ttyUSB0

# Network B (warehouse)
meshgrid-cli ota push-release --latest --port /dev/ttyUSB1

# Network C (outdoor)
meshgrid-cli ota push-release --latest --port /dev/ttyACM0

# Each mesh updates independently
```

---

## Troubleshooting

### "No device detected"

```bash
# List all serial ports
meshgrid-cli devices

# If empty, check:
1. Device connected via USB?
2. Correct USB cable (data, not charge-only)?
3. Driver installed? (Windows may need CH340/CP2102 driver)
4. Permissions? (Linux: sudo usermod -a -G dialout $USER)
```

### "Signature verification failed"

```bash
# Possible causes:
1. Firmware corrupted during download → re-download
2. Wrong board firmware → check filename
3. Unsigned firmware → must use .signed file from GitHub
4. Old firmware (before OTA keys added) → flash via USB first
```

### "Device not responding during OTA"

```bash
# Check device status
meshgrid-cli ota status --port /dev/ttyUSB0

# If stuck, abort and retry
meshgrid-cli ota abort --port /dev/ttyUSB0
meshgrid-cli ota push-release --latest --port /dev/ttyUSB0
```

### "Some devices not updating"

```bash
# Check which devices received OTA
meshgrid-cli neighbors --port /dev/ttyUSB0

# If device missing chunks:
1. Check battery level (won't apply if <50%)
2. Check signal strength (poor RSSI = slow download)
3. Wait longer (large networks take 1-2 hours)
4. Manual intervention: connect via USB and push directly
```

---

## Performance Characteristics

### Deployment Time Estimates

**Single device (USB direct):**
- Time: 2-5 minutes
- Bottleneck: Serial speed, flash writes

**Small mesh (2-5 devices, 1-2 hops):**
- Time: 15-30 minutes
- Bottleneck: LoRa airtime, chunk gossip

**Medium mesh (10-20 devices, 3-4 hops):**
- Time: 30-60 minutes
- Bottleneck: Multi-hop propagation delay

**Large mesh (50+ devices, 5+ hops):**
- Time: 1-2 hours
- Bottleneck: Network diameter, duty cycle limits

### Network Traffic

**Per-device traffic (564 KB firmware):**
- Receive: ~564 KB (firmware chunks)
- Transmit: ~200-400 KB (forwarding to neighbors)
- Total: ~1 MB per device

**Total network traffic (20 devices):**
- ~15-20 MB total (1.5-2x firmware size)
- Acceptable overhead for epidemic gossip

---

## Best Practices

### Before Deploying OTA

**1. Test on development devices first:**
```bash
# Flash dev device via USB
pio run -e heltec_v3 -t upload --upload-port /dev/ttyUSB0

# Verify it works
meshgrid-cli -p /dev/ttyUSB0 info

# If OK, proceed with OTA release
```

**2. Check mesh health:**
```bash
# Ensure devices can communicate
meshgrid-cli neighbors --port /dev/ttyUSB0

# Should see all expected neighbors
```

**3. Choose good timing:**
- ⚠️ Avoid peak usage hours
- ✅ Deploy during low-traffic periods
- ✅ Ensure devices have good battery levels

### During Deployment

**1. Monitor progress:**
```bash
meshgrid-cli ota watch --port /dev/ttyUSB0
```

**2. Don't disconnect:**
- Keep connected device powered during initial flood (~10 min)
- After that, can disconnect (mesh continues autonomously)

### After Deployment

**1. Verify all devices updated:**
```bash
# Check neighbors
meshgrid-cli neighbors --port /dev/ttyUSB0

# All should show new version
```

**2. Test functionality:**
```bash
# Send test messages
meshgrid-cli send --to "mg-B2" -- "Test after OTA" -p /dev/ttyUSB0

# Check if received
meshgrid-cli -p /dev/ttyUSB1 messages
```

**3. Monitor for issues:**
- Check for devices stuck on old version
- Check for boot loops (rollback triggered)
- Check neighbor table for connectivity issues

---

## Emergency Procedures

### Abort Failed OTA

```bash
# Cancel ongoing OTA session
meshgrid-cli ota abort --session auto --port /dev/ttyUSB0

# This floods ABORT message to all devices
# All devices discard OTA session and return to normal operation
```

### Manual Recovery

**If device won't boot after OTA:**

```bash
# Device auto-rolls back after 3 failed boot attempts
# But if you need to force recovery:

# 1. Connect via USB
# 2. Flash previous firmware
pio run -e heltec_v3 -t upload --upload-port /dev/ttyUSB0

# 3. Device recovers to working state
```

### Rollback Entire Mesh

**If bad OTA deployed to mesh:**

```bash
# 1. Build previous working version
git checkout v0.0.2
pio run -e heltec_v3

# 2. Sign as new version (v0.0.6 to be newer than v0.0.5 bad release)
meshgrid-cli ota sign .pio/build/heltec_v3/firmware.bin --version v0.0.6

# 3. Push to mesh
meshgrid-cli ota push firmware-v0.0.6.signed --port auto

# 4. All devices update to working firmware
```

---

## Summary

### Workflow

**For Team:**
1. Member tags release on GitHub → automated build & sign
2. Anyone downloads .signed file when convenient
3. Anyone pushes to any mesh device via CLI
4. Mesh distributes automatically
5. Done!

### Key Points

✅ **GitHub**: Automated build, sign, release
✅ **CLI**: Simple push command (works anywhere)
✅ **Mesh**: Automatic distribution (no manual work)
✅ **Security**: Ed25519 signatures, only team can release
✅ **Flexible**: Deploy when/where convenient
✅ **Resilient**: Survives failures, auto-rollback
✅ **Scalable**: Works for 2 devices or 200 devices

### One-Liner Deployment

```bash
meshgrid-cli ota push-release --repo yourorg/meshgrid-firmware --latest --port auto
```

**That's it!** 🚀
