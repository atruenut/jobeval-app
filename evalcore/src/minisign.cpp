#include "evalcore/minisign.hpp"

#include <cstring>
#include <sstream>
#include <vector>

extern "C" {
#include "monocypher.h"
}

namespace jobeval {

namespace {

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

// Standard Ed25519 (RFC 8032) verification: h = SHA-512(R || A || M) reduced mod L, then
// Monocypher's constant-checked curve equation.
bool ed25519_verify(const uint8_t signature[64], const uint8_t public_key[32], const uint8_t* msg, size_t msg_size) {
    Bytes buf;
    buf.reserve(64 + msg_size);
    buf.insert(buf.end(), signature, signature + 32);
    buf.insert(buf.end(), public_key, public_key + 32);
    buf.insert(buf.end(), msg, msg + msg_size);
    auto h = sha512(buf.data(), buf.size());
    uint8_t reduced[32];
    crypto_eddsa_reduce(reduced, h.data());
    return crypto_eddsa_check_equation(signature, public_key, reduced) == 0;
}

}  // namespace

bool parse_minisign_public_key(const std::string& text, MinisignKey& out, std::string& error) {
    std::string b64;
    for (const auto& line : split_lines(text)) {
        if (line.rfind("untrusted comment:", 0) == 0) continue;
        b64 = line;
        break;
    }
    Bytes raw;
    if (b64.empty() || !base64_decode(b64, raw)) {
        error = "public key is not valid base64";
        return false;
    }
    if (raw.size() != 42 || raw[0] != 'E' || raw[1] != 'd') {
        error = "not a minisign public key (expected 42 bytes starting with \"Ed\")";
        return false;
    }
    std::memcpy(out.key_id, raw.data() + 2, 8);
    std::memcpy(out.public_key, raw.data() + 10, 32);
    return true;
}

SignatureCheck verify_minisign(const MinisignKey& key, const std::string& message, const std::string& signature_file) {
    SignatureCheck res;
    auto fail = [&](const std::string& why) {
        res.ok = false;
        res.trusted_comment.clear();
        res.error = why;
        return res;
    };

    const auto lines = split_lines(signature_file);
    if (lines.size() < 4 || lines[0].rfind("untrusted comment:", 0) != 0 || lines[2].rfind("trusted comment:", 0) != 0) {
        return fail("malformed signature file");
    }
    Bytes sig_blob, global_sig;
    if (!base64_decode(lines[1], sig_blob) || sig_blob.size() != 74) return fail("malformed signature");
    if (!base64_decode(lines[3], global_sig) || global_sig.size() != 64) return fail("malformed global signature");

    const bool prehashed = sig_blob[0] == 'E' && sig_blob[1] == 'D';
    const bool legacy = sig_blob[0] == 'E' && sig_blob[1] == 'd';
    if (!prehashed && !legacy) return fail("unknown signature algorithm");
    if (std::memcmp(sig_blob.data() + 2, key.key_id, 8) != 0) {
        return fail("signature was made with a different key (key id mismatch)");
    }
    const uint8_t* sig = sig_blob.data() + 10;

    bool ok;
    if (prehashed) {
        auto digest = blake2b512(reinterpret_cast<const uint8_t*>(message.data()), message.size());
        ok = ed25519_verify(sig, key.public_key, digest.data(), digest.size());
    } else {
        ok = ed25519_verify(sig, key.public_key, reinterpret_cast<const uint8_t*>(message.data()), message.size());
    }
    if (!ok) return fail("signature does not match the data (it was altered, or signed with another key)");

    // Everything after "trusted comment:" up to end of line, exactly one optional leading space.
    std::string comment = lines[2].substr(std::strlen("trusted comment:"));
    if (!comment.empty() && comment[0] == ' ') comment.erase(0, 1);
    Bytes global_msg(sig, sig + 64);
    global_msg.insert(global_msg.end(), comment.begin(), comment.end());
    if (!ed25519_verify(global_sig.data(), key.public_key, global_msg.data(), global_msg.size())) {
        return fail("trusted comment does not match its signature");
    }
    res.ok = true;
    res.trusted_comment = comment;
    return res;
}

}  // namespace jobeval
