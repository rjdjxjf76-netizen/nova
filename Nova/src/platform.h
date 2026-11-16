#pragma once
#include <string>
// Platform boundary: network, hashing and file storage go through here so the UI stays
// testable off-console.
enum class DlState { Idle, Running, Done, Failed };
struct DownloadStatus {
    DlState state = DlState::Idle;
    unsigned long long received = 0, total = 0;
    std::string message;   // error text or "VERIFIED"
    std::string path;      // final file path once Done
};
// Starts a download of `url` that must match `sha256`. Returns false (with `err`) if it
// can't start, including when URL or hash are missing.
bool platform_start_download(const char* name,const char* url,const char* sha256,std::string& err);
// Call once per frame; advances the download and fills `s`.
void platform_poll_download(DownloadStatus& s);
