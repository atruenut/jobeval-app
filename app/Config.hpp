#pragma once

namespace config {

// Where packs are published, and the key that signs the catalog. Developers can override
// both with JOBEVAL_CATALOG_URL and JOBEVAL_DEV_PUBKEY (the window title then says so).
inline constexpr const char* kCatalogUrl = "https://atruenut.github.io/jobeval-data/";
inline constexpr const char* kPublicKey = "RWTO7IvPGG06wlW/mdSpbzkrKXHcF9xgPI5AOWBeei+EJPZ/dQw16RrC";

}  // namespace config
