# MeshGrid Test Suite

Automated testing for multiple connected MeshGrid devices.

## Test Script: test-devices.sh

Comprehensive test suite that automatically discovers connected USB devices and runs tests on them.

### Features

- **Automatic Device Discovery**: Finds all connected MeshGrid devices on /dev/ttyUSB* and /dev/ttyACM*
- **PIN Authentication**: Handles PIN authentication securely (prompts user for PINs)
- **Comprehensive Testing**:
  - Basic connectivity (PING, INFO, STATS)
  - Neighbor discovery between devices
  - Radio communication (public and private messages)
  - Statistics verification

### Usage

```bash
# Run all tests
./test/test-devices.sh
```

### Test Flow

1. **Device Discovery**
   - Scans all USB serial ports
   - Identifies MeshGrid devices
   - Shows device name and hash

2. **Authentication**
   - Tests if PIN auth is enabled
   - Prompts for PIN (view on OLED Security screen)
   - Authenticates each device

3. **Basic Connectivity**
   - Tests PING/PONG
   - Verifies INFO command
   - Checks STATS command

4. **Neighbor Discovery** (2+ devices)
   - Waits for advertisements
   - Verifies devices see each other as neighbors
   - Shows RSSI/SNR values

5. **Radio Communication** (2+ devices)
   - Sends public broadcast messages
   - Sends private direct messages
   - Verifies message reception

6. **Statistics**
   - Verifies RX packet counts
   - Checks neighbor counts
   - Shows traffic statistics

### Requirements

- **meshgrid-cli** must be in PATH
- At least 1 MeshGrid device connected
- For neighbor/radio tests: 2+ devices recommended

### Example Output

```
╔════════════════════════════════════════╗
║   MeshGrid Multi-Device Test Suite    ║
╚════════════════════════════════════════╝

[Phase 1] Device Discovery

Scanning USB ports...
  Checking /dev/ttyUSB0 ... Found: node-12 (0x12)
  Checking /dev/ttyACM0 ... Found: node-52 (0x52)

Found 2 MeshGrid device(s)

[Phase 2] Authentication

Testing node-12 (/dev/ttyUSB0)...
  Authentication required
  View PIN on OLED: Navigate to Security screen (press button)

Enter PIN for node-12 (/dev/ttyUSB0): 123456
  [TEST 1] Authenticate with PIN ... PASS

Testing node-52 (/dev/ttyACM0)...
  Auth disabled or already authenticated

[Phase 3] Basic Connectivity

Testing node-12 (/dev/ttyUSB0):
  [TEST 2] PING response ... PASS
  [TEST 3] INFO command ... PASS
  [TEST 4] STATS command ... PASS

Testing node-52 (/dev/ttyACM0):
  [TEST 5] PING response ... PASS
  [TEST 6] INFO command ... PASS
  [TEST 7] STATS command ... PASS

[Phase 4] Neighbor Discovery

Waiting 10 seconds for neighbor advertisements...

Checking neighbors of node-12 (/dev/ttyUSB0):
  [TEST 8] NEIGHBORS command ... PASS
  [TEST 9] Can see node-52 as neighbor ... PASS

Checking neighbors of node-52 (/dev/ttyACM0):
  [TEST 10] NEIGHBORS command ... PASS
  [TEST 11] Can see node-12 as neighbor ... PASS

[Phase 5] Radio Communication

Testing: node-12 → node-52

  [TEST 12] Send public message ... PASS
  [TEST 13] Receive public message ... PASS
  [TEST 14] Send private message ... PASS
  [TEST 15] Receive private message ... PASS

[Phase 6] Statistics Verification

Checking node-12 (/dev/ttyUSB0):
  [TEST 16] Has RX packets ... PASS
    ↳ RX: 47 packets
  [TEST 17] Has neighbor count ... PASS
    ↳ Neighbors: 1

Checking node-52 (/dev/ttyACM0):
  [TEST 18] Has RX packets ... PASS
    ↳ RX: 45 packets
  [TEST 19] Has neighbor count ... PASS
    ↳ Neighbors: 1

╔════════════════════════════════════════╗
║           Test Summary                 ║
╚════════════════════════════════════════╝

  Total Tests:  19
  Passed:       19
  Failed:       0

✓ All tests passed!
```

### Troubleshooting

**No devices found:**
- Check USB connections
- Verify devices are flashed with firmware
- Run `meshgrid-cli ports` to list available ports

**Authentication fails:**
- View PIN on OLED (navigate to Security screen with button)
- Use `/pin show` command after authenticating once
- Disable PIN temporarily with `/pin disable`

**Neighbor discovery fails:**
- Ensure devices are on same radio preset (EU/US)
- Check radio is working: `meshgrid-cli -p /dev/ttyUSB0 info`
- Increase wait time in script (change `sleep 10` to longer)

**Tests timeout:**
- Increase `TIMEOUT` variable in script
- Check device isn't busy (no other programs accessing serial port)

### Integration with CI/CD

The script exits with code 0 on success, 1 on failure, making it suitable for CI/CD pipelines:

```bash
#!/bin/bash
# CI test script
set -e

# Flash devices
pio run -e heltec_v3 --target upload --upload-port /dev/ttyUSB0
pio run -e lilygo_t3s3 --target upload --upload-port /dev/ttyACM0

# Wait for boot
sleep 5

# Run tests
./test/test-devices.sh
```
