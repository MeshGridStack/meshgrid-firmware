# Neighbor Discovery Testing Guide

This guide shows how to test and verify neighbor discovery functionality in MeshGrid firmware.

## Quick Test with Serial Monitor

### 1. Flash the Firmware
```bash
cd /home/bram/private/meshgrid/meshgrid-firmware
pio run -e heltec_v3 -t upload -t monitor
```

### 2. Check Initial State
In the serial monitor, type:
```
/stats
```

**Expected Output (No neighbors yet):**
```
Statistics:
  RX: 0
  TX: 0
  FWD: 0
  DROP: 0
  DUP: 0
Neighbors:
  Total: 0
  Clients: 0
  Repeaters: 0
  Rooms: 0
  Uptime: 45s
```

### 3. Send Advertisement
Force the device to send an advertisement:
```
/advert
```

**Expected Output:**
```
TX ADV LOCAL 0xA5 mg-A5
```

## Multi-Device Test Scenario

### Setup
You need at least 2 devices to test neighbor discovery:

#### Device 1: MeshGrid Client
```bash
# Flash MeshGrid firmware
cd /home/bram/private/meshgrid/meshgrid-firmware
pio run -e heltec_v3 -t upload
```

Connect serial and set name:
```
SET NAME Alice
/mode client
```

#### Device 2: MeshCore Repeater
```bash
# Flash MeshCore firmware with repeater example
cd /home/bram/private/MeshCore
./build.sh heltec_v3 examples/simple_repeater
```

#### Device 3: MeshCore Room Server (Optional)
```bash
cd /home/bram/private/MeshCore
./build.sh heltec_v3 examples/simple_room_server
```

### Expected Results After ~1 Minute

On Device 1 (MeshGrid - Alice), type `/stats`:

```
Statistics:
  RX: 245
  TX: 12
  FWD: 180
  DROP: 3
  DUP: 42
Neighbors:
  Total: 8
  Clients: 5       ← Other MeshCore/MeshGrid user nodes
  Repeaters: 2     ← rpt-* or relay-* nodes
  Rooms: 1         ← room-* or server-* nodes
  Uptime: 3600s
```

Type `/neighbors` for detailed info:
```
Neighbors:
  Alice (A5) RSSI:-42 SNR:8
  rpt-B3 (B3) RSSI:-55 SNR:6
  Bob (C7) RSSI:-60 SNR:5
  room-server (D2) RSSI:-48 SNR:7
```

## Testing with meshgrid-cli

### Monitor Real-Time Discovery
```bash
# Terminal 1: Monitor advertisements
meshgrid-cli -p /dev/ttyUSB0 monitor

# You should see:
# [14:23:45] ADV 0xB3 "rpt-relay1" (RSSI:-45)
# [14:23:52] ADV 0xC7 "Bob" (RSSI:-58)
# [14:24:03] ADV 0xD2 "room-server" (RSSI:-50)
```

### Check Neighbor Table
```bash
meshgrid-cli -p /dev/ttyUSB0 neighbors
```

**Expected Output:**
```
Neighbor Table (8 nodes):

  Hash     Name             RSSI   SNR    Last Seen
  -------- ---------------- ------ ------ --------
  0xA5     Alice            -42    8      2s ago
  0xB3     rpt-relay1       -55    6      5s ago
  0xC7     Bob              -60    5      8s ago
  0xD2     room-server      -48    7      3s ago
  0xE1     Carol            -52    7      12s ago
  0xF4     rpt-mesh2        -65    4      15s ago
  0x12     Dave             -58    6      7s ago
  0x34     Eve              -61    5      10s ago
```

## Automated Test Script

Run the provided test script:
```bash
cd /home/bram/private/meshgrid/meshgrid-firmware
chmod +x test_neighbor_discovery.sh
./test_neighbor_discovery.sh /dev/ttyUSB0
```

## Node Type Classification

The firmware automatically classifies nodes based on their names:

### Repeaters (Infrastructure)
- Names starting with: `rpt-`, `RPT`
- Names containing: `relay`, `Relay`, `repeater`, `Repeater`
- Examples: `rpt-mesh1`, `relay-tower`, `Repeater-A`

### Rooms (Servers/Fixed)
- Names starting with: `room-`, `Room`
- Names containing: `server`, `Server`
- Examples: `room-server`, `Room-Kitchen`, `mqtt-server`

### Clients (Default)
- All other names
- Examples: `Alice`, `Bob`, `mg-A5`, `node-123`

## Troubleshooting

### No Neighbors Detected
1. Check radio is initialized:
   ```
   /info
   ```
   Should show frequency and TX power

2. Check for received packets:
   ```
   /stats
   ```
   RX counter should be > 0

3. Verify LoRa settings match:
   ```
   CONFIG
   ```
   Frequency, bandwidth, SF should match other devices

4. Force advertisement:
   ```
   ADVERT
   ```

### Neighbors Timeout
- Normal behavior! MeshGrid doesn't auto-remove neighbors
- As per user request: "it sometimes cost 3 days to see all people"
- Neighbor table keeps entries until table is full (32 max)

### Statistics Not Incrementing
1. Enable logging to see receive events:
   ```
   LOG ENABLE
   LOG
   ```

2. Enable monitor mode for real-time output:
   ```
   MONITOR
   ```
   Press any key to exit monitor mode

## Expected Behavior

### Successful Discovery
- **RX counter increases** when packets received
- **Neighbors Total increases** when new advertisements parsed
- **Type counters increment** based on name classification
- **RSSI/SNR values** update on each advertisement
- **Log shows** `RX ADV 0x<hash> <rssi>dBm <name>`

### Advertisement Timing
- **Local (ROUTE_DIRECT)**: On demand via `/advert` or button press
- **Flood (ROUTE_FLOOD)**: Every 12 hours automatic
- **Force flood**: `ADVERT FLOOD` command

## Integration with MeshCore Network

MeshGrid is compatible with MeshCore. When running in a MeshCore network:

1. MeshGrid nodes will appear as standard clients
2. MeshCore repeaters will be classified as "Repeaters"
3. MeshCore room servers will be classified as "Rooms"
4. All nodes can see each other via flood routing
5. Messages are forwarded through the mesh

### Verify MeshCore Compatibility
```bash
# Send group message to public channel
SEND GROUP Hello MeshCore network!

# MeshCore nodes will receive and can reply
```

## Success Criteria

✅ Stats show neighbor counts increasing
✅ `/neighbors` command lists discovered nodes
✅ Node types correctly classified
✅ RSSI/SNR values are reasonable (-30 to -100 dBm)
✅ RX counter increments on advertisements
✅ meshgrid-cli `neighbors` command works
✅ Monitor mode shows live advertisements

## Files Modified for This Feature

- `app/neighbors.cpp` - Neighbor table management
- `app/messaging.cpp` - Advertisement reception
- `app/commands.cpp` - Stats and neighbor commands
- `main.cpp` - Variables exported for neighbor tracking
