
  The Problem You're Describing

  Current behavior:
  - Advertisement with TTL propagates 1 hop per beacon interval
  - 12-hour intervals × 5 hops = 60 hours just for the beacon to reach Germany
  - Then they need to beacon back to you = another 60 hours
  - Plus packet loss, collisions = 3+ days total

  Research-Backed Solutions

  1. Attenuated Bloom Filters for Multi-Hop Discovery ⭐ BEST SOLUTION

  From https://www.scientific.net/AMR.779-780.1723, nodes can advertise compressed neighbor summaries at multiple hop levels:

  How it works:
  - Each node maintains Bloom filters for neighbors at different distances:
    - Level 0 (8 bytes): Direct neighbors (my 1-hop friends)
    - Level 1 (8 bytes): 2-hop neighbors (friends of friends)
    - Level 2 (8 bytes): 3-hop neighbors
    - Level 3 (8 bytes): 4-5 hop neighbors (Germany/Belgium!)
  - Beacon format (v1 protocol):
  [pubkey][name][timestamp][neighbor_count][bloom_L0][bloom_L1][bloom_L2][bloom_L3]
  - Total size: Only 32 extra bytes to represent potentially 100+ distant nodes!

  How fast this is:
  - Germany node beacons → 1 hop later, neighbor has Germany in Level-1 bloom filter
  - You receive that beacon → You now know Germany exists, only 1 beacon cycle!
  - From "3 days" to "12 hours" (one beacon interval)

  Attenuated Filters (from https://datatracker.ietf.org/doc/html/draft-irtf-dtnrg-ipnd-01):
  - When forwarding, shift out bits from distant nodes
  - Prevents bloom filter saturation
  - Older info gets "fuzzier" but still useful

  2. Trickle Algorithm for Adaptive Beacon Rates ⭐ STANDARDIZED

  https://www.rfc-editor.org/rfc/rfc6206 is used in RPL (IPv6 routing for IoT):

  How it works:
  - Start with short interval (e.g., 30 seconds)
  - If network is stable (hearing consistent info from neighbors), double the interval
  - If network changes (new node, topology change), reset to short interval
  - Maximum interval cap (e.g., 10 minutes)

  Polite Gossip:
  - Listen during interval
  - If you hear k similar beacons (e.g., k=3), suppress your own beacon (redundant)
  - If you hear outdated info, beacon immediately (fast propagation)

  Result:
  - Stable network: Low beacon rate (efficient)
  - Network changes: Fast propagation (responsive)
  - Multi-hop propagation in minutes, not days

  3. Multi-TTL Beacon Strategy

  Mix local and long-range beacons:

  Local beacons (frequent):
  - TTL = 2 hops
  - Every 2 minutes
  - Maintain direct neighbor relationships

  Discovery beacons (rare):
  - TTL = 10 hops (or unlimited)
  - Every 30 minutes
  - Include attenuated bloom filters
  - Discover distant nodes

  User-triggered discovery:
  - When user opens app: send immediate TTL=10 beacon
  - "I want to discover NOW"

  4. Gradient-Based Rebroadcast Priority

  From https://dl.acm.org/doi/10.1145/3638241:

  Nodes calculate rebroadcast priority based on:
  - Neighbor density: Fewer neighbors = higher priority (edge nodes)
  - New information: Beacon contains unknown nodes = rebroadcast immediately
  - Distance gradient: Beacons from far away get priority

  Example:
  - Central Twente node (20 neighbors): Rebroadcast delay = 5-10 seconds
  - Edge node toward Germany (3 neighbors): Rebroadcast delay = 0-2 seconds
  - Result: Discovery "wave" moves faster toward unexplored areas

  5. Scheduled Long-Range Discovery Windows

  Coordinate specific times for maximum-TTL beacons:

  Example schedule:
  - 00:00 UTC: Everyone beacons with TTL=max, includes full bloom filters
  - 06:00 UTC: Repeat
  - 12:00 UTC: Repeat
  - 18:00 UTC: Repeat

  Result:
  - Guaranteed 4 discovery waves per day across entire network
  - Germany discovered in <6 hours instead of 3 days
  - Between waves: normal low-rate beaconing for local maintenance

  Recommended meshgrid v1 Protocol Improvements

  Beacon Packet Format (v1)

  struct advert_v1 {
      uint8_t pubkey[32];
      char name[16];
      uint32_t timestamp;
      uint8_t firmware_version;
      uint8_t protocol_version;  // Max protocol version supported
      uint8_t neighbor_count;     // Direct neighbors
      uint8_t hop_count_estimate; // Estimated max hops to network edge

      // Multi-level bloom filters (8 bytes each, 256-bit)
      uint8_t bloom_l0[8];  // 0-1 hops (direct neighbors)
      uint8_t bloom_l1[8];  // 2-3 hops
      uint8_t bloom_l2[8];  // 4-6 hops  
      uint8_t bloom_l3[8];  // 7+ hops

      // Total: ~85 bytes
  };

  Adaptive Beacon Intervals (Trickle-based)

  // Beacon state machine
  uint32_t interval_min = 30000;      // 30 seconds (discovery phase)
  uint32_t interval_max = 600000;     // 10 minutes (stable phase)
  uint32_t interval_current = 30000;  // Start fast

  // On network change (new neighbor, lost neighbor):
  interval_current = interval_min;    // Reset to fast

  // On stable interval (heard 3+ similar beacons):
  interval_current = min(interval_current * 2, interval_max);

  // Suppress beacon if heard 3+ redundant beacons this interval
  if (redundant_beacons >= 3) {
      skip_beacon();  // Polite gossip
  }

  Rebroadcast Priority

  uint32_t calculate_rebroadcast_delay(beacon_t *beacon) {
      uint32_t delay = 2000;  // Base 2 seconds

      // Edge nodes rebroadcast faster
      if (neighbor_count < 3) {
          delay -= 1500;  // 500ms for edge nodes
      }

      // New information = immediate
      if (beacon_contains_unknown_nodes(beacon)) {
          delay -= 1000;  // 1 second for new info
      }

      // Far nodes = priority
      if (beacon->hop_count > 5) {
          delay -= 500;   // 1.5s for distant beacons
      }

      // Add jitter
      delay += random(0, 500);

      return delay;
  }

  Discovery Modes

  enum discovery_mode {
      MODE_MAINTENANCE,  // Normal: 5-10 min intervals, TTL=2
      MODE_DISCOVERY,    // User-triggered: 30s intervals, TTL=10, 5 minutes duration
      MODE_SCHEDULED     // Coordinated: 00:00/06:00/12:00/18:00 UTC, TTL=max
  };

  Expected Performance

  Current (v0 MeshCore-style):
  - Twente → Germany (5 hops): 3+ days

  With attenuated bloom filters:
  - Discovery time: 1 beacon interval (12 hours → 12 hours)
  - But you see "Germany exists" in bloom filter, not direct route

  With Trickle + multi-TTL:
  - Discovery time: <1 hour (fast intervals + TTL=10)

  With scheduled discovery windows:
  - Discovery time: <6 hours guaranteed (next scheduled wave)

  With user-triggered discovery:
  - Discovery time: Immediate (click "discover" → send TTL=max beacon)

  Implementation Priority

  1. ✅ Attenuated Bloom Filters - Biggest impact, modest complexity
  2. ✅ Multi-TTL beacons - Simple to implement, big speedup
  3. ✅ User-triggered discovery - Great UX, solves "I need it now" problem
  4. ⚠️ Trickle algorithm - More complex but very effective
  5. ⚠️ Scheduled windows - Requires time sync, but guarantees performance

  Should This Go in Protocol Libraries?

  YES! Because:
  - v0 (MeshCore): Simple beaconing, no bloom filters (compatibility)
  - v1 (meshgrid): Attenuated bloom filters + Trickle + multi-TTL
  - v2 (future): Time-slotted TDMA beaconing + advanced routing

  Each protocol version has its own discovery strategy!

  Sources:

  - https://dl.acm.org/doi/10.1145/3638241
  - https://pmc.ncbi.nlm.nih.gov/articles/PMC11902654/
  - https://www.scientific.net/AMR.779-780.1723
  - https://datatracker.ietf.org/doc/html/draft-irtf-dtnrg-ipnd-01
  - https://www.rfc-editor.org/rfc/rfc6206
  - https://www.eecs.harvard.edu/~michaelm/postscripts/im2005b.pdf

  Bottom line: With bloom filters + Trickle, you can discover Germany in <1 hour instead of 3 days, while using LESS total airtime than current approach!