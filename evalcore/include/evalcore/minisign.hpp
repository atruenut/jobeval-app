// Verification of minisign signatures (https://jedisct1.github.io/minisign/).
//
// Public key : base64 of  "Ed" | key id (8) | Ed25519 public key (32)
// Signature  : line 2 is base64 of  alg (2) | key id (8) | signature (64)
//              alg "ED" signs BLAKE2b-512(message) (the default), "Ed" signs the message
//              itself (legacy). Line 4 is a "global" signature over  signature | trusted
//              comment, so the trusted comment on line 3 is authenticated too.
#pragma once

#include <string>

#include "evalcore/hash.hpp"

namespace jobeval {

struct MinisignKey {
    uint8_t key_id[8] = {};
    uint8_t public_key[32] = {};
};

// Accepts the bare base64 key (as printed by minisign -G) or a full .pub file
// (an "untrusted comment:" line followed by the key line).
bool parse_minisign_public_key(const std::string& text, MinisignKey& out, std::string& error);

struct SignatureCheck {
    bool ok = false;
    std::string trusted_comment;  // authenticated, only meaningful when ok
    std::string error;
};

// Verifies `signature_file` (the contents of a .minisig file) over `message`.
SignatureCheck verify_minisign(const MinisignKey& key, const std::string& message, const std::string& signature_file);

}  // namespace jobeval
