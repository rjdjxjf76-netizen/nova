#pragma once
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include "sha256.h"

// Frame-stepped HTTP/1.0 downloader. No threads: call step() once per UI frame.
// - http:// only (no TLS here). Redirects, chunked encoding and non-200 replies are refused.
// - SHA-256 is computed while streaming; the file is only moved to its final name on a match.
// - A download without an expected hash is refused.
// All socket calls live in the `net` section of http_download.cpp so they can be swapped
// for sceNet* equivalents if the target SDK's POSIX sockets are unavailable.
class HttpDownload {
public:
    enum State { Idle, Running, Done, Failed };
    HttpDownload();
    ~HttpDownload();
    bool begin(const std::string& url, const std::string& outPath,
               const std::string& expectedSha256, std::string& err);
    State step();
    State state() const { return state_; }
    uint64_t received() const { return received_; }
    uint64_t total() const { return total_; }            // 0 if the server gave no length
    const std::string& message() const { return msg_; }
    const std::string& outPath() const { return out_; }
    static constexpr uint64_t kMaxBytes = 4ull*1024*1024*1024;
private:
    enum Phase { Connect, Send, Headers, Body };
    State fail(const char* why);
    State finish();
    bool consume(const char* d, size_t n);
    bool parseHeaders();
    bool writeBody(const char* d, size_t n);
    void cleanup(bool removePart);
    State state_; Phase phase_;
    int fd_; FILE* file_;
    std::string out_, part_, expected_, req_, hdr_, msg_;
    size_t sent_;
    uint64_t received_, total_;
    Sha256 sha_;
    std::chrono::steady_clock::time_point last_;
};
