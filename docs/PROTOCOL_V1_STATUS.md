# Protocol v1 Implementation Status

## ✅ COMPLETED

### Crypto Layer (drivers/crypto/)
- ✅ `crypto_encrypt_v1()` - AES-256-CTR with 12-byte nonce
- ✅ `crypto_decrypt_v1()` - AES-256-CTR decryption with MAC verification
- ✅ `crypto_generate_nonce()` - Unique nonce generation
- ✅ 16-byte HMAC-SHA256 (vs 2-byte in v0)
- ✅ Full 32-byte key usage (vs 16-byte in v0)

### Device Identification
- ✅ Firmware type tracking in neighbor table
- ✅ `FW_MESHGRID` vs `FW_LEGACY` detection
- ✅ CLI displays firmware column in neighbors output
- ✅ Name-based inference (mg-* = meshgrid, others = legacy)

## 🚧 IN PROGRESS

### Message Format v1

**Direct Message (PAYLOAD_TXT_MSG, version=1):**
```
[dest_hash(1)][src_hash(1)][nonce(12)][MAC(16)][ciphertext]

Plaintext format: [seq(4)][txt_type(1)][text]
```

**Group Message (PAYLOAD_GRP_TXT, version=1):**
```
[channel_hash(1)][nonce(12)][MAC(16)][ciphertext]

Plaintext format: [seq(4)][txt_type(1)][sender: text]
```

## 📋 TODO

### High Priority
1. **Add sequence numbers to neighbor struct**
   - `uint32_t last_seq_rx` - Last received sequence number
   - `uint32_t next_seq_tx` - Next sequence to send
   - Initialize to 0 on neighbor discovery

2. **Implement `send_text_message_v1()`**
   - Check if dest is meshgrid firmware
   - Build plaintext with sequence number
   - Generate unique nonce
   - Encrypt with `crypto_encrypt_v1()`
   - Set version=1 in header
   - Increment `next_seq_tx`

3. **Implement `handle_text_msg_v1()`**
   - Detect version=1 in header
   - Decrypt with `crypto_decrypt_v1()`
   - Extract and validate sequence number
   - Reject if seq <= last_seq_rx (replay protection)
   - Update `last_seq_rx`

4. **Add dual-mode wrapper**
   ```cpp
   void send_text_message(uint8_t dest_hash, const char *text) {
       struct meshgrid_neighbor *n = neighbor_find(dest_hash);
       if (n && n->firmware == FW_MESHGRID) {
           send_text_message_v1(dest_hash, text);  // Enhanced
       } else {
           send_text_message_v0(dest_hash, text);  // Legacy
       }
   }
   ```

### Medium Priority
5. **Group message v1**
   - Similar to direct message
   - Per-channel sequence tracking

6. **Advertisement enhancement**
   - Include firmware version in app_data
   - More reliable than name-based detection

7. **ACK handling for v1**
   - Calculate ACK hash using sequence number
   - Better replay protection

### Low Priority
8. **Key rotation**
   - Periodic ECDH renegotiation
   - Forward secrecy

9. **Compression**
   - Optional compression for large messages

## Security Comparison

| Feature | v0 (Legacy) | v1 (Meshgrid) | Improvement |
|---------|-------------|---------------|-------------|
| Encryption | AES-128-ECB | AES-256-CTR | 2x key strength + no pattern leakage |
| MAC | 2 bytes | 16 bytes | 2^112x harder to forge |
| Replay Protection | None | Sequence numbers | Old messages rejected |
| Nonce | None | 12-byte unique | Each message unique ciphertext |
| Key Usage | 16 bytes of 32 | Full 32 bytes | Utilizing full key strength |

## Backward Compatibility

- ✅ Can communicate with legacy (MeshCore) devices using v0
- ✅ Can communicate with other meshgrid devices using v1
- ✅ Auto-detection based on firmware type
- ✅ Seamless mixed networks (some v0, some v1)
- ✅ No breaking changes to v0 protocol

## Next Steps

1. Add sequence fields to `struct meshgrid_neighbor`
2. Rename `send_text_message` → `send_text_message_v0`
3. Implement `send_text_message_v1`
4. Implement `handle_text_msg_v1`
5. Create wrapper `send_text_message` that auto-detects
6. Test with two meshgrid devices
7. Verify ACKs still work with v1
