#!/bin/bash
#
# MeshGrid Multi-Device Test Suite
#
# Automatically detects connected USB devices and runs comprehensive tests:
# - Device discovery and authentication
# - Neighbor detection
# - Radio communication
# - Public and private messaging
# - Statistics verification
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test results
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

# Device tracking
declare -A DEVICE_INFO     # port -> "hash:name"
declare -A DEVICE_PIN      # port -> PIN
declare -a DEVICE_PORTS    # List of valid ports

# Timeout for commands
TIMEOUT=5

echo -e "${BLUE}╔════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   MeshGrid Multi-Device Test Suite    ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════╝${NC}"
echo ""

# Check if meshgrid-cli is available
if ! command -v meshgrid-cli &> /dev/null; then
    echo -e "${RED}ERROR: meshgrid-cli not found in PATH${NC}"
    echo "Install with: cd meshgrid-cli && cargo install --path ."
    exit 1
fi

# ============================================================================
# Test Helper Functions
# ============================================================================

test_start() {
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
    echo -n "  [TEST $TESTS_TOTAL] $1 ... "
}

test_pass() {
    TESTS_PASSED=$((TESTS_PASSED + 1))
    echo -e "${GREEN}PASS${NC}"
}

test_fail() {
    TESTS_FAILED=$((TESTS_FAILED + 1))
    echo -e "${RED}FAIL${NC}"
    if [ -n "$1" ]; then
        echo -e "    ${RED}↳ $1${NC}"
    fi
}

test_skip() {
    echo -e "${YELLOW}SKIP${NC} ($1)"
}

# ============================================================================
# Device Discovery
# ============================================================================

discover_devices() {
    echo -e "${BLUE}[Phase 1] Device Discovery${NC}"
    echo ""

    # Find all potential serial devices
    local ports=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || true)

    if [ -z "$ports" ]; then
        echo -e "${RED}No USB serial devices found${NC}"
        exit 1
    fi

    echo "Scanning USB ports..."
    for port in $ports; do
        echo -n "  Checking $port ... "

        # Try to get device info
        if timeout $TIMEOUT meshgrid-cli -p "$port" info &> /tmp/mg_test_$$.txt; then
            # Extract hash and name
            local hash=$(grep '"hash"' /tmp/mg_test_$$.txt | cut -d'"' -f4)
            local name=$(grep '"name"' /tmp/mg_test_$$.txt | cut -d'"' -f4)

            if [ -n "$hash" ] && [ -n "$name" ]; then
                echo -e "${GREEN}Found: $name (0x$hash)${NC}"
                DEVICE_INFO[$port]="$hash:$name"
                DEVICE_PORTS+=("$port")
            else
                echo -e "${YELLOW}Not a MeshGrid device${NC}"
            fi
        else
            echo -e "${YELLOW}No response${NC}"
        fi
        rm -f /tmp/mg_test_$$.txt
    done

    echo ""
    echo -e "${GREEN}Found ${#DEVICE_PORTS[@]} MeshGrid device(s)${NC}"

    if [ ${#DEVICE_PORTS[@]} -eq 0 ]; then
        echo -e "${RED}No MeshGrid devices found. Flash firmware first.${NC}"
        exit 1
    fi

    echo ""
}

# ============================================================================
# Authentication
# ============================================================================

get_pin_from_user() {
    local port=$1
    local info=${DEVICE_INFO[$port]}
    local name=$(echo $info | cut -d: -f2)

    echo ""
    read -p "Enter PIN for $name ($port): " pin
    DEVICE_PIN[$port]=$pin
}

authenticate_devices() {
    echo -e "${BLUE}[Phase 2] Authentication${NC}"
    echo ""

    # First, try to connect to each device to see if auth is needed
    for port in "${DEVICE_PORTS[@]}"; do
        local info=${DEVICE_INFO[$port]}
        local name=$(echo $info | cut -d: -f2)

        echo "Testing $name ($port)..."

        # Try PING without auth
        if echo "PING" | timeout 2 meshgrid-cli -p "$port" - 2>&1 | grep -q "PONG"; then
            echo -e "  ${YELLOW}Auth disabled or already authenticated${NC}"
            DEVICE_PIN[$port]="none"
        else
            # Need authentication - get PIN from user
            echo -e "  ${YELLOW}Authentication required${NC}"
            echo "  View PIN on OLED: Navigate to Security screen (press button)"
            get_pin_from_user "$port"

            # Test authentication
            test_start "Authenticate with PIN"
            if echo "AUTH ${DEVICE_PIN[$port]}" | timeout 2 meshgrid-cli -p "$port" - 2>&1 | grep -q "OK"; then
                test_pass
            else
                test_fail "Invalid PIN"
                exit 1
            fi
        fi
    done

    echo ""
}

# ============================================================================
# Basic Connectivity Tests
# ============================================================================

test_basic_connectivity() {
    echo -e "${BLUE}[Phase 3] Basic Connectivity${NC}"
    echo ""

    for port in "${DEVICE_PORTS[@]}"; do
        local info=${DEVICE_INFO[$port]}
        local name=$(echo $info | cut -d: -f2)

        echo "Testing $name ($port):"

        # Test PING
        test_start "PING response"
        if echo -e "AUTH ${DEVICE_PIN[$port]}\nPING" | timeout 2 meshgrid-cli -p "$port" - 2>&1 | grep -q "PONG"; then
            test_pass
        else
            test_fail
        fi

        # Test INFO command
        test_start "INFO command"
        if echo -e "AUTH ${DEVICE_PIN[$port]}\nINFO" | timeout 2 meshgrid-cli -p "$port" - 2>&1 | grep -q "\"hash\""; then
            test_pass
        else
            test_fail
        fi

        # Test STATS command
        test_start "STATS command"
        if echo -e "AUTH ${DEVICE_PIN[$port]}\nSTATS" | timeout 2 meshgrid-cli -p "$port" - 2>&1 | grep -q "\"packets_rx\""; then
            test_pass
        else
            test_fail
        fi

        echo ""
    done
}

# ============================================================================
# Neighbor Discovery Tests
# ============================================================================

test_neighbor_discovery() {
    echo -e "${BLUE}[Phase 4] Neighbor Discovery${NC}"
    echo ""

    if [ ${#DEVICE_PORTS[@]} -lt 2 ]; then
        echo -e "${YELLOW}Skipping: Need at least 2 devices for neighbor tests${NC}"
        echo ""
        return
    fi

    echo "Waiting 10 seconds for neighbor advertisements..."
    sleep 10

    for port in "${DEVICE_PORTS[@]}"; do
        local info=${DEVICE_INFO[$port]}
        local name=$(echo $info | cut -d: -f2)
        local hash=$(echo $info | cut -d: -f1)

        echo "Checking neighbors of $name ($port):"

        # Get neighbor list
        test_start "NEIGHBORS command"
        local neighbors=$(echo -e "AUTH ${DEVICE_PIN[$port]}\nNEIGHBORS" | timeout 5 meshgrid-cli -p "$port" - 2>&1)

        if echo "$neighbors" | grep -q "\"hash\""; then
            test_pass

            # Check if other devices are visible as neighbors
            for other_port in "${DEVICE_PORTS[@]}"; do
                if [ "$port" != "$other_port" ]; then
                    local other_info=${DEVICE_INFO[$other_port]}
                    local other_hash=$(echo $other_info | cut -d: -f1)
                    local other_name=$(echo $other_info | cut -d: -f2)

                    test_start "Can see $other_name as neighbor"
                    if echo "$neighbors" | grep -q "\"hash\":\"$other_hash\""; then
                        test_pass
                    else
                        test_fail "Not visible yet (may need more time)"
                    fi
                fi
            done
        else
            test_fail
        fi

        echo ""
    done
}

# ============================================================================
# Radio Communication Tests
# ============================================================================

test_radio_communication() {
    echo -e "${BLUE}[Phase 5] Radio Communication${NC}"
    echo ""

    if [ ${#DEVICE_PORTS[@]} -lt 2 ]; then
        echo -e "${YELLOW}Skipping: Need at least 2 devices for radio tests${NC}"
        echo ""
        return
    fi

    local sender_port=${DEVICE_PORTS[0]}
    local receiver_port=${DEVICE_PORTS[1]}

    local sender_info=${DEVICE_INFO[$sender_port]}
    local sender_name=$(echo $sender_info | cut -d: -f2)

    local receiver_info=${DEVICE_INFO[$receiver_port]}
    local receiver_name=$(echo $receiver_info | cut -d: -f2)

    echo "Testing: $sender_name → $receiver_name"
    echo ""

    # Test public message
    test_start "Send public message"
    local test_msg="Test-$(date +%s)"
    if echo -e "AUTH ${DEVICE_PIN[$sender_port]}\nSEND $test_msg" | timeout 3 meshgrid-cli -p "$sender_port" - > /dev/null 2>&1; then
        test_pass
    else
        test_fail
    fi

    # Wait for message propagation
    sleep 3

    # Check if receiver got the message
    test_start "Receive public message"
    local messages=$(echo -e "AUTH ${DEVICE_PIN[$receiver_port]}\nMESSAGES" | timeout 3 meshgrid-cli -p "$receiver_port" - 2>&1)
    if echo "$messages" | grep -q "$test_msg"; then
        test_pass
    else
        test_fail "Message not received"
    fi

    echo ""

    # Test private message (if supported)
    local receiver_hash=$(echo $receiver_info | cut -d: -f1)

    test_start "Send private message"
    local test_dm="DM-$(date +%s)"
    if echo -e "AUTH ${DEVICE_PIN[$sender_port]}\nSEND $receiver_hash $test_dm" | timeout 3 meshgrid-cli -p "$sender_port" - > /dev/null 2>&1; then
        test_pass
    else
        test_fail
    fi

    # Wait for message propagation
    sleep 3

    # Check if receiver got the DM
    test_start "Receive private message"
    messages=$(echo -e "AUTH ${DEVICE_PIN[$receiver_port]}\nMESSAGES" | timeout 3 meshgrid-cli -p "$receiver_port" - 2>&1)
    if echo "$messages" | grep -q "$test_dm"; then
        test_pass
    else
        test_fail "DM not received"
    fi

    echo ""
}

# ============================================================================
# Statistics Verification
# ============================================================================

test_statistics() {
    echo -e "${BLUE}[Phase 6] Statistics Verification${NC}"
    echo ""

    for port in "${DEVICE_PORTS[@]}"; do
        local info=${DEVICE_INFO[$port]}
        local name=$(echo $info | cut -d: -f2)

        echo "Checking $name ($port):"

        # Get stats
        local stats=$(echo -e "AUTH ${DEVICE_PIN[$port]}\nSTATS" | timeout 3 meshgrid-cli -p "$port" - 2>&1)

        test_start "Has RX packets"
        local rx=$(echo "$stats" | grep '"packets_rx"' | grep -o '[0-9]*' | head -1)
        if [ -n "$rx" ] && [ "$rx" -gt 0 ]; then
            test_pass
            echo "    ↳ RX: $rx packets"
        else
            test_fail "No packets received"
        fi

        test_start "Has neighbor count"
        local neighbors=$(echo "$stats" | grep '"neighbor_count"' | grep -o '[0-9]*' | head -1)
        if [ -n "$neighbors" ]; then
            test_pass
            echo "    ↳ Neighbors: $neighbors"
        else
            test_fail
        fi

        echo ""
    done
}

# ============================================================================
# Test Report
# ============================================================================

print_report() {
    echo ""
    echo -e "${BLUE}╔════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║           Test Summary                 ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════╝${NC}"
    echo ""
    echo "  Total Tests:  $TESTS_TOTAL"
    echo -e "  ${GREEN}Passed:       $TESTS_PASSED${NC}"
    echo -e "  ${RED}Failed:       $TESTS_FAILED${NC}"
    echo ""

    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}✓ All tests passed!${NC}"
        exit 0
    else
        echo -e "${RED}✗ Some tests failed${NC}"
        exit 1
    fi
}

# ============================================================================
# Main Test Flow
# ============================================================================

main() {
    discover_devices
    authenticate_devices
    test_basic_connectivity
    test_neighbor_discovery
    test_radio_communication
    test_statistics
    print_report
}

# Run tests
main
