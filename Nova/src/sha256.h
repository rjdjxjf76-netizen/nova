#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
// Streaming SHA-256 (FIPS 180-4). No dependencies.
class Sha256 {
public:
    Sha256();
    void update(const void* data, size_t len);
    std::string finalHex();           // lowercase hex; call once
private:
    void block(const uint8_t* p);
    uint32_t h_[8]; uint8_t buf_[64]; size_t fill_; uint64_t total_;
};
