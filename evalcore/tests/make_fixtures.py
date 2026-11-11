#!/usr/bin/env python3
"""Regenerates evalcore/tests/fixtures/ (deterministic; needs the `cryptography` package).

Signs with a throwaway TEST key whose seed is in this file. It protects nothing and must
never be used for real packs. Output mimics what minisign 0.12 produces (prehashed "ED"
signatures plus a trusted-comment signature), so the C++ verifier is exercised against the
real format; CI additionally verifies the live published catalog.
"""
import base64
import hashlib
import json
import shutil
from pathlib import Path

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

OUT = Path(__file__).resolve().parent / "fixtures"
SEED = bytes(range(32))                      # TEST ONLY
KEY_ID = bytes.fromhex("a1b2c3d4e5f60718")   # TEST ONLY
priv = Ed25519PrivateKey.from_private_bytes(SEED)
pub = priv.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
b64 = lambda b: base64.b64encode(b).decode()


def minisig(message: bytes, comment: str, legacy: bool = False) -> str:
    if legacy:
        sig = priv.sign(message)
        alg = b"Ed"
    else:
        sig = priv.sign(hashlib.blake2b(message, digest_size=64).digest())
        alg = b"ED"
    global_sig = priv.sign(sig + comment.encode())
    return (
        "untrusted comment: signature from minisign secret key\n"
        + b64(alg + KEY_ID + sig) + "\n"
        + "trusted comment: " + comment + "\n"
        + b64(global_sig) + "\n"
    )


MINI = r'''{"schema":1,"id":"T-2026","kind":"country","region":{"country":"TT"},"valid":{"from":"2026-01-01","to":"2026-12-31"},"revision":1,"tier":"partial","currency":"TTD","sources":{"s":{"authority":"official","title":"test"}},"parameters":{"bpa":{"source":"s","value":13000},"mw":{"effective":[{"from":"2025-04-01","value":15.65,"source":"s"},{"from":"2026-04-01","value":15.90,"source":"s"}]}},"inputs":{"gross":{"type":"money"}},"rules":[{"id":"tax.base","expr":"max(0, gross - $bpa) * 0.1"},{"id":"floor","public":true,"expr":"$mw"}],"outputs":{"income_tax":["tax.base"]}}
'''


def child(revision: int, rate: str) -> str:
    return (
        '{"schema":1,"id":"T-XX-2026","kind":"subregion","extends":"T-2026","region":{"country":"TT","subregion":"XX"},'
        '"valid":{"from":"2026-01-01","to":"2026-12-31"},"revision":%d,"tier":"partial",'
        '"sources":{"s2":{"authority":"official","title":"child","retrieved":"2026-11-0%d"}},'
        '"parameters":{"xx.rate":{"source":"s2","value":%s}},'
        '"rules":[{"id":"xx.tax","expr":"gross * $xx.rate"}],"append":{"income_tax":["xx.tax"]}}\n'
    ) % (revision, revision, rate)


def build_site(name: str, generated: str, packs: list) -> None:
    root = OUT / name
    if root.exists():
        shutil.rmtree(root)
    (root / "packs").mkdir(parents=True)
    entries = []
    for pid, rev, text, kind, extends, sub in packs:
        data = text.encode()
        digest = hashlib.sha256(data).hexdigest()
        fname = f"packs/{pid}-r{rev}.{digest[:8]}.json"
        (root / fname).write_bytes(data)
        entries.append({
            "id": pid, "kind": kind, "extends": extends, "country": "TT", "subregion": sub,
            "labels": {"en": pid}, "tier": "partial", "revision": rev,
            "valid_from": "2026-01-01", "valid_to": "2026-12-31", "requires_engine": ">=1.0",
            "last_verified": "2026-11-0%d" % rev, "file": fname, "sha256": digest, "size": len(data),
        })
    index = json.dumps({"catalog": 1, "generated": generated, "packs": entries}, indent=2) + "\n"
    (root / "index.json").write_text(index)
    (root / "index.json.minisig").write_text(minisig(index.encode(), "generated:" + generated))


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "test.pub").write_text(b64(b"Ed" + KEY_ID + pub) + "\n")
    build_site("site_v1", "2026-11-01T00:00:00Z", [
        ("T-2026", 1, MINI, "country", None, None),
        ("T-XX-2026", 1, child(1, "0.10"), "subregion", "T-2026", "XX"),
    ])
    build_site("site_v2", "2026-11-02T00:00:00Z", [
        ("T-2026", 1, MINI, "country", None, None),
        ("T-XX-2026", 2, child(2, "0.12"), "subregion", "T-2026", "XX"),
    ])
    msg = b"hello, minisign\n"
    vec = OUT / "vectors"
    vec.mkdir(exist_ok=True)
    (vec / "message.txt").write_bytes(msg)
    (vec / "prehashed.minisig").write_text(minisig(msg, "timestamp:1 file:message.txt"))
    (vec / "legacy.minisig").write_text(minisig(msg, "timestamp:1 file:message.txt", legacy=True))
    # Known-answer hashes for the unit tests, computed with Python's hashlib.
    (vec / "hashes.json").write_text(json.dumps({
        "sha256_empty": hashlib.sha256(b"").hexdigest(),
        "sha256_abc": hashlib.sha256(b"abc").hexdigest(),
        "sha256_1000a": hashlib.sha256(b"a" * 1000).hexdigest(),
        "sha512_empty": hashlib.sha512(b"").hexdigest(),
        "sha512_abc": hashlib.sha512(b"abc").hexdigest(),
        "sha512_1000a": hashlib.sha512(b"a" * 1000).hexdigest(),
        "blake2b_abc": hashlib.blake2b(b"abc", digest_size=64).hexdigest(),
    }, indent=1) + "\n")


if __name__ == "__main__":
    main()
